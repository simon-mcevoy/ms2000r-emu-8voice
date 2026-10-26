// voice_split.h - EIGHT-1 (2026-10-26): which of the 8-voice plugin's two machines plays a note (docs/eight_voice.md).
//
// Each machine has the unit's 4 voices. A note-on goes to the machine with fewer held (or sustain-pedalled) notes; on a
// tie to the one with fewer notes released in the last kTail seconds; then alternately. A note still held or pedalled
// is retriggered on its own machine. With both machines full, the machine holding the oldest note gets it - that note
// is the one the machine steals, as the unit steals its oldest. A one-machine program (two = false: mono, unison,
// arpeggiator) sends everything to A and does not count for the alternation.
// Fixed storage, no allocation: called on the audio thread.
#pragma once
#include <cstdint>

namespace MS2000 {

class VoiceSplit {
public:
    static constexpr int kVoices = 4;          // per machine (the unit's polyphony)
    static constexpr double kTail = 2.0;       // s a released note still counts as a tail

    // 0 = machine A, 1 = machine B
    int noteOn(int ch, int key, double t, bool two)
    {
        Note* n = find(ch, key);
        if (n && (n->held || n->pedal)) { n->held = true; n->pedal = false; return n->machine; }
        const int m = two ? choose(t) : 0;
        if (two) m_last = m;
        if (!n) n = slot();
        *n = Note{ true, uint8_t(ch & 15), uint8_t(key & 127), uint8_t(m), true, false, t, 0.0 };
        return m;
    }

    // the machine the note's note-on went to; -1 = not known (send it to both)
    int noteOff(int ch, int key, double t)
    {
        Note* n = find(ch, key);
        if (!n) return -1;
        if (n->held) {
            n->held = false;
            if (m_pedalDown[ch & 15]) n->pedal = true; else n->off = t;
        }
        return n->machine;
    }

    void sustain(int ch, bool down, double t)
    {
        m_pedalDown[ch & 15] = down;
        if (down) return;
        for (auto& n : m_notes)
            if (n.used && n.ch == (ch & 15) && n.pedal) { n.pedal = false; n.off = t; }
    }

    void allOff(int ch, double t)                // CC 120 / 123
    {
        for (auto& n : m_notes)
            if (n.used && n.ch == (ch & 15) && (n.held || n.pedal)) { n.held = n.pedal = false; n.off = t; }
    }

    void reset()
    {
        for (auto& n : m_notes) n = Note{};
        for (auto& p : m_pedalDown) p = false;
        m_last = -1;
    }

    int held(int m) const                        // held or pedalled notes on machine m
    {
        int k = 0;
        for (const auto& n : m_notes) if (n.used && n.machine == m && (n.held || n.pedal)) ++k;
        return k;
    }

private:
    struct Note {
        bool used = false;
        uint8_t ch = 0, key = 0, machine = 0;
        bool held = false, pedal = false;
        double on = 0.0, off = 0.0;
    };
    static constexpr int kMax = 64;
    Note m_notes[kMax];
    bool m_pedalDown[16] = {};
    int m_last = -1;                             // the machine the last shared note went to

    Note* find(int ch, int key)
    {
        for (auto& n : m_notes) if (n.used && n.ch == (ch & 15) && n.key == (key & 127)) return &n;
        return nullptr;
    }

    // a free entry, else the longest-released one, else the oldest
    Note* slot()
    {
        Note* best = nullptr;
        for (auto& n : m_notes) {
            if (!n.used) return &n;
            const bool rel = !n.held && !n.pedal;
            if (!best) { best = &n; continue; }
            const bool bestRel = !best->held && !best->pedal;
            if (rel != bestRel) { if (rel) best = &n; continue; }
            if ((rel ? n.off : n.on) < (rel ? best->off : best->on)) best = &n;
        }
        return best;
    }

    int choose(double t) const
    {
        int busy[2] = {}, tails[2] = {};
        double oldest[2] = { 1e300, 1e300 };
        for (const auto& n : m_notes) {
            if (!n.used) continue;
            if (n.held || n.pedal) { ++busy[n.machine]; if (n.on < oldest[n.machine]) oldest[n.machine] = n.on; }
            else if (t - n.off < kTail) ++tails[n.machine];
        }
        if (busy[0] >= kVoices && busy[1] >= kVoices) return oldest[0] <= oldest[1] ? 0 : 1;
        if (busy[0] != busy[1]) return busy[0] < busy[1] ? 0 : 1;
        if (tails[0] != tails[1]) return tails[0] < tails[1] ? 0 : 1;
        return m_last == 0 ? 1 : 0;
    }
};

} // namespace MS2000
