# CHIP-8 Audio Test Program

This directory contains simple CHIP-8 programs for testing audio functionality.

## test_beep.ch8

A simple program that plays a beep sound for 1 second and then stops.

### Assembly Code
```assembly
; Set sound timer to 60 (plays for 1 second at 60Hz)
6060    ; V0 = 60
F018    ; Set sound timer = V0

; Infinite loop to keep program running
1204    ; Jump to address 0x204
```

### Hexadecimal Machine Code
```
60 60 F0 18 12 04
```

### Expected Behavior
- Sound plays immediately when program starts
- Beep lasts approximately 1 second (60/60 = 1.0 seconds)
- Program continues running in infinite loop
- No sound after timer expires

### Creating the Test ROM

You can create this ROM file with the following command:

```bash
echo -n -e '\x60\x60\xF0\x18\x12\x04' > test_beep.ch8
```

Or using Python:

```python
with open('test_beep.ch8', 'wb') as f:
    f.write(bytes([0x60, 0x60, 0xF0, 0x18, 0x12, 0x04]))
```

### Running the Test

```bash
./chip8 test_beep.ch8
```

You should hear a 440Hz tone for approximately 1 second.

## test_repeating_beep.ch8

A program that plays repeating beeps with pauses between them.

### Assembly Code
```assembly
; Main loop - play beep and wait
6030    ; V0 = 48 (0.8 second beep)
F018    ; Set sound timer = V0
6130    ; V1 = 48 (0.8 second delay)
F115    ; Set delay timer = V1

; Wait for delay timer to expire
F107    ; V1 = delay timer
3100    ; Skip if V1 == 0
120A    ; Jump back to wait loop (0x20A)

; Repeat main loop
1200    ; Jump to beginning (0x200)
```

### Hexadecimal Machine Code
```
60 30 F0 18 61 30 F1 15 F1 07 31 00 12 0A 12 00
```

### Expected Behavior
- Beep for 0.8 seconds
- Silence for 0.8 seconds
- Repeat indefinitely

### Creating the Test ROM

```bash
echo -n -e '\x60\x30\xF0\x18\x61\x30\xF1\x15\xF1\x07\x31\x00\x12\x0A\x12\x00' > test_repeating_beep.ch8
```

## Debugging Audio Issues

If you don't hear sound:

1. **Check SDL2 audio initialization**:
   - Ensure SDL_Init includes SDL_INIT_AUDIO
   - Check for SDL error messages

2. **Verify audio device**:
   - Check system volume is not muted
   - Verify audio output device is working
   - Test with other audio applications

3. **Check CHIP-8 sound timer**:
   - Add debug prints to see timer_value
   - Verify FX18 instruction is executed
   - Confirm timer decrements at 60Hz

4. **Verify audio callback**:
   - Check audio_working flag is set
   - Ensure callback is being called
   - Verify phase accumulation

## Audio Specification Quick Reference

- **Frequency**: 440 Hz (A4 musical note)
- **Sample Rate**: 44100 Hz
- **Format**: 16-bit signed mono
- **Timer Rate**: 60 Hz (decrements 60 times per second)
- **Instruction**: FX18 (set sound timer to VX)

## Additional Test Ideas

### Long Beep Test
```assembly
60FF    ; V0 = 255 (maximum value)
F018    ; Set sound timer = V0
; Should play for 255/60 ≈ 4.25 seconds
```

### Short Beep Test
```assembly
6001    ; V0 = 1
F018    ; Set sound timer = V0
; Should play for 1/60 ≈ 0.017 seconds (very short chirp)
```

### No Sound Test
```assembly
6000    ; V0 = 0
F018    ; Set sound timer = V0
; Should not play any sound
```
