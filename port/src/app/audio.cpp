#include "audio.h"

namespace pacman::app {

bool AudioOutput::open(int sampleRate) {
    close();
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = static_cast<WORD>(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    if (waveOutOpen(&device_, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        device_ = nullptr;
        return false;
    }
    rate_ = sampleRate;
    return true;
}

void AudioOutput::reap() {
    while (!queue_.empty() && (queue_.front()->header.dwFlags & WHDR_DONE)) {
        waveOutUnprepareHeader(device_, &queue_.front()->header, sizeof(WAVEHDR));
        queue_.pop_front();
    }
}

void AudioOutput::play(const core::Pcm& pcm) {
    if (!device_ || pcm.samples.empty() || pcm.sampleRate != rate_) return;
    reap();
    auto block = std::make_unique<Block>();
    block->data = pcm.samples;
    block->header.lpData = reinterpret_cast<LPSTR>(block->data.data());
    block->header.dwBufferLength = static_cast<DWORD>(block->data.size() * sizeof(std::int16_t));
    if (waveOutPrepareHeader(device_, &block->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) return;
    if (waveOutWrite(device_, &block->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(device_, &block->header, sizeof(WAVEHDR));
        return;
    }
    queue_.push_back(std::move(block));
}

bool AudioOutput::busy() {
    if (!device_) return false;
    reap();
    return !queue_.empty();
}

void AudioOutput::stop() {
    if (!device_) return;
    waveOutReset(device_);   // marks every queued block done
    reap();
}

void AudioOutput::close() {
    if (!device_) return;
    waveOutReset(device_);
    reap();
    waveOutClose(device_);
    device_ = nullptr;
}

}  // namespace pacman::app
