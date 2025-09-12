CC = gcc
TARGET = chip8
SOURCE = chip8.c

# Default build for Apple Silicon macOS
all:
	$(CC) -O3 -I/opt/homebrew/include -L/opt/homebrew/lib -o $(TARGET) $(SOURCE) -lSDL2

# Linux build
linux:
	$(CC) -O3 `pkg-config --cflags sdl2` -o $(TARGET) $(SOURCE) `pkg-config --libs sdl2` -lm

# Windows build (MSYS2/MinGW)
windows:
	$(CC) -O3 -I/mingw64/include -L/mingw64/lib -o $(TARGET).exe $(SOURCE) -lSDL2main -lSDL2 -lm

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all linux windows clean
