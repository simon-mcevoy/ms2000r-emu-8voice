// companion.h - EIGHT-1 (2026-10-26): machine B of the 8-voice plugin, as machine A's processor sees it
// (docs/eight_voice.md). B runs in a private copy of this binary (companion_engine.cpp, behind companion_api.h) and
// renders each block on a real-time worker thread, joined to the host's audio workgroup, while A renders on the
// host's thread. Without the worker B renders on the host's thread, after A.
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "companion_api.h"
#include "core/flash_rom.h"

class Ms2kCompanion : private juce::Thread
{
public:
    Ms2kCompanion();
    ~Ms2kCompanion() override;

    // power B on from A's flash. Called where A powers on, under the processor's machine lock (no block renders).
    bool start(const MS2000::FlashROM& flash, bool dspThread);
    void stop();
    bool running() const { return m_running.load(std::memory_order_acquire); }
    juce::String error() const { return m_error; }

    void prepare(size_t maxFrames, double periodMs);     // prepareToPlay
    // audio thread, each block: B renders its frames; endRender waits for them and adds them to A's
    void beginRender(uint32_t frames, const MS2000::Ms2kRunner::MidiEv* events, size_t n);
    void endRender(float* left, float* right, uint32_t frames);

    // B's inputs, any thread (the machine locks MIDI IN; the panel is atomic)
    void midi(const uint8_t* d, size_t n)                    { if (running()) m_api.midi(d, n); }
    void knob(unsigned mux, unsigned x, uint16_t v)          { if (running()) m_api.knob(mux, x, v); }
    void key(unsigned col, unsigned row, bool down)          { if (running()) m_api.key(col, row, down ? 1 : 0); }
    void stage(float in1, float in2, bool mic2, bool dac20)  { if (running()) m_api.stage(in1, in2, mic2 ? 1 : 0, dac20 ? 1 : 0); }
    void audioIn(int input, const float* p, uint32_t n)      { if (running()) m_api.audioIn(input, p, n); }
    bool peek(uint32_t addr, uint8_t* out, size_t n)         { if (!running()) return false; m_api.peek(addr, out, n); return true; }

    void setWorkgroup(const juce::AudioWorkgroup& wg);       // AudioProcessor::audioWorkgroupContextChanged

private:
    struct Api {
        Ms2kBStart start = nullptr; Ms2kBStop stop = nullptr; Ms2kBRender render = nullptr; Ms2kBMidi midi = nullptr;
        Ms2kBKnob knob = nullptr; Ms2kBKey key = nullptr; Ms2kBStage stage = nullptr; Ms2kBAudioIn audioIn = nullptr;
        Ms2kBPeek peek = nullptr;
    };
    bool load();
    void run() override;

    Api m_api;
    std::atomic<bool> m_running{ false };
    juce::String m_error;
    double m_periodMs = 10.0;

    // one block's job for the worker
    std::vector<float> m_l, m_r;
    std::vector<MS2000::Ms2kRunner::MidiEv> m_events;
    uint32_t m_frames = 0;
    std::atomic<bool> m_busy{ false };
    bool m_posted = false;
    juce::WaitableEvent m_go, m_done;

    // the host's workgroup, joined on the worker when it changes
    juce::SpinLock m_wgLock;
    juce::AudioWorkgroup m_wg;
    std::atomic<int> m_wgGen{ 0 };
    int m_wgJoined = 0;
    juce::WorkgroupToken m_wgToken;
};
