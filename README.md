# alsarb

A small C++ program demonstrating a simple real-time audio pipeline on Linux using **ALSA** and a custom **ring buffer**.

The program captures microphone audio, processes it, and optionally renders it to a playback device while also writing raw audio files for inspection.

This project is intended as a **learning and interview study project** for understanding real-time audio pipelines.

---

# Overview

The program implements the following pipeline:

```
Microphone
   ↓
ALSA Capture
   ↓
capture_buffer
   ↓
RingBuffer
   ↓
processing_frame
   ↓
DSP (gain)
   ↓
File output + Playback
```

Audio flows through several stages:

1. Capture audio from an ALSA device
2. Store incoming samples in a ring buffer
3. Pop fixed-size frames from the ring buffer
4. Apply a simple DSP effect (gain)
5. Write both original and processed audio to files
6. Render processed audio to a playback device

---

# Features

* ALSA microphone capture
* ALSA audio playback
* Lock-free style ring buffer implementation
* Simple DSP example (gain)
* Raw audio file output for debugging
* Console diagnostics for learning purposes

---

# Files

### `main.cpp`

Main program implementing the audio pipeline.

Contains the high-level stages:

* ALSA device setup
* audio capture loop
* ring buffer processing
* DSP processing
* playback rendering
* file output

---

### `ringbuffer.hpp`

A simple templated circular buffer implementation.

Supports:

```
push()
pop()
size()
```

The ring buffer decouples capture timing from processing timing.

---

### `makefile`

Builds the program using `g++` and links against ALSA.

---

# Building

Install ALSA development headers if needed:

```
sudo apt install libasound2-dev
```

Compile:

```
make
```

Clean build artifacts:

```
make clean
```

---

# Running

Run the program:

```
./alsarb
```

The program will:

1. Count down
2. Capture audio
3. Process frames
4. Write audio files
5. Play processed audio

Example output:

```
Starting capture in 3...
Starting capture in 2...
Starting capture in 1...
GO

Capture device: plughw:2,0
Playback device: plughw:2,0
Sample rate: 16000
Channels: 1
```

---

# Output Files

Two files are written for debugging:

### `samples.raw`

Raw captured microphone audio.

### `amplified.raw`

Audio after DSP gain processing.

---

# Listening to the Raw Files

These files are **16-bit signed little endian PCM** at **16 kHz mono**.

Play them using ALSA:

```
aplay -f S16_LE -r 16000 -c 1 samples.raw
```

```
aplay -f S16_LE -r 16000 -c 1 amplified.raw
```

---

# Audio Parameters

Current configuration:

```
Sample rate:      16000 Hz
Channels:         1 (mono)
Format:           S16_LE
Frame size:       640 samples
Frame duration:   40 ms
```

### Why 640 samples?

640 samples at 16 kHz equals **40 ms of audio**.

This slightly larger buffer helps prevent **ALSA playback underruns** during development while still keeping latency relatively small.

---

# DSP Example

The program applies a simple gain effect:

```
output_sample = input_sample × gain
```

The result is clamped to the valid 16-bit range:

```
[-32768, 32767]
```

This prevents integer overflow distortion.

---

# ALSA Functions Used

The program uses several ALSA PCM functions:

| Function | Purpose |
|--------|--------|
| `snd_pcm_open` | Open audio device |
| `snd_pcm_hw_params_any` | Initialize hardware parameters |
| `snd_pcm_hw_params_set_access` | Set access type |
| `snd_pcm_hw_params_set_format` | Set audio format |
| `snd_pcm_hw_params_set_channels` | Set channel count |
| `snd_pcm_hw_params_set_rate_near` | Negotiate sample rate |
| `snd_pcm_hw_params_set_period_size_near` | Configure buffer period |
| `snd_pcm_hw_params` | Apply hardware configuration |
| `snd_pcm_prepare` | Prepare device for streaming |
| `snd_pcm_readi` | Read captured audio |
| `snd_pcm_writei` | Write playback audio |
| `snd_pcm_drain` | Finish playback before shutdown |
| `snd_pcm_close` | Close device |

---

# Notes on Real-Time Audio

This program is intentionally simple and single-threaded.

Real production audio systems often use:

* separate capture and playback threads
* callback-based audio APIs
* larger buffering strategies
* lock-free ring buffers

This project focuses on **clarity and learning** rather than production performance.

---

# Possible Improvements

Future enhancements could include:

* multi-threaded capture / playback
* real-time latency measurements
* WAV file output
* more advanced DSP (filters, AGC, noise suppression)
* configurable devices via command line
* stereo support

---

