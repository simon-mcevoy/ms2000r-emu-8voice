# MS2000R 8-voice (EIGHT-1, 2026-10-26)

A second Audio Unit, "MS2000R 8-voice" (aumu Ms28 Msem), beside the 4-voice one. It runs two complete
MS2000R machines behind one panel and gives each note to one of them, so up to 8 notes sound with one timbre.
Nothing in the firmware or the DSP code changes; the 4-voice plugin is built and behaves as before.

## How

- **Two machines.** Machine A is the plugin's own (the editor shows it). Machine B runs in a private copy of the
  plugin binary (the emulator core holds one machine per loaded copy, as MULTI-1 found), loaded by A from
  `$TMPDIR/MS2000R_au_engines/engine_b_*` and driven through a small C interface the 8-voice binary exports
  (`companion_api.h`, `companion_engine.cpp`). B starts and stops with A, from A's flash, so both hold the same programs.
- **Everything but notes goes to both.** Pots, keys, host automation, AUDIO IN, the input stage and DAC options,
  and every MIDI message that is not a note (CC, pitch bend, program change, clock, SysEx, the Library and .syx
  import) reach A and B at the same machine time. Same firmware, same flash, same inputs: the two edit buffers stay
  equal. The timer compares them once a second and says so in the status line if they differ for 3 s
  (a program pad press puts both right again).
- **Notes are shared** (`voice_split.h`). A note-on goes to the machine with fewer held (or sustain-pedalled) notes;
  on a tie, the one with fewer notes released in the last 2 s; then alternately. A note still held or pedalled on one
  machine is retriggered there. With 8 held, a 9th goes to the machine whose oldest held note is the oldest, so
  that note is the one stolen, as on the unit. Note-offs follow their note-on; an unknown note-off goes to both.
- **When one machine is right, all notes go to A:** arpeggiator on (program byte 32 bit 7), or a timbre in use that is
  not Poly (timbre byte 1 bits 6-7: 0 Mono, 1 Poly, 2 Unison; Single and Vocoder use timbre 1, Split and Layer both).
  Read from A's edit buffer every block. Mono, unison and arpeggiated programs therefore behave exactly as on the unit.
- **Audio.** B renders the same 48 kHz frames as A on a real-time worker thread that joins the host's audio workgroup,
  while A renders on the host's thread; the two are summed before the resampler and the VOLUME pot. Without the
  worker (it failed to start) B renders after A on the host's thread.
- **MIDI OUT** is A's only. **Project state** is A's: B is rebuilt from it on every start.

## Building

`tools/install_mac.sh [ROM folder]` builds and installs both Audio Units (4-voice and 8-voice), copies the ROM folder to
`~/Documents/MS2000R` when one is given, and checks both with auval. On a fresh Mac:

    gh repo clone Tuth/ms2000r-emu ~/Code/ms2000r-emu -- -b macos-8-voice && ~/Code/ms2000r-emu/tools/install_mac.sh <ROM folder>

By hand:

    cmake --build build_vst.nosync --target MS2000R8_AU
    cp -R build_vst.nosync/AU_multi/MS2000R8.component ~/Library/Audio/Plug-Ins/Components/

`test_voice_split` (ctest VOICE_split) tests the note sharing.

## Tested (2026-10-26, M4 Max, macOS 27.0.1, offline AVAudioEngine renders at 48 kHz, 512-frame blocks)

- `auval -v aumu Ms28 Msem` passes.
- INIT Program (H16), notes 60-67 one every 0.25 s and held: the 4-voice plugin sounds 64-67 (60-63 36-48 dB down,
  stolen); the 8-voice sounds all eight within 0.3 dB. A 9th note: 60, the oldest, is stolen; 61-68 sound.
- Sustain pedal down, eight short notes: all eight keep sounding.
- Filter Cutoff turned down while the eight are held: A's and B's notes fall on one smooth curve (B follows the panel).
- Boost Bass (A04, Mono), Krazy Arpy 1 (C02, arpeggiator), and INIT Program with the ARP ON/OFF key pressed on the panel:
  the 8-voice output is sample-identical to the 4-voice output (every note on A).
- Three notes on four programs (INIT, Evolution, Turn Wheel, Mod3&4 Squad): the same level as the 4-voice within 0.05 dB.
- POWER off and on (the VOLUME pot's switch): both machines boot again and the next chord sounds all eight notes.
- The in-step check: with B's knob link cut on purpose it reports after 3 s; with the link, it stays quiet.
- Two 8-voice and one 4-voice instance in one process: 8, 8 and 4 notes sounding.
- Cost per 10.67 ms block after the boot, eight notes held: median 1.95 ms, max 2.40 ms (4-voice: 1.86 / 2.19 ms).
  Both plugins have two blocks of about 50 ms during the firmware's boot.

## Known limits

- Twice the CPU of the 4-voice plugin (spread over two cores).
- Oscillator phases differ from the 4-voice plugin's (they depend on which voice slot plays a note), as between any two
  notes on the unit: the waveform differs, the level and pitch content do not (tested above). The two machines boot
  together and get the same inputs, so their LFOs and motion sequences start together; not measured whether they
  stay exactly in phase over a long session.
- In Split or Layer with one timbre Mono or Unison, the program stays at 4 voices (all on A).
- Only the Audio Unit (macOS). The VST3 is unchanged.
