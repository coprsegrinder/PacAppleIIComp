// TASK-032-4: audio output through winmm waveOut (part of Windows; nothing to download). Buffers are queued and played in order, without blocking the caller.
#pragma once

#include <windows.h>
#include <mmsystem.h>

#include <deque>
#include <memory>
#include <vector>

#include "../core/squarewave.h"

namespace pacman::app {

class AudioOutput {
public:
    ~AudioOutput() { close(); }
    bool open(int sampleRate = 44100);   // mono, 16 bit; false if there is no audio device
    void close();
    bool ok() const noexcept { return device_ != nullptr; }
    void play(const core::Pcm& pcm);     // queued behind what is still playing; ignored if the device is not open
    bool busy();                         // something still queued or playing
    void stop();                         // drop everything queued (mute)

private:
    struct Block {
        WAVEHDR header{};
        std::vector<std::int16_t> data;
    };
    void reap();                         // free the blocks the device has finished
    HWAVEOUT device_ = nullptr;
    int rate_ = 0;
    std::deque<std::unique_ptr<Block>> queue_;
};

}  // namespace pacman::app
