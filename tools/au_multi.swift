// EIGHT-1 (2026-10-26): offline AU test for docs/eight_voice.md. Build: swiftc -O au_multi.swift -o au_multi
// Pass ABSOLUTE output paths: the plugin makes the MS2000 folder the process's working directory.
// Several MS2000R instances in one process, each in its own offline engine, rendered block by block in turn.
// usage: multi <subtype> <subtype> ...   Each plays INIT Program with notes 60..67 (one every 0.25 s, all held);
// prints which notes each instance still sounds at 6.5-7.5 s.
import AVFoundation

setvbuf(stdout, nil, _IONBF, 0)
let subs = Array(CommandLine.arguments.dropFirst())
let rate = 48000.0, block: AVAudioFrameCount = 512
func fourcc(_ s: String) -> UInt32 { s.utf8.reduce(0) { ($0 << 8) | UInt32($1) } }

struct Inst { let sub: String; let engine: AVAudioEngine; let synth: AVAudioUnitMIDIInstrument; let buf: AVAudioPCMBuffer; var captured: [Float] }
var insts: [Inst] = []
for sub in subs {
    var desc = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice, componentSubType: fourcc(sub),
                                         componentManufacturer: fourcc("Msem"), componentFlags: 0, componentFlagsMask: 0)
    let sem = DispatchSemaphore(value: 0); var unit: AVAudioUnit?
    AVAudioUnit.instantiate(with: desc, options: []) { u, _ in unit = u; sem.signal() }
    sem.wait()
    guard let synth = unit as? AVAudioUnitMIDIInstrument else { print("no instrument \(sub)"); exit(1) }
    let engine = AVAudioEngine(), fmt = AVAudioFormat(standardFormatWithSampleRate: rate, channels: 2)!
    engine.attach(synth); engine.connect(synth, to: engine.mainMixerNode, format: fmt)
    try engine.enableManualRenderingMode(.offline, format: fmt, maximumFrameCount: block)
    try engine.start()
    insts.append(Inst(sub: sub, engine: engine, synth: synth, buf: AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: block)!, captured: []))
}
let notes = Array(UInt8(60)...UInt8(67))
var pos = 0
while Double(pos) < 8.5 * rate {
    let t0 = Double(pos) / rate, t1 = Double(pos + Int(block)) / rate
    for i in insts.indices {
        let s = insts[i].synth
        if t0 <= 3.5 && 3.5 < t1 { s.sendProgramChange(127, onChannel: 0) }
        for (k, n) in notes.enumerated() { let tn = 4.0 + 0.25 * Double(k); if t0 <= tn && tn < t1 { s.startNote(n, withVelocity: 100, onChannel: 0) } }
        _ = try insts[i].engine.renderOffline(block, to: insts[i].buf)
        if t0 >= 6.5 && t0 < 7.5 { insts[i].captured.append(contentsOf: UnsafeBufferPointer(start: insts[i].buf.floatChannelData![0], count: Int(block))) }
    }
    pos += Int(block)
    CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 0, false)
}
func goertzel(_ x: [Float], _ f: Double) -> Double {
    let w = 2 * Double.pi * f / rate, c = 2 * cos(w); var s1 = 0.0, s2 = 0.0
    for v in x { let s0 = Double(v) + c * s1 - s2; s2 = s1; s1 = s0 }
    return (s1 * s1 + s2 * s2 - c * s1 * s2).squareRoot() / Double(max(x.count, 1))
}
for (i, inst) in insts.enumerated() {
    let m = notes.map { goertzel(inst.captured, 440 * pow(2, (Double($0) - 69) / 12)) }
    let peak = max(m.max() ?? 0, 1e-12)
    let sounding = zip(notes, m).filter { 20 * log10(max($0.1, 1e-12) / peak) > -20 }.map { String($0.0) }
    print("instance \(i) (\(inst.sub)): \(sounding.count) of 8 notes sounding: \(sounding.joined(separator: " "))")
}
// close the plugins as a host does before it quits: stop each engine, detach its instrument, release it all
for inst in insts { inst.engine.stop(); inst.engine.detach(inst.synth) }
insts.removeAll()
_ = CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 0.5, false)
print("done, exiting")
