// companion_engine.cpp - EIGHT-1 (2026-10-26): machine B of the 8-voice plugin (docs/eight_voice.md, companion_api.h).
// Built into the 8-voice binary; only the private copy that companion.cpp loads ever calls it. The machine is set up as
// Ms2kProcessor::startMachine sets up machine A, from A's flash.
#include "companion_api.h"
#include "core/flash_rom.h"
#include <memory>

#define MS2K_B_EXPORT extern "C" __attribute__((visibility("default")))

namespace {
std::unique_ptr<MS2000::Ms2kRunner> g_b;
}

MS2K_B_EXPORT void ms2k_b_stop()
{
    if (!g_b) return;
    g_b->getEmulator().setMidiOutSink(nullptr);
    g_b->stop();
    g_b.reset();
}

MS2K_B_EXPORT int ms2k_b_start(const uint8_t* flash, size_t size, uint32_t mask, int dspThread)
{
    ms2k_b_stop();
    MS2000::Ms2kConfig cfg;
    cfg.romPath = "flash.bin";
    cfg.cpuCyclesPerTick = 20000;
    cfg.sampleRate = 48000;
    cfg.quietBoot = true;
    cfg.useMmcss = false;
    cfg.audio.enabled = false;
    cfg.dspThread = dspThread != 0;
    cfg.flashStateSave = false;
    cfg.flashStateLoad = false;                              // A's flash, whatever it came from
    if (mask && flash && size == MS2000::FlashROM::FLASH_SIZE) { cfg.flashStateMask = mask; cfg.flashStateImage.assign(flash, flash + size); }
    MS2000::Ms2kGuiHooks hooks;
    hooks.logFn = [](const char*) {};
    g_b = std::make_unique<MS2000::Ms2kRunner>(cfg, hooks);
    if (!g_b->init() || !g_b->startManual()) { g_b.reset(); return 0; }
    return 1;
}

MS2K_B_EXPORT void ms2k_b_render(float* left, float* right, uint32_t frames, const MS2000::Ms2kRunner::MidiEv* events, size_t n)
{
    if (g_b) g_b->render(left, right, frames, events, n);
    else for (uint32_t i = 0; i < frames; ++i) left[i] = right[i] = 0.0f;
}

MS2K_B_EXPORT void ms2k_b_midi(const uint8_t* data, size_t n) { if (g_b) g_b->sendMIDIData(data, n); }
MS2K_B_EXPORT void ms2k_b_knob(unsigned mux, unsigned x, uint16_t value) { if (g_b) g_b->getEmulator().setPanelKnob(mux, x, value); }
MS2K_B_EXPORT void ms2k_b_key(unsigned col, unsigned row, int down) { if (g_b) g_b->getEmulator().setPanelSwitch(col, row, down != 0); }

MS2K_B_EXPORT void ms2k_b_stage(float in1, float in2, int mic2, int dac20)
{
    if (auto* d = g_b ? g_b->getEmulator().dsp() : nullptr) { d->setInputStage(in1, in2, mic2 != 0); d->setDac20(dac20 != 0); }
}

MS2K_B_EXPORT void ms2k_b_audio_in(int input, const float* mono, uint32_t frames)
{
    if (auto* d = g_b ? g_b->getEmulator().dsp() : nullptr) d->pushAudioIn(input, mono, frames);
}

MS2K_B_EXPORT void ms2k_b_peek(uint32_t addr, uint8_t* out, size_t n)
{
    for (size_t i = 0; i < n; ++i) out[i] = g_b ? g_b->getEmulator().peekExternal(addr + uint32_t(i)) : 0;
}
