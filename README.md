# ALSA Ring Buffer Audio Pipeline

## Overview

This project is a small C++ program that demonstrates a basic real-time audio processing pipeline using the Linux ALSA API and a custom ring buffer.

The program captures microphone audio from ALSA, pushes samples into a ring buffer, extracts fixed-size processing frames, applies a simple DSP operation (gain), and writes both the raw and processed audio streams to disk.

This project is intended as a learning and study exercise for understanding:

- ALSA audio capture
- real-time audio buffering
- ring buffer design
- frame-based DSP pipelines
- raw PCM audio debugging workflows

The code is intentionally simple and structured to make the data flow easy to follow.

---

## Audio Processing Pipeline

The program implements the following pipeline:

```
Mic
 ↓
ALSA capture (snd_pcm_readi)
 ↓
capture buffer
 ↓
RingBuffer
 ↓
processing frame (20 ms)
 ↓
DSP stage (simple gain amplification)
 ↓
write raw and processed audio to files
```

The ring buffer decouples the capture stage from the DSP processing stage, which is a common pattern in real-time audio systems.

---

## Project Structure

```
alsarb/
├── main.cpp
├── ringbuffer.hpp
├── Makefile
├── README.md
```

### main.cpp

Implements the audio pipeline:

- opens the ALSA capture device
- reads audio frames from ALSA
- pushes samples into the ring buffer
- pops processing frames
- applies a DSP gain stage
- writes output to files

The code is organized into small helper functions:

```
cap_open_device()
cap_read_buffer()
rb_push_samples()
rb_pop_frame()
dsp_apply_gain()
file_write_samples()
```

### ringbuffer.hpp

A simple templated ring buffer implementation.

Features:

- fixed capacity
- push/pop operations
- constant-time insert and removal
- suitable for real-time producer/consumer pipelines

The template allows it to be used with any data type:

```
RingBuffer<float>
RingBuffer<int16_t>
RingBuffer<double>
```

In this project it stores `int16_t` audio samples.

---

## Audio Format

The program captures audio in the following format:

| Property | Value |
|--------|--------|
| Sample format | Signed 16-bit little-endian |
| Type | `int16_t` |
| Sample rate | 16,000 Hz |
| Channels | Mono |
| Frame size | 320 samples |
| Frame duration | 20 ms |

The 20 ms frame size is common in speech processing systems such as:

- speech recognition
- VoIP
- voice activity detection
- ML inference pipelines

---

## Build

Requires:

- Linux
- ALSA development libraries
- g++

Install ALSA development headers if needed:

```
sudo apt install libasound2-dev
```

Build the program:

```
make
```

Clean build artifacts:

```
make clean
```

---

## Running

Run the program:

```
./alsarb
```

The program captures approximately two seconds of audio.

During execution it prints:

- number of captured frames
- ring buffer occupancy
- sample values for debugging

---

## Output Files

The program writes two files:

```
samples.raw
amplified.raw
```

`samples.raw` contains the captured microphone audio.

`amplified.raw` contains the processed audio after applying a simple gain DSP stage.

Both files are raw PCM data.

---

## Playing the Raw Audio

Because the files are raw PCM (no header), the format must be specified when playing them.

Play the captured audio:

```
aplay -f S16_LE -r 16000 -c 1 samples.raw
```

Play the amplified audio:

```
aplay -f S16_LE -r 16000 -c 1 amplified.raw
```

Where:

- `S16_LE` = signed 16-bit little-endian samples
- `16000` = sample rate
- `1` = mono channel

---

## Debugging Workflow

A common audio development workflow is:

1. Capture audio
2. Dump raw samples to a file
3. Play the file
4. Add DSP or ML processing
5. Validate output

This project follows that pattern.

---

## Possible Extensions

This example can easily be extended with additional DSP or ML components such as:

- filters
- automatic gain control
- voice activity detection
- feature extraction
- edge ML inference

The ring buffer architecture allows capture and processing to operate independently.

---

## License

This project is intended for learning and experimentation.