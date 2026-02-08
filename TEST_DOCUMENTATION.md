# CHIP-8 Emulator Test Suite Documentation

## Overview

This document provides comprehensive documentation for the CHIP-8 emulator test suite. The test suite is designed to verify all core functionality of the CHIP-8 emulator without requiring SDL2 or any graphics libraries.

## Running the Tests

### Quick Start

```bash
make test
```

This command will:
1. Compile the test suite
2. Run all tests automatically
3. Display results with pass/fail status
4. Return exit code 0 if all tests pass, 1 if any fail

### Building Without Running

```bash
make test-build
```

This builds the test executable without running it. You can then run it manually:

```bash
./chip8_test
```

## Test Categories

The test suite is organized into 11 comprehensive categories covering all aspects of CHIP-8 emulation:

### 1. Memory and Initialization Tests
- **Purpose**: Verify proper initialization of all CHIP-8 components
- **Tests**:
  - Memory zeroing (program area)
  - Register initialization
  - Display buffer clearing
  - Timer initialization
  - Stack initialization
  - Font loading into memory

### 2. Display Operations Tests
- **Purpose**: Test graphics and display functionality
- **Instructions Tested**: `00E0`, `DXYN`
- **Tests**:
  - Clear screen operation
  - Sprite drawing
  - Draw flag setting
  - Collision detection

### 3. Flow Control Tests
- **Purpose**: Verify program flow control mechanisms
- **Instructions Tested**: `1NNN`, `2NNN`, `00EE`, `BNNN`
- **Tests**:
  - Unconditional jump
  - Subroutine calls
  - Return from subroutine
  - Jump with offset (V0 + address)
  - Stack push/pop during calls

### 4. Conditional Operations Tests
- **Purpose**: Test conditional branching logic
- **Instructions Tested**: `3XNN`, `4XNN`, `5XY0`, `9XY0`
- **Tests**:
  - Skip if register equals value
  - Skip if register not equals value
  - Skip if registers equal
  - Skip if registers not equal

### 5. Register Operations Tests
- **Purpose**: Verify all arithmetic and logical register operations
- **Instructions Tested**: `6XNN`, `7XNN`, `8XY0-8XYE`
- **Tests**:
  - Load immediate value
  - Add immediate value
  - Copy register
  - Bitwise OR
  - Bitwise AND
  - Bitwise XOR
  - Add with carry flag
  - Subtract with borrow flag
  - Shift right with LSB in VF
  - Reverse subtract
  - Shift left with MSB in VF

### 6. Memory Operations Tests
- **Purpose**: Test memory access and I register operations
- **Instructions Tested**: `ANNN`, `FX1E`, `FX29`, `FX33`, `FX55`, `FX65`
- **Tests**:
  - Set I register to address
  - Add to I register
  - Load font sprite location
  - Binary-coded decimal conversion
  - Store registers to memory
  - Load registers from memory

### 7. Timer Operations Tests
- **Purpose**: Verify delay and sound timer functionality
- **Instructions Tested**: `FX07`, `FX15`, `FX18`
- **Tests**:
  - Set delay timer
  - Read delay timer
  - Set sound timer
  - Sound enable on timer set

### 8. Keyboard Operations Tests
- **Purpose**: Test keyboard input handling
- **Instructions Tested**: `EX9E`, `EXA1`, `FX0A`
- **Tests**:
  - Skip if key pressed
  - Skip if key not pressed
  - Wait for key press and store

### 9. Audio System Tests
- **Purpose**: Verify audio subsystem state management
- **Tests**:
  - Audio data structure initialization
  - Sound timer value setting
  - Sound enable flag behavior
  - Timer countdown mechanism

### 10. Stack Operations Tests
- **Purpose**: Test the call stack implementation
- **Tests**:
  - Push operation
  - Pop operation (LIFO behavior)
  - Stack size tracking
  - Multiple push/pop operations

### 11. Edge Cases and Boundary Tests
- **Purpose**: Verify behavior at boundaries and edge conditions
- **Tests**:
  - Register overflow (wrapping)
  - Register underflow (wrapping)
  - Sprite drawing near screen edges
  - Memory boundary conditions

## Test Output Format

### Individual Test Results
```
[PASS] Test description
[FAIL] Test description
```

### Summary Report
```
========================================
Test Summary
========================================
Total tests:  79
Passed:       79
Failed:       0
Success rate: 100.0%
========================================
```

## Understanding Test Results

- **Total tests**: Number of test assertions executed
- **Passed**: Number of tests that passed
- **Failed**: Number of tests that failed
- **Success rate**: Percentage of tests passed

A success rate of 100% indicates all tests passed.

## Adding New Tests

To add new tests to the suite:

1. **Create a test function** following the naming convention `test_<category>()`

2. **Use test_assert()** for assertions:
   ```c
   test_assert(condition, "Test description");
   ```

3. **Call your test** from `main()`:
   ```c
   int main() {
       // ... existing tests ...
       test_new_category();
       print_test_summary();
       return (g_stats.failed > 0) ? 1 : 0;
   }
   ```

4. **Rebuild and run**:
   ```bash
   make test
   ```

## Test Implementation Details

### Test Framework
The test suite uses a simple assertion-based framework:
- `test_assert(condition, description)` - Assert a condition
- `g_stats` - Global statistics tracker
- `print_test_summary()` - Display final results

### CHIP-8 Implementation
The test suite includes a minimal CHIP-8 implementation without SDL dependencies:
- All core instruction implementations
- Memory management
- Stack operations
- Display buffer (without rendering)
- Audio state management (without actual sound generation)

### Why Tests Don't Require SDL2
The test suite focuses on CHIP-8 instruction logic and state management, not rendering or audio playback. This allows:
- Fast test execution
- No external dependencies
- Easy integration into CI/CD pipelines
- Platform-independent testing

## Continuous Integration

The test suite is designed for CI/CD integration:

```yaml
# Example GitHub Actions workflow
- name: Run CHIP-8 Tests
  run: make test
```

Exit codes:
- `0` - All tests passed
- `1` - One or more tests failed

## Common Issues and Troubleshooting

### Compilation Errors
- **Issue**: `gcc: command not found`
- **Solution**: Install GCC compiler: `apt-get install gcc` (Linux) or `brew install gcc` (macOS)

### Test Failures
If tests fail:
1. Check the failed test description
2. Review the corresponding instruction implementation in `chip8.c`
3. Verify the test expectations match CHIP-8 specification
4. Use a debugger to step through the failing test

### Memory Issues
- All memory is stack-allocated in tests
- No dynamic allocation means no memory leaks
- Valgrind can be used for additional verification:
  ```bash
  valgrind --leak-check=full ./chip8_test
  ```

## CHIP-8 Specification References

The tests are based on the standard CHIP-8 specification:
- Memory: 4KB (4096 bytes)
- Display: 64x32 pixels, monochrome
- Registers: 16 8-bit (V0-VF, where VF is flags)
- Stack: 16 levels
- Timers: Delay and sound, count down at 60Hz
- Input: 16-key hexadecimal keypad
- Audio: Single tone at 440Hz when sound timer > 0

## Performance Considerations

The test suite is designed for speed:
- No I/O operations during core tests
- No SDL initialization
- Direct memory access
- Minimal function call overhead

Typical execution time: < 0.1 seconds for all 79 tests

## Coverage

The test suite covers:
- ✅ All 35 standard CHIP-8 instructions
- ✅ Memory operations
- ✅ Stack operations
- ✅ Display operations (logical, not visual)
- ✅ Timer management
- ✅ Keyboard input logic
- ✅ Audio state management
- ✅ Edge cases and boundary conditions

Not covered (requires SDL/graphics environment):
- Actual rendering to screen
- Real-time audio playback
- Physical keyboard input
- Window management

## License

This test suite is part of the CHIP-8 emulator project and shares the same license.

## Contributing

When contributing new features to the CHIP-8 emulator:
1. Add corresponding tests first (TDD approach)
2. Ensure all existing tests still pass
3. Achieve 100% test success rate before submitting PR
4. Document any new test categories

## References

- [CHIP-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8)
- [Cowgod's CHIP-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [CHIP-8 Test Suite by Timendus](https://github.com/Timendus/chip8-test-suite)
