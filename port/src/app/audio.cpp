#include "audio.h"

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <chrono>

namespace pacman::app {

namespace {
template <class T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}
}  // namespace

bool AudioOutput::open(int sampleRate) {
    close();
    rate_ = sampleRate;
    base_ = 0;
    buffered_ = 0;
    scheduledEnd_ = 0;
    readPos_ = 0;
    pending_.clear();
    result_ = 0;
    quit_ = false;
    thread_ = std::thread([this, sampleRate] { run(sampleRate); });
    for (int i = 0; i < 500; ++i) {   // up to 5 s for the device to come up
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (result_ != 0) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    bool failed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failed = result_ != 1;
    }
    if (failed) {
        close();
        return false;
    }
    return true;
}

void AudioOutput::run(int sampleRate) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT32 frames = 0;
    bool ok = event != nullptr;
    if (ok) ok = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)));
    if (ok) ok = SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
    if (ok) ok = SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)));
    if (ok) {
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 1;
        fmt.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = static_cast<WORD>(fmt.nChannels * fmt.wBitsPerSample / 8);
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
        const REFERENCE_TIME bufferTime = 600000;   // 60 ms in 100 ns units (the device may round it up)
        ok = SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, bufferTime, 0, &fmt, nullptr));
    }
    if (ok) ok = SUCCEEDED(client->SetEventHandle(event)) && SUCCEEDED(client->GetBufferSize(&frames)) && SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render)));
    if (ok) {   // start with a full buffer of silence
        BYTE* data = nullptr;
        ok = SUCCEEDED(render->GetBuffer(frames, &data));
        if (ok) render->ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
        std::lock_guard<std::mutex> lock(mutex_);
        base_ = frames;
        buffered_ = frames;
    }
    if (ok) ok = SUCCEEDED(client->Start());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = ok ? 1 : -1;
    }
    if (ok) running_ = true;
    while (ok && !quit_) {
        WaitForSingleObject(event, 100);
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) break;
        const UINT32 target = static_cast<UINT32>(sampleRate / 50);   // keep about 20 ms ahead, not the whole buffer: a new sound is heard that much later
        const UINT32 room = std::min(frames - padding, target > padding ? target - padding : 0u);
        if (room == 0) continue;
        BYTE* data = nullptr;
        if (FAILED(render->GetBuffer(room, &data))) break;
        auto* out = reinterpret_cast<std::int16_t*>(data);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const std::size_t n = std::min<std::size_t>(pending_.size() - readPos_, room);
            for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::int16_t>(std::max<std::int32_t>(-32767, std::min<std::int32_t>(32767, pending_[readPos_ + i])));
            for (std::size_t i = n; i < room; ++i) out[i] = 0;
            readPos_ += n;
            if (readPos_ == pending_.size()) {
                pending_.clear();
                readPos_ = 0;
            } else if (readPos_ > 65536) {   // compact now and then, not on every pass
                pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(readPos_));
                readPos_ = 0;
            }
            base_ += room;
            buffered_ = padding + room;
        }
        render->ReleaseBuffer(room, 0);
    }
    running_ = false;
    if (client) client->Stop();
    release(render);
    release(client);
    release(device);
    release(enumerator);
    if (event) CloseHandle(event);
    if (SUCCEEDED(com)) CoUninitialize();
    std::lock_guard<std::mutex> lock(mutex_);
    if (result_ == 0) result_ = -1;
}

void AudioOutput::play(const core::Pcm& pcm) {
    if (!running_ || pcm.samples.empty() || pcm.sampleRate != rate_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    const std::int64_t start = std::max(scheduledEnd_, base_);
    const std::size_t offset = readPos_ + static_cast<std::size_t>(start - base_);
    if (pending_.size() < offset + pcm.samples.size()) pending_.resize(offset + pcm.samples.size(), 0);
    for (std::size_t i = 0; i < pcm.samples.size(); ++i) pending_[offset + i] += pcm.samples[i];
    scheduledEnd_ = start + static_cast<std::int64_t>(pcm.nominal != 0 ? pcm.nominal : pcm.samples.size());
}

bool AudioOutput::busy() {
    if (!running_) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return scheduledEnd_ > base_ - buffered_;   // what is not yet heard
}

void AudioOutput::stop() {
    if (!running_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.clear();
    readPos_ = 0;
    scheduledEnd_ = base_;
}

void AudioOutput::close() {
    quit_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
}

}  // namespace pacman::app
