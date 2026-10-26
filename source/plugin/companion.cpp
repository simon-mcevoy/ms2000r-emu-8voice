// companion.cpp - EIGHT-1 (2026-10-26): see companion.h and docs/eight_voice.md.
#include "companion.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace {
// The copy of this binary B runs in: one per copy of the plugin binary, kept for the life of the process and used by
// every instance that copy holds (one at a time - MULTI-1). Its file is in the AU loader's folder, named so that the
// loader's clean-up of an earlier process's copies takes it too (engine_*).
struct BImage { void* module = nullptr; std::string error; };
BImage& bImage() { static BImage i; return i; }

std::string ownPath()
{
    Dl_info info{};
    return dladdr(reinterpret_cast<const void*>(&ownPath), &info) && info.dli_fname ? std::string(info.dli_fname) : std::string();
}

std::string engineDir()
{
    const char* t = std::getenv("TMPDIR");
    std::string d = t && *t ? t : "/tmp/";
    if (d.back() != '/') d += '/';
    d += "MS2000R_au_engines";
    std::error_code ec; std::filesystem::create_directories(d, ec);
    return d;
}

void* loadBImage(std::string& error)
{
    auto& img = bImage();
    if (img.module || !img.error.empty()) { error = img.error; return img.module; }
    const std::string self = ownPath();
    std::string path = engineDir() + "/engine_b_XXXXXX";
    const int fd = self.empty() ? -1 : mkstemp(path.data());
    std::error_code ec;
    if (fd < 0) img.error = "no file for the second machine's engine";
    else {
        close(fd);
        if (!std::filesystem::copy_file(self, path, std::filesystem::copy_options::overwrite_existing, ec))
            img.error = "copying the engine failed: " + ec.message();
        else if (!(img.module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))) {
            const char* why = dlerror();
            img.error = std::string("loading the engine copy failed: ") + (why ? why : "?");
        }
    }
    error = img.error;
    return img.module;
}
} // namespace

Ms2kCompanion::Ms2kCompanion() : juce::Thread("MS2000R machine B") {}

Ms2kCompanion::~Ms2kCompanion()
{
    stop();
    signalThreadShouldExit();
    m_go.signal();
    stopThread(2000);
}

bool Ms2kCompanion::load()
{
    if (m_api.start) return true;
    std::string err;
    void* m = loadBImage(err);
    if (!m) { m_error = err; return false; }
    Api a;
    a.start = reinterpret_cast<Ms2kBStart>(dlsym(m, "ms2k_b_start"));
    a.stop = reinterpret_cast<Ms2kBStop>(dlsym(m, "ms2k_b_stop"));
    a.render = reinterpret_cast<Ms2kBRender>(dlsym(m, "ms2k_b_render"));
    a.midi = reinterpret_cast<Ms2kBMidi>(dlsym(m, "ms2k_b_midi"));
    a.knob = reinterpret_cast<Ms2kBKnob>(dlsym(m, "ms2k_b_knob"));
    a.key = reinterpret_cast<Ms2kBKey>(dlsym(m, "ms2k_b_key"));
    a.stage = reinterpret_cast<Ms2kBStage>(dlsym(m, "ms2k_b_stage"));
    a.audioIn = reinterpret_cast<Ms2kBAudioIn>(dlsym(m, "ms2k_b_audio_in"));
    a.peek = reinterpret_cast<Ms2kBPeek>(dlsym(m, "ms2k_b_peek"));
    if (!a.start || !a.stop || !a.render || !a.midi || !a.knob || !a.key || !a.stage || !a.audioIn || !a.peek) {
        m_error = "the engine copy lacks the second machine's interface";
        return false;
    }
    m_api = a;
    return true;
}

bool Ms2kCompanion::start(const MS2000::FlashROM& flash, bool dspThread)
{
    stop();
    m_error = {};
    if (!load()) return false;
    if (!m_api.start(flash.data(), flash.size(), flash.stateMask(), dspThread ? 1 : 0)) {
        m_error = "the second machine did not start";
        return false;
    }
    m_running.store(true, std::memory_order_release);
    // the worker: a real-time thread with the host's block period; without it B renders on the host's thread
    if (!isThreadRunning() && !startRealtimeThread(juce::Thread::RealtimeOptions{}.withPeriodMs(m_periodMs)))
        startThread(juce::Thread::Priority::highest);
    return true;
}

void Ms2kCompanion::stop()
{
    if (!m_running.exchange(false)) return;
    if (m_posted) { while (m_busy.load(std::memory_order_acquire)) m_done.wait(1.0); m_posted = false; }
    m_api.stop();
}

void Ms2kCompanion::prepare(size_t maxFrames, double periodMs)
{
    m_l.assign(maxFrames, 0.0f); m_r.assign(maxFrames, 0.0f);
    m_events.reserve(512);
    if (periodMs > 0.0) m_periodMs = periodMs;
}

void Ms2kCompanion::beginRender(uint32_t frames, const MS2000::Ms2kRunner::MidiEv* events, size_t n)
{
    if (!running()) return;
    if (m_l.size() < frames) { m_l.resize(frames); m_r.resize(frames); }
    m_events.assign(events, events + n);
    m_frames = frames;
    if (isThreadRunning()) {
        m_busy.store(true, std::memory_order_release);
        m_posted = true;
        m_go.signal();
    } else {
        m_api.render(m_l.data(), m_r.data(), frames, m_events.data(), m_events.size());
    }
}

void Ms2kCompanion::endRender(float* left, float* right, uint32_t frames)
{
    if (!running()) return;
    if (m_posted) {
        // A and B take about as long, so B is usually done: a few yields first, then 1 ms waits
        for (int i = 0; i < 64 && m_busy.load(std::memory_order_acquire); ++i) std::this_thread::yield();
        while (m_busy.load(std::memory_order_acquire)) m_done.wait(1.0);
        m_posted = false;
    }
    const uint32_t n = juce::jmin(frames, m_frames);
    for (uint32_t i = 0; i < n; ++i) { left[i] += m_l[i]; right[i] += m_r[i]; }
}

void Ms2kCompanion::setWorkgroup(const juce::AudioWorkgroup& wg)
{
    const juce::SpinLock::ScopedLockType l(m_wgLock);
    m_wg = wg;
    m_wgGen.fetch_add(1, std::memory_order_release);
}

void Ms2kCompanion::run()
{
    while (!threadShouldExit()) {
        if (!m_go.wait(100.0)) continue;
        if (threadShouldExit()) break;
        if (const int g = m_wgGen.load(std::memory_order_acquire); g != m_wgJoined) {
            m_wgJoined = g;
            m_wgToken.reset();
            juce::AudioWorkgroup wg;
            { const juce::SpinLock::ScopedLockType l(m_wgLock); wg = m_wg; }
            if (wg) wg.join(m_wgToken);
        }
        if (m_busy.load(std::memory_order_acquire)) {
            m_api.render(m_l.data(), m_r.data(), m_frames, m_events.data(), m_events.size());
            m_busy.store(false, std::memory_order_release);
            m_done.signal();
        }
    }
    m_wgToken.reset();
}
