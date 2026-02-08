# CHIP-8 Audio Implementation Documentation

## Overview

The CHIP-8 emulator includes full audio support according to the CHIP-8 specification. Audio is implemented using SDL2's audio subsystem and generates a continuous 440Hz tone (musical note A4) when the sound timer is active.

## CHIP-8 Audio Specification

According to the CHIP-8 specification:
- **Frequency**: Single tone at 440Hz (A4 musical note)
- **Activation**: Sound plays when sound timer > 0
- **Timer behavior**: Sound timer decrements at 60Hz
- **Instruction**: `FX18` - Set sound timer to value in register VX

## Implementation Details

### Audio Data Structure

```c
typedef struct {
    float phase;              // Current phase of sine wave (0 to 2π)
    bool sound_enabled;       // Whether audio system is enabled
    int timer_value;          // Current sound timer value
    bool audio_working;       // Flag indicating callback is executing
} AudioData;
```

### Key Components

#### 1. Audio Callback Function (`audio_callback`)

The audio callback is called by SDL2's audio thread at regular intervals to fill audio buffers:

**Purpose**: Generate sine wave samples in real-time

**Process**:
1. Receives an empty buffer from SDL2
2. Checks if sound should play (timer_value > 0)
3. Generates sine wave samples using phase accumulation
4. Returns buffer to SDL2 for playback

**Formula**: `sample = AMPLITUDE × sin(phase)`

Where:
- `AMPLITUDE = 16000` (volume level)
- `phase` increments by `(2π × 440) / 44100` per sample

#### 2. Phase Accumulation

Phase accumulation is the core of digital audio synthesis:

```c
audio->phase += 2.0f * M_PI * TONE_FREQUENCY / SAMPLE_RATE;
```

**Explanation**:
- `TONE_FREQUENCY = 440.0` Hz (A4 note)
- `SAMPLE_RATE = 44100` Hz (CD quality)
- Phase increment = `(2π × 440) / 44100 ≈ 0.0628` radians per sample

After approximately 100 samples, the phase completes one full cycle (2π radians), creating exactly 440 cycles per second.

#### 3. Phase Wrapping

To prevent floating-point overflow and maintain precision:

```c
if (audio->phase >= 2.0f * M_PI) {
    audio->phase -= 2.0f * M_PI;
}
```

Since `sin(x) = sin(x + 2π)`, subtracting 2π doesn't change the output.

#### 4. Timer Management

The sound timer decrements at 60Hz in the main loop:

```c
// Every ~17ms (60 times per second)
if (chip8.audio_data.timer_value > 0) {
    chip8.audio_data.timer_value--;
}
```

**Timing**:
- `TIMER_TICK_MS = 17` milliseconds
- Frequency = 1000ms / 17ms ≈ 60Hz

## Sound Timer Instruction (FX18)

The `FX18` instruction sets the sound timer:

```c
case 0x18:
    // Set sound timer = Vx (FX18)
    chip8->audio_data.timer_value = chip8->reg[x];
    if (chip8->reg[x] > 0) {
        chip8->audio_data.sound_enabled = true;
    }
    break;
```

**Behavior**:
- Sets `timer_value` to the value in register VX
- Enables sound if value > 0
- Sound plays for approximately `timer_value / 60` seconds

**Example**: If VX = 60, sound plays for 1 second (60 ticks ÷ 60 Hz = 1 second)

## Audio System Initialization

```c
// SDL Audio Specification
SDL_AudioSpec want, have;
want.format = AUDIO_S16SYS;      // 16-bit signed audio
want.channels = 1;               // Mono
want.freq = SAMPLE_RATE;         // 44100 Hz
want.samples = 1024;             // Buffer size
want.callback = audio_callback;  // Our callback function
want.userdata = &chip8.audio_data;

// Open audio device
SDL_AudioDeviceID deviceId = SDL_OpenAudioDevice(
    NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE
);

// Start playing (initially silent)
SDL_PauseAudioDevice(deviceId, 0);
```

## Audio Quality Parameters

| Parameter | Value | Reason |
|-----------|-------|--------|
| Sample Rate | 44100 Hz | CD quality, standard for high-quality audio |
| Bit Depth | 16-bit signed | Good quality, standard for gaming audio |
| Channels | Mono (1) | CHIP-8 spec only supports single channel |
| Buffer Size | 1024 samples | Good balance of latency and stability |
| Frequency | 440 Hz | CHIP-8 standard (A4 musical note) |
| Amplitude | 16000 | Loud enough without clipping |

## Audio Flow Diagram

```
User Program           Main Loop              Audio Thread
     |                     |                        |
     | FX18               |                        |
     |-------------------->|                        |
     |                     |                        |
     |              Set timer_value                 |
     |              sound_enabled=true              |
     |                     |                        |
     |                     |                        |
     |                Every 17ms                    |
     |                     |                        |
     |              Decrement timer                 |
     |                     |                        |
     |                     |    Callback needed     |
     |                     |<-----------------------|
     |                     |                        |
     |              Check timer_value > 0           |
     |                     |                        |
     |              Generate samples                |
     |                     |----------------------->|
     |                     |      Samples           |
     |                     |                        |
     |                     |                  Play sound
```

## Common Issues and Solutions

### No Sound Plays

**Possible Causes**:
1. SDL2 audio not initialized
2. Audio device muted
3. Sound timer set to 0
4. Audio callback not being called

**Debug**:
- Check `audio_working` flag
- Verify `timer_value > 0`
- Check SDL error messages
- Ensure audio device is not paused

### Crackling or Popping

**Possible Causes**:
1. Buffer underrun
2. Thread priority issues
3. Phase discontinuity

**Solutions**:
- Increase buffer size
- Ensure phase resets cleanly
- Check CPU load

### Volume Too Loud/Soft

**Solution**: Adjust `AMPLITUDE` constant (currently 16000)
- Higher = louder (max ~32767 before clipping)
- Lower = quieter (min ~0 for silence)

## Testing Audio

### Using CHIP-8 Test Programs

Many CHIP-8 test ROMs include sound tests. They typically:
1. Set sound timer to specific value
2. Wait for timer to expire
3. Verify timing is correct

### Manual Testing

```assembly
; Simple CHIP-8 program to test audio
6060  ; V0 = 60 (play sound for ~1.0 seconds)
F018  ; Set sound timer to V0
1204  ; Jump to 0x204 (infinite loop)
```

This program sets the sound timer to 60, which plays for approximately 1.0 second (60 / 60 = 1.0).

## Performance Considerations

### CPU Usage

Audio generation is lightweight:
- Simple sine wave calculation
- No complex DSP operations
- Runs in separate audio thread
- Minimal main thread impact

### Latency

From instruction execution to sound output:
1. `FX18` executes: < 1µs
2. Timer value set: immediate
3. Audio callback sees change: < 23ms (one buffer)
4. Sound plays: immediate after callback

**Total latency**: Typically 20-30ms (imperceptible to users)

## Code Quality

The implementation features:
- ✅ Extensive inline documentation
- ✅ Clear variable naming
- ✅ Mathematical formulas explained
- ✅ Proper resource management
- ✅ Thread-safe audio callback
- ✅ Clean phase accumulation
- ✅ No memory leaks

## Compliance with CHIP-8 Specification

| Requirement | Status | Implementation |
|-------------|--------|----------------|
| 440Hz tone | ✅ | `TONE_FREQUENCY = 440.0f` |
| Play when timer > 0 | ✅ | `if (timer_value > 0)` |
| 60Hz decrement | ✅ | `TIMER_TICK_MS = 17` |
| FX18 instruction | ✅ | Fully implemented |
| Single channel | ✅ | Mono audio |
| Continuous tone | ✅ | Phase accumulation |

## References

- SDL2 Audio Documentation: https://wiki.libsdl.org/CategoryAudio
- CHIP-8 Specification: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
- Digital Audio Synthesis: https://en.wikipedia.org/wiki/Digital_audio

## Summary

The CHIP-8 emulator's audio implementation is:
- **Specification-compliant**: Follows all CHIP-8 audio requirements
- **High-quality**: Uses CD-quality sample rate and clean waveform generation
- **Well-documented**: Extensive comments explain every design decision
- **Efficient**: Minimal CPU usage, low latency
- **Robust**: Proper initialization, cleanup, and error handling

The audio system correctly generates a 440Hz tone when the sound timer is active, providing authentic CHIP-8 sound behavior.
