#include <alsa/asoundlib.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>

#define DEBUG_MESSAGES 1
#include "debugmsg.hpp"
#include "ringbuffer.hpp"
#include "wav.hpp"

void print_help(const char* prog)
{
    std::cout
        << "Usage:\n"
        << "  " << prog << " [mode]\n\n"

        << "Modes:\n"
        << "  test-ring      Run ring buffer stress test\n"
        << "  list-devices   List available ALSA PCM devices\n"
        << "  --help, -h     Show this help message\n\n"

        << "Default behavior:\n"
        << "  If no mode is specified, the program runs the ALSA audio\n"
        << "  capture → ring buffer → DSP → playback pipeline.\n\n"

        << "Examples:\n"
        << "  " << prog << "\n"
        << "  " << prog << " test-ring\n"
        << "  " << prog << " list-devices\n";
}
void list_devices()
{
    void **hints;
    int err = snd_device_name_hint(-1, "pcm", &hints);

    if (err != 0) {
        std::cerr << "snd_device_name_hint failed: " << snd_strerror(err) << "\n";
        return;
    }

    std::cout << "Available ALSA PCM devices:\n\n";

    void **n = hints;

    while (*n != nullptr)
    {
        char *name = snd_device_name_get_hint(*n, "NAME");
        char *desc = snd_device_name_get_hint(*n, "DESC");
        char *ioid = snd_device_name_get_hint(*n, "IOID");

        if (name)
        {
            std::cout << "Device: " << name;

            if (ioid)
                std::cout << " (" << ioid << ")";

            std::cout << "\n";

            if (desc)
                std::cout << desc << "\n";

            std::cout << "\n";
        }

        if (name) free(name);
        if (desc) free(desc);
        if (ioid) free(ioid);

        n++;
    }

    snd_device_name_free_hint(hints);
}

int test_ringbuffer()
{
    std::cout << "Running ring buffer stress test...\n";

    RingBuffer<int> rb(1024);
    const int iterations = 1000000;

    for (int i = 0; i < iterations; ++i) {

        if (!rb.push(i)) {
            std::cerr << "Push failed at iteration " << i << "\n";
            return 3;
        }

        int value;

        if (!rb.pop(value)) {
            std::cerr << "Pop failed at iteration " << i << "\n";
            return 2;
        }

        if (value != i) {
            std::cerr << "Data mismatch: expected "
                      << i << " got " << value << "\n";
            return 1;
        }
    }

    std::cout << "Ring buffer test passed (" << iterations << " iterations)\n";

    return 0;
}
void play_beep(snd_pcm_t* handle, int freq, int duration_ms)
{
    const int sample_rate = 16000;

    int samples = sample_rate * duration_ms / 1000;
    std::vector<int16_t> buf(samples);

    double phase = 0.0;
    double step = 2.0 * M_PI * freq / sample_rate;

    for (int i = 0; i < samples; ++i) {
        buf[i] = static_cast<int16_t>(sin(phase) * 12000);
        phase += step;
    }

    snd_pcm_sframes_t written = snd_pcm_writei(handle, buf.data(), samples);

    if (written == -EPIPE) {
        snd_pcm_prepare(handle);
        written = snd_pcm_writei(handle, buf.data(), samples);
    }

    if (written < 0) {
        std::cerr << "Beep write error: " << snd_strerror(written) << "\n";
    }
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

void console_countdown(int secs, const char* device)
{
    unsigned int sample_rate = 16000;
    int channels = 1;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_uframes_t frames = 320;

    snd_pcm_t* handle = audio_open_device(
        device,
        SND_PCM_STREAM_PLAYBACK,
        sample_rate,
        channels,
        format,
        frames);

    for (int i = secs; i > 0; --i)
    {
        std::cout << "Starting capture in " << i << "...\n";

        play_beep(handle, 800, 120);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "GO\n\n";
    snd_pcm_prepare(handle);
    play_beep(handle, 1200, 300);
    snd_pcm_drain(handle);

    snd_pcm_close(handle);
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

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        std::string mode = argv[1];

        if (mode == "test-ring")
            return test_ringbuffer();

        if (mode == "list-devices") {
            list_devices();
            return 0;
        }

        if (mode == "--help" || mode == "-h") {
            print_help(argv[0]);
            return 0;
        }

        std::cerr << "Unknown mode: " << mode << "\n\n";
        print_help(argv[0]);
        return 1;
    }



    // Audio configuration
    const char* device = "plughw:2,0";
    unsigned int capture_sample_rate = 16000;
    unsigned int playback_sample_rate = 16000;
    const int channels = 1;
    const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

    snd_pcm_uframes_t capture_frames_per_buffer = 320;   // 20 ms @ 16 kHz
    snd_pcm_uframes_t playback_frames_per_buffer = 320;  // 20 ms @ 16 kHz

    // Pipeline configuration
    const size_t processing_frame_samples = 320;
    const size_t ring_capacity = 8192;
    const double gain = 4.0;
    const int iterations = 200;
    const int playback_prefill_frames = 3;

    console_countdown(3, device);

    snd_pcm_t* capture_handle = audio_open_device(
        device, SND_PCM_STREAM_CAPTURE,
        capture_sample_rate, channels, format, capture_frames_per_buffer);

    snd_pcm_t* playback_handle = audio_open_device(
        device, SND_PCM_STREAM_PLAYBACK,
        playback_sample_rate, channels, format, playback_frames_per_buffer);

    // Shared state between capture and playback threads
    RingBuffer<int16_t> audio_rb(ring_capacity);
    std::mutex rb_mutex;
    std::condition_variable rb_cv;
    std::atomic<bool> capture_done(false);

    std::vector<int16_t> silence_frame(processing_frame_samples, 0);

    WavFile wav_cap = wav_begin("samples.wav", 16000, 1);
    WavFile wav_amp = wav_begin("amplified.wav", 16000, 1);
    if (!wav_cap.file || !wav_amp.file) {
        std::cerr << "Failed to open WAV output files\n";
    
        if (wav_cap.file) wav_end(wav_cap);
        if (wav_amp.file) wav_end(wav_amp);
    
        snd_pcm_close(capture_handle);
        snd_pcm_close(playback_handle);
        return 1;
    }

    std::cout << "Capture device:          " << device << "\n";
    std::cout << "Capture sample rate:     " << capture_sample_rate << "\n";
    std::cout << "Playback sample rate:    " << playback_sample_rate << "\n";
    std::cout << "Channels:                " << channels << "\n";
    std::cout << "Capture frames/buffer:   " << capture_frames_per_buffer << "\n";
    std::cout << "Playback frames/buffer:  " << playback_frames_per_buffer << "\n";
    std::cout << "Processing frame size:   " << processing_frame_samples << "\n";
    std::cout << "Ring capacity:           " << ring_capacity << "\n";
    std::cout << "Gain:                    " << gain << "\n";
    std::cout << "Iterations:              " << iterations << "\n\n";

    // Prefill playback buffer with silence
    for (int i = 0; i < playback_prefill_frames; ++i) {
        snd_pcm_sframes_t written = rend_write_buffer(playback_handle, silence_frame);
        if (written <= 0) {
            std::cerr << "Failed to prefill playback buffer\n";
            break;
        }
    }

    // --- Capture thread ---
    // Reads from ALSA, writes raw file, pushes into ring buffer,
    // signals the playback thread when data is available.
    std::thread capture_thread([&]()
    {
        std::vector<int16_t> capture_buffer(capture_frames_per_buffer);

        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            snd_pcm_sframes_t frames_read =
                cap_read_buffer(capture_handle, capture_buffer);

            if (frames_read <= 0)
                continue;

            // assumes mono samples
            wav_write(wav_cap, capture_buffer.data(), frames_read);

            {
                std::lock_guard<std::mutex> lock(rb_mutex);
                rb_push_samples(audio_rb, capture_buffer, frames_read);
            }

            rb_cv.notify_one();

            if ((iteration % 10) == 0) {
                dbgmsg("[capture] iteration " << iteration  << ": captured " << frames_read << " frames\n");
            }
        }

        capture_done = true;
        rb_cv.notify_one();  // wake playback thread so it can drain and exit
    });

    // --- Playback thread (main thread) ---
    // Waits for data in the ring buffer, pops a frame, applies DSP, renders.
    {
        std::vector<int16_t> processing_frame(processing_frame_samples);
        int frames_processed = 0;

        while (true)
        {
            std::unique_lock<std::mutex> lock(rb_mutex);

            // Wait until there is a full frame available, or capture is done
            rb_cv.wait(lock, [&]() {
                return audio_rb.size() >= processing_frame_samples || capture_done.load();
            });

            // Drain whatever is left in the ring buffer before exiting
            if (!rb_pop_frame(audio_rb, processing_frame)) {
                if (capture_done.load())
                    break;
                continue;
            }

            lock.unlock();

            dsp_apply_gain(processing_frame, gain);

            wav_write(wav_amp, processing_frame.data(), processing_frame.size());

            rend_write_buffer(playback_handle, processing_frame);

            ++frames_processed;

            if ((frames_processed % 10) == 0) {
                dbgmsg("[playback] processed frame " << frames_processed << "\n");
            }
        }
    }

    capture_thread.join();

    snd_pcm_drain(playback_handle);

    wav_end(wav_cap);
    wav_end(wav_amp);
    //std::fclose(raw_file);
    //std::fclose(amp_file);
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    return 0;
}
