// companion_api.h - EIGHT-1 (2026-10-26): the C interface between the 8-voice plugin and its second machine
// (docs/eight_voice.md). The plugin's processor runs machine A; it loads a private copy of its own binary
// (companion.cpp) and drives machine B there through these exports (companion_engine.cpp). The copy is a separate
// image, so B has its own emulator globals - the reason MULTI-1 gives every instance its own copy too.
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/ms2000_runner.h"

extern "C" {
// power on from this flash (FlashROM::FLASH_SIZE bytes; mask = the sectors that are not factory any more); 1 = running.
// The process's working directory is the MS2000 folder already (machine A set it).
typedef int  (*Ms2kBStart)(const uint8_t* flash, size_t size, uint32_t mask, int dspThread);
typedef void (*Ms2kBStop)();
// the next frames at 48 kHz, with MIDI IN events at their frame (Ms2kRunner::render)
typedef void (*Ms2kBRender)(float* left, float* right, uint32_t frames, const MS2000::Ms2kRunner::MidiEv* events, size_t n);
// the inputs; thread-safe in the machine (MIDI IN is locked, the panel is atomic)
typedef void (*Ms2kBMidi)(const uint8_t* data, size_t n);
typedef void (*Ms2kBKnob)(unsigned mux, unsigned x, uint16_t value);
typedef void (*Ms2kBKey)(unsigned col, unsigned row, int down);
typedef void (*Ms2kBStage)(float in1, float in2, int mic2, int dac20);
typedef void (*Ms2kBAudioIn)(int input, const float* mono, uint32_t frames);
// host-side read of the DRAM image (the edit buffer check)
typedef void (*Ms2kBPeek)(uint32_t addr, uint8_t* out, size_t n);
}
