# CHIP-8 Interpreter/Emulator

A fully functional CHIP-8 interpreter/emulator written in C with SDL2 for graphics and audio. This implementation provides authentic CHIP-8 behavior including display rendering, keyboard input, timers, and audio feedback.

## Features

- Complete CHIP-8 instruction set implementation
- Full keyboard mapping support
- 64x32 pixel display with customizable scaling
- Audio generation with 440Hz tone (authentic CHIP-8 beep)

## Technical Specifications

- **Memory**: 4KB RAM
- **Display**: 64x32 monochrome pixels
- **Registers**: 16 8-bit general-purpose registers (V0-VF)
- **Stack**: 16-level call stack
- **Timers**: Delay timer and sound timer (60Hz)
- **Input**: 16-key hexadecimal keypad
- **Audio**: Single-tone beeper at 440Hz

## Installation

### Prerequisites

Make sure you have the following installed:
- GCC/Clang compiler
- Make utility
- SDL2 development libraries

### Installing SDL2

#### macOS
```bash
brew install sdl2
```

#### Linux

```bash
# Update package manager
sudo apt update

# Install SDL2 development libraries
sudo apt install libsdl2-dev

# For other distributions:
# Fedora/RHEL:
sudo dnf install SDL2-devel
# Arch Linux:
sudo pacman -S sdl2
# openSUSE:
sudo zypper install libSDL2-devel
```

#### Windows

```bash
# Using MSYS2 (recommended for Windows development)
pacman -S mingw-w64-x86_64-SDL2

# Manual installation:
# 1. Download SDL2 development libraries from https://www.libsdl.org/
# 2. Extract to a directory (e.g., C:\SDL2)
# 3. Add SDL2\bin to your PATH environment variable
# 4. Update the Makefile with correct SDL2 paths if needed
```

### Building the emulator

```bash
git clone https://github.com/Andrealavi/chip-8
cd chip-8

# For Apple Silicon macOS users
make

# For Linux users
make linux

# For Windows users (with MSYS2/MinGW)
make windows
```

Note: The Makefile has been primarily tested on Apple Silicon macOS. While Linux and Windows targets are included, you may need to adjust compiler flags or SDL2 paths for your specific environment.

## Usage

### Running a CHIP-8 program

Execute any CHIP-8 ROM file (.ch8 extension) with:

```bash

./chip8 <rom_file>

# Example:
./chip8 games/pong.ch8
./chip8 roms/tetris.ch8
```

### Keyboard controls

The CHIP-8 uses a 16-key hexadecimal keypad (0-F) mapped to your keyboard:

```
CHIP-8 Keypad    →    Your Keyboard
1 2 3 C               1 2 3 4
4 5 6 D               Q W E R
7 8 9 E               A S D F
A 0 B F               Z X C V
```

It should be work with other layouts as well, since it was implemented using keycodes instead of pressed char.

## ROM Compatibility

This emulator supports standard CHIP-8 ROMs and has been tested with:

- Classic games (Pong, Snake)
- Test ROMs for instruction verification

ROM Sources: You can find CHIP-8 ROMs at:

- [CHIP-8 Archive](https://johnearnest.github.io/chip8Archive/)
- [CHIP-8 Test Suite](https://github.com/Timendus/chip8-test-suite)
