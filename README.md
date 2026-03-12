# alsarb

A C++ program demonstrating a real-time audio pipeline on Linux using **ALSA** and a custom **ring buffer**, with a threaded producer/consumer architecture.

The program captures microphone audio, serializes raw samples to disk, processes frames through a DSP stage, serializes the processed output, and renders it to a playback device — all across two concurrent threads.

---

## Pipeline

Two threads run concurrently, decoupled by a ring buffer:

```
CAPTURE THREAD
  Mic → ALSA capture (snd_pcm_readi) → samples.raw → RingBuffer (push)

PLAYBACK THREAD
  RingBuffer (wait/pop) → DSP gain → amplified.raw → ALSA playback (snd_pcm_writei) → Speaker
```

The ring buffer is the synchronization boundary between threads. The capture thread pushes samples and signals via a condition variable. The playback thread blocks until a full processing frame is available, then pops, processes, serializes, and renders it.

---

## Threading Model

| Thread | Responsibilities |
|---|---|
| Capture thread | `snd_pcm_readi` → write `samples.raw` → `RingBuffer::push` → `notify_one` |
| Main thread (playback) | `cond_var wait` → `RingBuffer::pop` → `dsp_apply_gain` → write `amplified.raw` → `snd_pcm_writei` |

Shared state between threads:

- `RingBuffer<int16_t>` — the sample queue
- `std::mutex` — protects ring buffer access
- `std::condition_variable` — playback thread sleeps until data is available
- `std::atomic<bool> capture_done` — signals playback thread to drain and exit

---

## Files

### `main.cpp`

Single-file implementation of the full pipeline. Contains all helper functions and `main()`.

Helper functions:

```
audio_open_device()   — opens an ALSA PCM device for capture or playback
cap_read_buffer()     — reads frames from ALSA; handles overrun (EPIPE)
rend_write_buffer()   — writes frames to ALSA; handles underrun (EPIPE)
rb_push_samples()     — pushes captured samples into the ring buffer
rb_pop_frame()        — pops a fixed-size frame from the ring buffer
dsp_apply_gain()      — applies gain with int16 saturation clamping
file_write_samples()  — writes raw PCM samples to a file
```

### `ringbuffer.hpp`

Templated circular buffer. Fixed capacity, non-blocking push/pop, head/tail indices with modulo wrap. One slot intentionally unused to distinguish full from empty.

```cpp
RingBuffer<int16_t> rb(8192);
rb.push(sample);
rb.pop(sample);
rb.size();
```

---

## Audio Format

| Property | Value |
|---|---|
| Sample format | Signed 16-bit little-endian (S16_LE) |
| Sample rate | 16,000 Hz |
| Channels | Mono |
| Frame size | 320 samples |
| Frame duration | 20 ms |

The 20 ms frame size (320 samples at 16 kHz) is standard in speech processing systems such as VAD, VoIP, and ML inference pipelines. At this frame size scheduling jitter can cause occasional playback underruns on non-real-time kernels.

---

## Build

Install ALSA development headers if needed:

```
sudo apt install libasound2-dev
```

Compile debug build:

```
make
```

Compile release build:

```
make release
```

Clean:

```
make clean
```

---

## Running

```
./alsarb
```

The program counts down, then captures approximately 4 seconds of audio. Console output is throttled to every 5 iterations to avoid disturbing timing.

---

## Output Files

Two raw PCM files are written for debugging and validation:

**`samples.raw`** — raw captured audio, written by the capture thread immediately after `snd_pcm_readi`.

**`amplified.raw`** — processed audio written by the playback thread after `dsp_apply_gain`, recording exactly what was sent to the speaker.

Play them back with `aplay`:

```
aplay -f S16_LE -r 16000 -c 1 samples.raw
aplay -f S16_LE -r 16000 -c 1 amplified.raw
```

---

## DSP Stage

The current DSP stage applies a linear gain:

```
output_sample = clamp(input_sample × gain, -32768, 32767)
```

Clamping prevents integer overflow distortion at the int16 boundary. This stage is a placeholder — it can be replaced with filters, noise suppression, VAD, or ML inference without changing the pipeline architecture.

---

## ALSA Functions

| Function | Purpose |
|---|---|
| `snd_pcm_open` | Open capture or playback device |
| `snd_pcm_hw_params_*` | Configure format, rate, channels, period size |
| `snd_pcm_prepare` | Prepare device for streaming |
| `snd_pcm_readi` | Read captured audio frames |
| `snd_pcm_writei` | Write playback audio frames |
| `snd_pcm_drain` | Flush playback before shutdown |
| `snd_pcm_close` | Close device |

EPIPE (overrun on capture, underrun on playback) is handled by calling `snd_pcm_prepare` and continuing.

---

## Possible Extensions

- VAD (voice activity detection) in the DSP stage
- Echo cancellation using playback reference signal
- Streaming frames to an external API (e.g. OpenAI Realtime API)
- Lock-free ring buffer using `std::atomic` head/tail
- Configurable device and parameters via command line
- WAV file output with header
