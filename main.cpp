#include <alsa/asoundlib.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include "ringbuffer.hpp"

static void fatal_audio_error(const char* msg, int err, snd_pcm_t* handle)
{
    std::cerr << msg << ": " << snd_strerror(err) << "\n";
    if (handle)
        snd_pcm_close(handle);
    std::exit(1);
}

static snd_pcm_t* cap_open_device(
    const char* device,
    unsigned int requested_sample_rate,
    unsigned int& actual_sample_rate,
    int channels,
    snd_pcm_format_t format,
    snd_pcm_uframes_t& actual_capture_frames)
{
    snd_pcm_t* capture_handle = nullptr;
    snd_pcm_hw_params_t* hw_params = nullptr;

    int err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0)
        fatal_audio_error("snd_pcm_open failed", err, capture_handle);

    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(capture_handle, hw_params);
    if (err < 0)
        fatal_audio_error("snd_pcm_hw_params_any failed", err, capture_handle);

    err = snd_pcm_hw_params_set_access(
        capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
        fatal_audio_error("set_access failed", err, capture_handle);

    err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format);
    if (err < 0)
        fatal_audio_error("set_format failed", err, capture_handle);

    err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    if (err < 0)
        fatal_audio_error("set_channels failed", err, capture_handle);

    err = snd_pcm_hw_params_set_rate_near(
        capture_handle, hw_params, &actual_sample_rate, nullptr);
    if (err < 0)
        fatal_audio_error("set_rate_near failed", err, capture_handle);

    err = snd_pcm_hw_params_set_period_size_near(
        capture_handle, hw_params, &actual_capture_frames, nullptr);
    if (err < 0)
        fatal_audio_error("set_period_size_near failed", err, capture_handle);

    err = snd_pcm_hw_params(capture_handle, hw_params);
    if (err < 0)
        fatal_audio_error("snd_pcm_hw_params failed", err, capture_handle);

    err = snd_pcm_prepare(capture_handle);
    if (err < 0)
        fatal_audio_error("snd_pcm_prepare failed", err, capture_handle);

    return capture_handle;
}

static snd_pcm_sframes_t cap_read_buffer(
    snd_pcm_t* capture_handle,
    std::vector<int16_t>& captureBuffer)
{
    snd_pcm_sframes_t frames_read =
        snd_pcm_readi(capture_handle, captureBuffer.data(), captureBuffer.size());

    if (frames_read == -EPIPE) {
        std::cerr << "Overrun occurred; re-preparing device\n";
        snd_pcm_prepare(capture_handle);
        return 0;
    }

    if (frames_read < 0) {
        std::cerr << "Read error: " << snd_strerror(frames_read) << "\n";
        snd_pcm_prepare(capture_handle);
        return 0;
    }

    return frames_read;
}

static size_t rb_push_samples(
    RingBuffer<int16_t>& audioBuffer,
    const std::vector<int16_t>& captureBuffer,
    snd_pcm_sframes_t frames_read)
{
    size_t pushed = 0;

    for (snd_pcm_sframes_t i = 0; i < frames_read; ++i) {
        if (!audioBuffer.push(captureBuffer[i])) {
            std::cerr << "Ring buffer full; dropping remaining captured samples\n";
            break;
        }
        ++pushed;
    }

    return pushed;
}

static bool rb_pop_frame(
    RingBuffer<int16_t>& audioBuffer,
    std::vector<int16_t>& processingFrame)
{
    if (audioBuffer.size() < processingFrame.size())
        return false;

    for (size_t i = 0; i < processingFrame.size(); ++i) {
        if (!audioBuffer.pop(processingFrame[i])) {
            std::cerr << "Unexpected buffer underrun while forming processing frame\n";
            return false;
        }
    }

    return true;
}

static void dsp_apply_gain(std::vector<int16_t>& frame, double gain)
{
    for (size_t i = 0; i < frame.size(); ++i) {
        int sample = frame[i];
        sample = static_cast<int>(sample * gain);

        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;

        frame[i] = static_cast<int16_t>(sample);
    }
}

static void file_write_samples(FILE* file, const int16_t* data, size_t count)
{
    std::fwrite(data, sizeof(int16_t), count, file);
}

int main()
{
    const char* device = "default";
    const unsigned int requested_sample_rate = 16000;
    unsigned int actual_sample_rate = requested_sample_rate;

    const int channels = 1;
    const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t actual_capture_frames = 320;   // 20 ms at 16 kHz

    const size_t processing_frame_samples = 320;     // also 20 ms
    const size_t ring_capacity = 8192;               // about 0.5 s at 16 kHz
    const double gain = 2.0;

    snd_pcm_t* capture_handle = cap_open_device(
        device,
        requested_sample_rate,
        actual_sample_rate,
        channels,
        format,
        actual_capture_frames);

    RingBuffer<int16_t> audioBuffer(ring_capacity);
    std::vector<int16_t> captureBuffer(actual_capture_frames);
    std::vector<int16_t> processingFrame(processing_frame_samples);

    FILE* rawFile = std::fopen("samples.raw", "wb");
    FILE* ampFile = std::fopen("amplified.raw", "wb");
    if (!rawFile || !ampFile) {
        std::cerr << "Failed to open output files\n";
        if (rawFile) std::fclose(rawFile);
        if (ampFile) std::fclose(ampFile);
        snd_pcm_close(capture_handle);
        return 1;
    }

    std::cout << "Capturing from ALSA device: " << device << "\n";
    std::cout << "Requested sample rate: " << requested_sample_rate
              << ", actual sample rate: " << actual_sample_rate << "\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Capture frames per read: " << actual_capture_frames << "\n";
    std::cout << "Processing frame size: " << processing_frame_samples << "\n";
    std::cout << "Ring capacity: " << ring_capacity << " samples\n";
    std::cout << "DSP gain: " << gain << "\n";
    std::cout << "Writing raw audio to samples.raw\n";
    std::cout << "Writing amplified audio to amplified.raw\n\n";

    for (int iteration = 0; iteration < 20; ++iteration) {
        snd_pcm_sframes_t frames_read =
            cap_read_buffer(capture_handle, captureBuffer);

        if (frames_read <= 0)
            continue;

        file_write_samples(rawFile, captureBuffer.data(), frames_read);

        size_t pushed = rb_push_samples(audioBuffer, captureBuffer, frames_read);

        std::cout << "Iteration " << iteration
                  << ": captured " << frames_read
                  << " frames, pushed " << pushed
                  << ", ring size " << audioBuffer.size() << "\n";

        std::cout << "  First 8 captured samples: ";
        for (int i = 0; i < 8 && i < static_cast<int>(frames_read); ++i)
            std::cout << captureBuffer[i] << " ";
        std::cout << "\n";

        if (rb_pop_frame(audioBuffer, processingFrame)) {
            dsp_apply_gain(processingFrame, gain);
            file_write_samples(ampFile, processingFrame.data(), processingFrame.size());

            std::cout << "  Processed one 20 ms frame with gain"
                      << ", ring size now " << audioBuffer.size() << "\n";
        } else {
            std::cout << "  Not enough data yet for one processing frame\n";
        }

        std::cout << "\n";
    }

    std::fclose(rawFile);
    std::fclose(ampFile);
    snd_pcm_close(capture_handle);
    return 0;
}