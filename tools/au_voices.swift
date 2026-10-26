// EIGHT-1 (2026-10-26): offline AU test for docs/eight_voice.md. Build: swiftc -O au_voices.swift -o au_voices
// Pass ABSOLUTE output paths: the plugin makes the MS2000 folder the process's working directory.
// Offline test of the MS2000R AUs: which held notes are still sounding.
// usage: au_voices <Ms2r|Ms28> <chord8|chord9|pedal|mono|arp|arpkey|knob|power|chord3> [absolute raw out path] [program]
// Boots the machine, selects a program by MIDI program change, plays notes, then measures each note's fundamental
// (Goertzel) over a window while they should all be held. Prints dB per note and the render cost per block.
import AVFoundation

let args = CommandLine.arguments
let sub = args.count > 1 ? args[1] : "Ms28"
let test = args.count > 2 ? args[2] : "chord8"
let outPath = args.count > 3 ? args[3] : nil
let rate = 48000.0, block: AVAudioFrameCount = 512

func fourcc(_ s: String) -> UInt32 { s.utf8.reduce(0) { ($0 << 8) | UInt32($1) } }
var desc = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice, componentSubType: fourcc(sub),
                                     componentManufacturer: fourcc("Msem"), componentFlags: 0, componentFlagsMask: 0)
let sem = DispatchSemaphore(value: 0)
var unit: AVAudioUnit?
AVAudioUnit.instantiate(with: desc, options: []) { u, err in if let err { print("instantiate: \(err)") }; unit = u; sem.signal() }
sem.wait()
guard let synth = unit as? AVAudioUnitMIDIInstrument else { print("no instrument"); exit(1) }

let engine = AVAudioEngine()
let fmt = AVAudioFormat(standardFormatWithSampleRate: rate, channels: 2)!
engine.attach(synth)
engine.connect(synth, to: engine.mainMixerNode, format: fmt)
try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: block)
try engine.start()

// the test: events at seconds, the notes to measure, the window
var events: [(Double, () -> Void)] = []
var notes: [UInt8] = []
var window = (6.5, 7.5), total = 9.0
func on(_ t: Double, _ n: UInt8) { events.append((t, { synth.startNote(n, withVelocity: 100, onChannel: 0) })) }
func off(_ t: Double, _ n: UInt8) { events.append((t, { synth.stopNote(n, onChannel: 0) })) }
func program(_ t: Double, _ p: UInt8) { events.append((t, { synth.sendProgramChange(p, onChannel: 0) })) }
func cc(_ t: Double, _ c: UInt8, _ v: UInt8) { events.append((t, { synth.sendController(c, withValue: v, onChannel: 0) })) }
func param(_ t: Double, _ name: String, _ v: AUValue) {   // a host parameter (a panel key: 1 = held)
    events.append((t, { guard let p = synth.auAudioUnit.parameterTree?.allParameters.first(where: { $0.displayName == name }) else { print("no parameter \(name)"); return }; p.setValue(v, originator: nil) }))
}
switch test {
case "chord8":                        // INIT Program (H16): 8 notes, one every 0.25 s, all held
    program(3.5, 127); notes = Array(60...67)
    for (i, n) in notes.enumerated() { on(4.0 + 0.25 * Double(i), n); off(8.0, n) }
case "chord9":                        // a 9th note: the oldest (60) should be the one stolen
    program(3.5, 127); notes = Array(60...68)
    for (i, n) in notes.enumerated() { on(4.0 + 0.25 * Double(i), n); off(8.0, n) }
case "pedal":                         // sustain pedal down, 8 short notes: all should keep sounding
    program(3.5, 127); cc(3.8, 64, 127); notes = Array(60...67)
    for (i, n) in notes.enumerated() { on(4.0 + 0.25 * Double(i), n); off(4.1 + 0.25 * Double(i), n) }
    cc(8.0, 64, 0)
case "mono":                          // A04 Boost Bass is Mono: the second note must replace the first, not add to it
    program(3.5, 3); notes = [48, 55]
    on(4.0, 48); on(4.5, 55); off(8.0, 48); off(8.0, 55); window = (5.5, 6.5)
case "arp":                           // C02 Krazy Arpy 1: the arpeggiator is on in the program
    program(3.5, 33); notes = [60, 64, 67]
    for n in notes { on(4.0, n); off(8.0, n) }
case "arpkey":                        // INIT Program, the ARP ON/OFF key pressed on the panel, then a chord
    program(3.5, 127); param(3.8, "Arp On/Off key", 1); param(4.1, "Arp On/Off key", 0); notes = [60, 64, 67]
    for n in notes { on(4.5, n); off(8.0, n) }
case "knob":                          // chord8, then the filter cutoff turned down: both machines must follow
    program(3.5, 127); notes = Array(60...67)
    for (i, n) in notes.enumerated() { on(4.0 + 0.25 * Double(i), n); off(8.0, n) }
    param(6.2, "Filter Cutoff", 0.25)
    // 4.5 s of real time for the plugin's once-a-second in-step check (its 10 Hz timer runs on wall time)
    events.append((7.6, { _ = CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 4.5, false) }))
case "power":                         // POWER off and on (the VOLUME pot's switch): both machines reboot, then chord8
    param(5.0, "Power / Volume", 0); param(5.5, "Power / Volume", 1)
    // the power switch is polled by the plugin's 10 Hz timer (wall time): give it real time after each move
    for t in [5.05, 5.55] { events.append((t, { _ = CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 0.4, false) })) }
    program(9.5, 127); notes = Array(60...67); window = (12.5, 13.5); total = 14.0
    for (i, n) in notes.enumerated() { on(10.0 + 0.25 * Double(i), n); off(14.0, n) }
case "chord3":                        // 3 notes held on the program in args[4]: fits in 4 voices
    program(3.5, UInt8(args.count > 4 ? Int(args[4])! : 127)); notes = [60, 64, 67]
    for n in notes { on(4.0, n); off(8.0, n) }; window = (5.0, 7.5)
default: print("unknown test"); exit(1)
}
events.sort { $0.0 < $1.0 }

let buf = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: block)!
var raw: [Float] = []                 // the whole left channel, written raw to outPath for an A/B between the two AUs
var captured: [Float] = []
var pos = 0, ei = 0
var blockMs: [Double] = []
while Double(pos) < total * rate {
    let t = Double(pos) / rate
    while ei < events.count && events[ei].0 <= t { events[ei].1(); ei += 1 }
    let t0 = DispatchTime.now().uptimeNanoseconds
    let st = try engine.renderOffline(block, to: buf)
    blockMs.append(Double(DispatchTime.now().uptimeNanoseconds - t0) / 1e6)
    guard st == .success else { print("render status \(st.rawValue)"); exit(1) }
    raw.append(contentsOf: UnsafeBufferPointer(start: buf.floatChannelData![0], count: Int(buf.frameLength)))
    if t >= window.0 && t < window.1 { captured.append(contentsOf: UnsafeBufferPointer(start: buf.floatChannelData![0], count: Int(buf.frameLength))) }
    pos += Int(buf.frameLength)
    CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 0, false)   // the plugin's timer (status, in-step check)
}

func goertzel(_ x: [Float], _ f: Double) -> Double {
    let w = 2 * Double.pi * f / rate, c = 2 * cos(w)
    var s1 = 0.0, s2 = 0.0
    for v in x { let s0 = Double(v) + c * s1 - s2; s2 = s1; s1 = s0 }
    return (s1 * s1 + s2 * s2 - c * s1 * s2).squareRoot() / Double(x.count)
}
let mags = notes.map { goertzel(captured, 440 * pow(2, (Double($0) - 69) / 12)) }
let peak = mags.max() ?? 1
print("\(sub) \(test): level of each note's fundamental, \(window.0)-\(window.1) s (dB below the loudest)")
for (n, m) in zip(notes, mags) { print(String(format: "  note %3d  %6.1f dB  %@", n, 20 * log10(max(m, 1e-12) / peak), 20 * log10(max(m, 1e-12) / peak) > -20 ? "sounding" : "-")) }
let sorted = blockMs.dropFirst(10).sorted()
let slow = blockMs.enumerated().filter { $0.element > Double(block) / rate * 1000 }.map { String(format: "%.2fs:%.0fms", Double($0.offset) * Double(block) / rate, $0.element) }
print("  blocks over budget: \(slow.count) - \(slow.prefix(8).joined(separator: " "))")
let after = blockMs.enumerated().filter { Double($0.offset) * Double(block) / rate > 4.0 }.map { $0.element }.sorted()
print(String(format: "  after 4 s: median %.2f ms, max %.2f ms", after[after.count / 2], after.last ?? 0))
print(String(format: "  render per %d-frame block (%.2f ms of audio): median %.2f ms, max %.2f ms", block, Double(block) / rate * 1000, sorted[sorted.count / 2], sorted.last ?? 0))
if let outPath { raw.withUnsafeBufferPointer { FileManager.default.createFile(atPath: outPath, contents: Data(buffer: $0)) } }
let rms = (captured.reduce(0.0) { $0 + Double($1 * $1) } / Double(max(captured.count, 1))).squareRoot()
print(String(format: "  level over the window: %.2f dBFS RMS", 20 * log10(max(rms, 1e-12))))
