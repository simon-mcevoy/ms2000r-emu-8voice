// EIGHT-1 (2026-10-26): the 8-voice plugin's note sharing between its two machines (core/voice_split.h,
// docs/eight_voice.md).
#include "core/voice_split.h"
#include <cstdio>
using namespace MS2000;

static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { ++fails; std::printf("[SPLIT-FAIL] %s\n", what); } else std::printf("[SPLIT-OK]   %s\n", what); }

int main()
{
    {   // 8 held notes: 4 on each machine, alternating from A
        VoiceSplit s; int got[8];
        for (int i = 0; i < 8; ++i) got[i] = s.noteOn(0, 60 + i, 0.1 * i, true);
        check(got[0] == 0 && got[1] == 1 && got[2] == 0 && got[3] == 1 && got[6] == 0 && got[7] == 1, "8 notes alternate A, B, A, B ...");
        check(s.held(0) == 4 && s.held(1) == 4, "4 held on each machine");
        // the 9th goes where the oldest held note is (60 on A), so that is the one the machine steals
        check(s.noteOn(0, 70, 1.0, true) == 0, "9th note goes to the machine with the oldest note");
        check(s.noteOff(0, 61, 1.1) == 1, "a note-off follows its note-on (61 -> B)");
        check(s.noteOff(0, 99, 1.1) == -1, "an unknown note-off goes to both (-1)");
    }
    {   // fewer held wins over alternation; a retrigger stays on its machine
        VoiceSplit s;
        s.noteOn(0, 60, 0.0, true);            // A
        s.noteOn(0, 62, 0.1, true);            // B
        s.noteOn(0, 64, 0.2, true);            // A: A 2, B 1
        check(s.noteOn(0, 65, 0.3, true) == 1, "the machine with fewer held notes gets the note");
        check(s.noteOn(0, 64, 0.4, true) == 0 && s.held(0) == 2, "a held note retriggered stays on its machine, not counted twice");
    }
    {   // a program that wants one machine (mono, unison, arpeggiator): everything to A
        VoiceSplit s; bool allA = true;
        for (int i = 0; i < 6; ++i) allA = allA && s.noteOn(0, 60 + i, 0.1 * i, false) == 0;
        check(allA, "one-machine programs: every note to A");
    }
    {   // the sustain pedal keeps released notes busy until it is lifted
        VoiceSplit s;
        s.sustain(0, true, 0.0);
        for (int i = 0; i < 4; ++i) s.noteOn(0, 60 + i, 0.1 * i, true);
        for (int i = 0; i < 4; ++i) s.noteOff(0, 60 + i, 0.5);
        check(s.held(0) + s.held(1) == 4, "pedalled notes still count after their note-offs");
        s.sustain(0, false, 1.0);
        check(s.held(0) + s.held(1) == 0, "lifting the pedal releases them");
    }
    {   // a tie goes to the machine with fewer notes still releasing; old releases no longer count. The first note is a
        // one-machine note (it does not count for the alternation), so alternation alone would answer A both times.
        VoiceSplit s;
        s.noteOn(0, 60, 0.0, false);           // A
        s.noteOff(0, 60, 0.5);                 // A releasing
        check(s.noteOn(0, 62, 0.6, true) == 1, "a tie goes to the machine without a release tail");
        VoiceSplit t;
        t.noteOn(0, 60, 0.0, false); t.noteOff(0, 60, 0.1);  // A, released long ago
        check(t.noteOn(0, 62, 5.0, true) == 0 && t.noteOn(0, 64, 5.1, true) == 1, "releases older than 2 s are ignored");
    }
    {   // channels are separate notes; all notes off clears a channel
        VoiceSplit s;
        check(s.noteOn(0, 60, 0.0, true) == 0 && s.noteOn(1, 60, 0.1, true) == 1, "the same key on two channels is two notes");
        s.allOff(0, 0.2);
        check(s.held(0) == 0 && s.held(1) == 1, "all notes off clears only its channel");
        s.reset();
        check(s.held(0) == 0 && s.held(1) == 0 && s.noteOff(1, 60, 0.3) == -1, "reset forgets every note");
    }
    std::printf(fails ? "[SPLIT] %d FAIL\n" : "[SPLIT] all passed\n", fails);
    return fails ? 1 : 0;
}
