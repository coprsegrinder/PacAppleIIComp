// TASK-032-4 / 033-S: audio output through WASAPI shared mode, event driven (part of Windows; nothing to download). The first version used waveOut with small blocks; the device
// left gaps between the blocks (the 5 s jingle took 6.4 s), so the output is one continuous stream that a mixer thread fills. A sound is placed behind the nominal length of the
// sounds before it (so the original's spacing is kept) and the ring-out of the speaker (the part of a Pcm beyond `nominal`) is mixed over whatever follows instead of delaying it.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "../core/squarewave.h"

namespace pacman::app {

class AudioOutput {
public:
    ~AudioOutput() { close(); }
    bool open(int sampleRate = 44100);   // mono, 16 bit (the system converts); false if there is no audio device
    void close();
    bool ok() const noexcept { return running_.load(); }
    void play(const core::Pcm& pcm);     // starts behind the nominal length of what was played before; the ring-out overlaps; ignored if the device is not open
    bool busy();                         // the nominal length of something is still to be heard
    void stop();                         // drop everything not yet handed to the device (mute)

private:
    void run(int sampleRate);            // the mixer thread: sets up WASAPI, then keeps the device buffer full
    std::thread thread_;
    std::atomic<bool> running_{false};   // the device is open and the thread runs
    std::atomic<bool> quit_{false};
    int rate_ = 0;
    std::mutex mutex_;
    std::vector<std::int32_t> pending_;  // mix of the samples not yet handed to the device; pending_[readPos_] is at position base_
    std::size_t readPos_ = 0;
    std::int64_t base_ = 0;              // samples handed to the device so far
    std::int64_t buffered_ = 0;          // samples handed over but not yet played (the device buffer's fill)
    std::int64_t scheduledEnd_ = 0;      // end of the nominal length of the last sound
    int result_ = 0;                     // 0 = still starting, 1 = open, -1 = failed (set by the thread under the lock)
};

}  // namespace pacman::app
