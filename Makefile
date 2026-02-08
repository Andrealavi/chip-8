CC = gcc
TARGET = chip8
SOURCE = chip8.c
TEST_TARGET = chip8_test
TEST_SOURCE = chip8_test.c

# Default build for Apple Silicon macOS
all:
	$(CC) -O3 -I/opt/homebrew/include -L/opt/homebrew/lib -o $(TARGET) $(SOURCE) -lSDL2

# Linux build
linux:
	$(CC) -O3 `pkg-config --cflags sdl2` -o $(TARGET) $(SOURCE) `pkg-config --libs sdl2` -lm

# Windows build (MSYS2/MinGW)
windows:
	$(CC) -O3 -I/mingw64/include -L/mingw64/lib -o $(TARGET).exe $(SOURCE) -lSDL2main -lSDL2 -lm

# Build test suite (platform-independent, no SDL required)
test: $(TEST_SOURCE)
	$(CC) -O2 -o $(TEST_TARGET) $(TEST_SOURCE) -lm
	./$(TEST_TARGET)

# Build test binary without running
test-build: $(TEST_SOURCE)
	$(CC) -O2 -o $(TEST_TARGET) $(TEST_SOURCE) -lm

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET).exe $(TEST_TARGET)

.PHONY: all linux windows test test-build clean
