#include <alsa/asoundlib.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "ringbuffer.hpp"

void console_countdown(int seconds)
{
    for (int i = seconds; i > 0; --i)
    {
        std::cout << "Starting capture in " << i << "...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "GO\n\n";
}

static void fatal_audio_error(const char* msg, int err, snd_pcm_t* handle)
{
    std::cerr << msg << ": " << snd_strerror(err) << "\n";
    if (handle)
        snd_pcm_close(handle);
    std::exit(1);
}

static snd_pcm_t* audio_open_device(
    const char* device,
    snd_pcm_stream_t stream_type,
    unsigned int& sample_rate,
    int channels,
    snd_pcm_format_t format,
    snd_pcm_uframes_t& frames_per_buffer)
{
    snd_pcm_t* handle = nullptr;
    snd_pcm_hw_params_t* hw_params = nullptr;

    int err = snd_pcm_open(&handle, device, stream_type, 0);
    if (err < 0)
        fatal_audio_error("snd_pcm_open failed", err, handle);

    snd_pcm_hw_params_alloca(&hw_params);

    err = snd_pcm_hw_params_any(handle, hw_params);
    if (err < 0)
        fatal_audio_error("snd_pcm_hw_params_any failed", err, handle);

    err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
        fatal_audio_error("set_access failed", err, handle);

    err = snd_pcm_hw_params_set_format(handle, hw_params, format);
    if (err < 0)
        fatal_audio_error("set_format failed", err, handle);

    err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
    if (err < 0)
        fatal_audio_error("set_channels failed", err, handle);

    err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &sample_rate, nullptr);
    if (err < 0)
        fatal_audio_error("set_rate_near failed", err, handle);

    err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &frames_per_buffer, nullptr);
    if (err < 0)
        fatal_audio_error("set_period_size_near failed", err, handle);

    err = snd_pcm_hw_params(handle, hw_params);
    if (err < 0)
        fatal_audio_error("snd_pcm_hw_params failed", err, handle);

    err = snd_pcm_prepare(handle);
    if (err < 0)
        fatal_audio_error("snd_pcm_prepare failed", err, handle);

    return handle;
}

static snd_pcm_sframes_t cap_read_buffer(
    snd_pcm_t* handle,
    std::vector<int16_t>& capture_buffer)
{
    snd_pcm_sframes_t frames_read =
        snd_pcm_readi(handle, capture_buffer.data(), capture_buffer.size());

    if (frames_read == -EPIPE) {
        std::cerr << "Capture overrun occurred; re-preparing device\n";
        snd_pcm_prepare(handle);
        return 0;
    }

    if (frames_read < 0) {
        std::cerr << "Capture read error: " << snd_strerror(frames_read) << "\n";
        snd_pcm_prepare(handle);
        return 0;
    }

    return frames_read;
}

static snd_pcm_sframes_t rend_write_buffer(
    snd_pcm_t* handle,
    const std::vector<int16_t>& playback_buffer)
{
    snd_pcm_sframes_t frames_written =
        snd_pcm_writei(handle, playback_buffer.data(), playback_buffer.size());

    if (frames_written == -EPIPE) {
        std::cerr << "Playback underrun occurred; re-preparing device\n";
        snd_pcm_prepare(handle);
        return 0;
    }

    if (frames_written < 0) {
        std::cerr << "Playback write error: " << snd_strerror(frames_written) << "\n";
        snd_pcm_prepare(handle);
        return 0;
    }

    return frames_written;
}

static size_t rb_push_samples(
    RingBuffer<int16_t>& rb,
    const std::vector<int16_t>& capture_buffer,
    snd_pcm_sframes_t frames_read)
{
    size_t pushed = 0;

    for (snd_pcm_sframes_t i = 0; i < frames_read; ++i) {
        if (!rb.push(capture_buffer[i])) {
            std::cerr << "Ring buffer full; dropping remaining captured samples\n";
            break;
        }
        ++pushed;
    }

    return pushed;
}

static bool rb_pop_frame(
    RingBuffer<int16_t>& rb,
    std::vector<int16_t>& frame)
{
    if (rb.size() < frame.size())
        return false;

    for (size_t i = 0; i < frame.size(); ++i) {
        if (!rb.pop(frame[i])) {
            std::cerr << "Unexpected buffer underrun while forming frame\n";
            return false;
        }
    }

    return true;
}

static void dsp_apply_gain(std::vector<int16_t>& frame, double gain)
{
    for (size_t i = 0; i < frame.size(); ++i) {
        int sample = static_cast<int>(frame[i] * gain);

        if (sample > 32767)  sample = 32767;
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
    // Audio configuration
    const char* device = "plughw:2,0";
    unsigned int capture_sample_rate = 16000;
    unsigned int playback_sample_rate = 16000;
    const int channels = 1;
    const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

    // Larger periods reduce underruns
    snd_pcm_uframes_t capture_frames_per_buffer = 640;   // 40 ms @ 16 kHz
    snd_pcm_uframes_t playback_frames_per_buffer = 640;  // 40 ms @ 16 kHz

    // Pipeline configuration
    const size_t processing_frame_samples = 640;
    const size_t ring_capacity = 8192;
    const double gain = 1.5;          // slightly safer than 2.0
    const int iterations = 100;        // ~2 seconds at 40 ms/frame
    const int playback_prefill_frames = 3;

    snd_pcm_t* capture_handle = audio_open_device(
        device,
        SND_PCM_STREAM_CAPTURE,
        capture_sample_rate,
        channels,
        format,
        capture_frames_per_buffer);

    snd_pcm_t* playback_handle = audio_open_device(
        device,
        SND_PCM_STREAM_PLAYBACK,
        playback_sample_rate,
        channels,
        format,
        playback_frames_per_buffer);

    RingBuffer<int16_t> audio_rb(ring_capacity);
    std::vector<int16_t> capture_buffer(capture_frames_per_buffer);
    std::vector<int16_t> processing_frame(processing_frame_samples);
    std::vector<int16_t> silence_frame(processing_frame_samples, 0);

    FILE* raw_file = std::fopen("samples.raw", "wb");
    FILE* amp_file = std::fopen("amplified.raw", "wb");
    if (!raw_file || !amp_file) {
        std::cerr << "Failed to open output files\n";
        if (raw_file) std::fclose(raw_file);
        if (amp_file) std::fclose(amp_file);
        snd_pcm_close(capture_handle);
        snd_pcm_close(playback_handle);
        return 1;
    }

    console_countdown(3);

    std::cout << "Capture device: " << device << "\n";
    std::cout << "Playback device: " << device << "\n";
    std::cout << "Capture sample rate: " << capture_sample_rate << "\n";
    std::cout << "Playback sample rate: " << playback_sample_rate << "\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Capture frames per buffer: " << capture_frames_per_buffer << "\n";
    std::cout << "Playback frames per buffer: " << playback_frames_per_buffer << "\n";
    std::cout << "Processing frame size: " << processing_frame_samples << "\n";
    std::cout << "Ring capacity: " << ring_capacity << "\n";
    std::cout << "Gain: " << gain << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Playback prefill frames: " << playback_prefill_frames << "\n\n";

    // Prefill playback with silence so playback starts with some cushion
    for (int i = 0; i < playback_prefill_frames; ++i) {
        snd_pcm_sframes_t frames_written =
            rend_write_buffer(playback_handle, silence_frame);

        if (frames_written <= 0) {
            std::cerr << "Failed to prefill playback buffer\n";
            break;
        }
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        snd_pcm_sframes_t frames_read =
            cap_read_buffer(capture_handle, capture_buffer);

        if (frames_read <= 0)
            continue;

        // Save original captured audio
        file_write_samples(raw_file, capture_buffer.data(), frames_read);

        // Push captured samples into ring buffer
        size_t pushed = rb_push_samples(audio_rb, capture_buffer, frames_read);

        // Keep console output lighter to avoid disturbing timing
        if ((iteration % 5) == 0) {
            std::cout << "Iteration " << iteration
                      << ": captured " << frames_read
                      << ", pushed " << pushed
                      << ", ring size " << audio_rb.size() << "\n";

            std::cout << "  First 8 samples: ";
            for (int i = 0; i < 8 && i < static_cast<int>(frames_read); ++i)
                std::cout << capture_buffer[i] << " ";
            std::cout << "\n";
        }

        // Form one processing frame, apply gain, save processed output, render it
        if (rb_pop_frame(audio_rb, processing_frame)) {
            dsp_apply_gain(processing_frame, gain);

            file_write_samples(
                amp_file,
                processing_frame.data(),
                processing_frame.size());

            snd_pcm_sframes_t frames_written =
                rend_write_buffer(playback_handle, processing_frame);

            if ((iteration % 5) == 0) {
                std::cout << "  Processed one frame, wrote " << frames_written
                          << " frames to playback, ring size now "
                          << audio_rb.size() << "\n\n";
            }
        } else {
            if ((iteration % 5) == 0) {
                std::cout << "  Not enough data for one processing frame\n\n";
            }
        }
    }

    // Let playback drain before shutdown
    snd_pcm_drain(playback_handle);

    std::fclose(raw_file);
    std::fclose(amp_file);
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    return 0;
}