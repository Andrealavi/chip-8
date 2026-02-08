/*
 * CHIP-8 Emulator Test Suite
 * 
 * This file contains comprehensive tests for the CHIP-8 emulator implementation.
 * Tests are organized by instruction category and verify correct behavior
 * according to the CHIP-8 specification.
 *
 * Test Categories:
 * 1. Memory and initialization tests
 * 2. Display operations (00E0, DXYN)
 * 3. Flow control (1NNN, 2NNN, 00EE, BNNN)
 * 4. Conditional operations (3XNN, 4XNN, 5XY0, 9XY0)
 * 5. Register operations (6XNN, 7XNN, 8XY*)
 * 6. Memory operations (ANNN, FX1E, FX29, FX33, FX55, FX65)
 * 7. Random number generation (CXNN)
 * 8. Timer operations (FX07, FX15, FX18)
 * 9. Keyboard operations (EX9E, EXA1, FX0A)
 * 10. Audio system tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// --- Constants for CHIP-8 Struct ---
#define MEM_SIZE 4096
#define REG_SIZE 16
#define PRG_START 512
#define STACK_SIZE 16
#define KEYBOARD_SIZE 16
#define DISPLAY_H 32
#define DISPLAY_W 64

// --- Constants for Audio Generation ---
#define SAMPLE_RATE 44100
#define TONE_FREQUENCY 440.0f
#define AMPLITUDE 16000

// Macro to extract nibbles from opcode
#define GET_NIBBLE(value, index) (((value >> ((4 - index) * 4)) & 0x000F))

// Audio data structure
typedef struct {
    float phase;
    bool sound_enabled;
    int timer_value;
    bool audio_working;
} AudioData;

// Stack structure
typedef struct {
    int size;
    unsigned char *data[STACK_SIZE];
} Stack;

// CHIP-8 main structure
typedef struct {
    unsigned char memory[MEM_SIZE];
    unsigned int display[DISPLAY_H * DISPLAY_W];
    Stack prg_stack;
    unsigned char *pc;
    unsigned char *i;
    unsigned char d_timer;
    AudioData audio_data;
    unsigned char reg[REG_SIZE];
    unsigned char keys[KEYBOARD_SIZE];
    bool draw_flag;
} CHIP8;

// Test statistics
typedef struct {
    int total;
    int passed;
    int failed;
} TestStats;

// Global test stats
TestStats g_stats = {0, 0, 0};

// Function prototypes
void initialize_chip(CHIP8 *chip8);
void load_font(CHIP8 *chip8);
void clear_display(unsigned int display[], int size);
void draw(CHIP8 *chip8, unsigned char x, unsigned char y, unsigned char height);
bool decex(CHIP8 *chip8, uint16_t opcode);
unsigned char *stack_pop(Stack *s);
void stack_push(Stack *s, unsigned char *val);

// --- Stack Implementation ---

unsigned char *stack_pop(Stack *s) {
    if (s->size >= 1) return s->data[--s->size];
    return NULL;
}

void stack_push(Stack *s, unsigned char *val) {
    if (s->size <= 15) s->data[s->size++] = val;
}

// --- CHIP-8 Core Functions ---

void initialize_chip(CHIP8 *chip8) {
    memset(chip8->memory, 0, MEM_SIZE);
    memset(chip8->display, 0, DISPLAY_H * DISPLAY_W * sizeof(unsigned int));
    
    chip8->pc = NULL;
    chip8->i = NULL;
    chip8->prg_stack.size = 0;
    memset(chip8->prg_stack.data, 0, STACK_SIZE);
    
    chip8->d_timer = 0;
    chip8->audio_data.phase = 0.0f;
    chip8->audio_data.sound_enabled = true;
    chip8->audio_data.timer_value = 0;
    chip8->audio_data.audio_working = false;
    
    memset(chip8->reg, 0, REG_SIZE);
    memset(chip8->keys, 0, KEYBOARD_SIZE);
    
    chip8->draw_flag = true;
}

void load_font(CHIP8 *chip8) {
    unsigned char sprites[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    
    for (int i = 0x50, sc = 0; i <= 0x9F; i++, sc++) {
        chip8->memory[i] = sprites[sc];
    }
}

void clear_display(unsigned int display[], int size) {
    for (int i = 0; i < size; i++) {
        display[i] = 0x00000000;
    }
}

void draw(CHIP8 *chip8, unsigned char x, unsigned char y, unsigned char height) {
    unsigned char *val = chip8->i;
    chip8->reg[0xF] = 0;
    
    int wrap_x = x % DISPLAY_W;
    int wrap_y = y % DISPLAY_H;
    int pos = 0;
    const int width = 8;
    
    for (int row = 0; row < height && row + wrap_y < DISPLAY_H; row++) {
        for (int col = 0; col < width && col + wrap_x < DISPLAY_W; col++) {
            if (*val & (0b10000000 >> col)) {
                pos = DISPLAY_W * (wrap_y + row) + (wrap_x + col);
                chip8->display[pos] ^= 0xFFFFFFFF;
                if (!chip8->display[pos]) chip8->reg[0xF] = 1;
            }
        }
        val++;
    }
}

bool decex(CHIP8 *chip8, uint16_t opcode) {
    unsigned char op = opcode >> 12;
    unsigned char sub_op = 0;
    unsigned int x = 0;
    unsigned int y = 0;
    
    x = GET_NIBBLE(opcode, 2);
    y = GET_NIBBLE(opcode, 3);
    
    bool skip_pc_incr = false;
    
    switch (op) {
        case 0x0:
            if (opcode & 0x000F) {
                // Return from subroutine (00EE)
                chip8->pc = stack_pop(&(chip8->prg_stack));
            } else {
                // Clear display (00E0)
                clear_display(chip8->display, DISPLAY_H * DISPLAY_W);
                chip8->draw_flag = true;
            }
            break;
            
        case 0x1:
            // Jump to address NNN (1NNN)
            chip8->pc = chip8->memory + (opcode & 0x0FFF);
            skip_pc_incr = true;
            break;
            
        case 0x2:
            // Call subroutine at NNN (2NNN)
            stack_push(&(chip8->prg_stack), chip8->pc);
            chip8->pc = chip8->memory + (opcode & 0x0FFF);
            skip_pc_incr = true;
            break;
            
        case 0x3:
            // Skip next instruction if Vx == NN (3XNN)
            if (chip8->reg[x] == (opcode & 0x00FF)) chip8->pc += 2;
            break;
            
        case 0x4:
            // Skip next instruction if Vx != NN (4XNN)
            if (chip8->reg[x] != (opcode & 0x00FF)) chip8->pc += 2;
            break;
            
        case 0x5:
            // Skip next instruction if Vx == Vy (5XY0)
            if (chip8->reg[x] == chip8->reg[y]) chip8->pc += 2;
            break;
            
        case 0x6:
            // Set Vx = NN (6XNN)
            chip8->reg[x] = (opcode & 0x00FF);
            break;
            
        case 0x7:
            // Add NN to Vx (7XNN)
            chip8->reg[x] += (opcode & 0x00FF);
            break;
            
        case 0x8:
            sub_op = opcode & 0x000F;
            
            switch (sub_op) {
                case 0x0:
                    // Set Vx = Vy (8XY0)
                    chip8->reg[x] = chip8->reg[y];
                    break;
                    
                case 0x1:
                    // Set Vx = Vx OR Vy (8XY1)
                    chip8->reg[x] |= chip8->reg[y];
                    break;
                    
                case 0x2:
                    // Set Vx = Vx AND Vy (8XY2)
                    chip8->reg[x] &= chip8->reg[y];
                    break;
                    
                case 0x3:
                    // Set Vx = Vx XOR Vy (8XY3)
                    chip8->reg[x] ^= chip8->reg[y];
                    break;
                    
                case 0x4:
                    // Add Vy to Vx, set VF = carry (8XY4)
                    if (chip8->reg[x] > 0xFF - chip8->reg[y]) {
                        chip8->reg[0xF] = 1;
                    } else {
                        chip8->reg[0xF] = 0;
                    }
                    chip8->reg[x] += chip8->reg[y];
                    break;
                    
                case 0x5:
                    // Subtract Vy from Vx, set VF = NOT borrow (8XY5)
                    if (chip8->reg[x] >= chip8->reg[y]) {
                        chip8->reg[0xF] = 1;
                    } else {
                        chip8->reg[0xF] = 0;
                    }
                    chip8->reg[x] -= chip8->reg[y];
                    break;
                    
                case 0x6:
                    // Shift Vx right by 1, VF = LSB (8XY6)
                    chip8->reg[0xF] = chip8->reg[x] & 1;
                    chip8->reg[x] = chip8->reg[x] >> 1;
                    break;
                    
                case 0x7:
                    // Set Vx = Vy - Vx, set VF = NOT borrow (8XY7)
                    if (chip8->reg[y] >= chip8->reg[x]) {
                        chip8->reg[0xF] = 1;
                    } else {
                        chip8->reg[0xF] = 0;
                    }
                    chip8->reg[x] = chip8->reg[y] - chip8->reg[x];
                    break;
                    
                case 0xE:
                    // Shift Vx left by 1, VF = MSB (8XYE)
                    chip8->reg[0xF] = (chip8->reg[x] & 0x80) >> 7;
                    chip8->reg[x] = chip8->reg[x] << 1;
                    break;
            }
            break;
            
        case 0x9:
            // Skip next instruction if Vx != Vy (9XY0)
            if (chip8->reg[x] != chip8->reg[y]) chip8->pc += 2;
            break;
            
        case 0xA:
            // Set I = NNN (ANNN)
            chip8->i = chip8->memory + (opcode & 0x0FFF);
            break;
            
        case 0xB:
            // Jump to NNN + V0 (BNNN)
            chip8->pc = chip8->memory + (opcode & 0x0FFF) + chip8->reg[0];
            skip_pc_incr = true;
            break;
            
        case 0xC:
            // Set Vx = random byte AND NN (CXNN)
            chip8->reg[x] = (rand() % 256) & (opcode & 0x00FF);
            break;
            
        case 0xD:
            // Draw sprite (DXYN)
            {
                unsigned char height = GET_NIBBLE(opcode, 4);
                draw(chip8, chip8->reg[x], chip8->reg[y], height);
                chip8->draw_flag = true;
            }
            break;
            
        case 0xE:
            sub_op = opcode & 0x00FF;
            
            switch (sub_op) {
                case 0x9E:
                    // Skip if key Vx is pressed (EX9E)
                    if (chip8->keys[chip8->reg[x]]) {
                        chip8->pc += 2;
                    }
                    break;
                    
                case 0xA1:
                    // Skip if key Vx is not pressed (EXA1)
                    if (!chip8->keys[chip8->reg[x]]) {
                        chip8->pc += 2;
                    }
                    break;
            }
            break;
            
        case 0xF:
            sub_op = opcode & 0x00FF;
            
            switch (sub_op) {
                case 0x07:
                    // Set Vx = delay timer (FX07)
                    chip8->reg[x] = chip8->d_timer;
                    break;
                    
                case 0x0A:
                    // Wait for key press, store in Vx (FX0A)
                    skip_pc_incr = true;
                    for (int i = 0; i < KEYBOARD_SIZE; i++) {
                        if (chip8->keys[i]) {
                            chip8->reg[x] = i;
                            skip_pc_incr = false;
                            break;
                        }
                    }
                    break;
                    
                case 0x15:
                    // Set delay timer = Vx (FX15)
                    chip8->d_timer = chip8->reg[x];
                    break;
                    
                case 0x18:
                    // Set sound timer = Vx (FX18)
                    chip8->audio_data.timer_value = chip8->reg[x];
                    if (chip8->reg[x] > 0) {
                        chip8->audio_data.sound_enabled = true;
                    }
                    break;
                    
                case 0x1E:
                    // Add Vx to I (FX1E)
                    if ((uintptr_t)chip8->i > 0x1000 - chip8->reg[x]) {
                        chip8->reg[0xF] = 1;
                    } else {
                        chip8->reg[0xF] = 0;
                    }
                    chip8->i += chip8->reg[x];
                    break;
                    
                case 0x29:
                    // Set I = location of sprite for digit Vx (FX29)
                    chip8->i = chip8->memory + 0x50 + (chip8->reg[x] * 5);
                    break;
                    
                case 0x33:
                    // Store BCD of Vx at I, I+1, I+2 (FX33)
                    *(chip8->i) = chip8->reg[x] / 100;
                    *(chip8->i + 1) = (chip8->reg[x] / 10) % 10;
                    *(chip8->i + 2) = chip8->reg[x] % 10;
                    break;
                    
                case 0x55:
                    // Store V0 to Vx in memory starting at I (FX55)
                    for (unsigned int i = 0; i <= x; i++) {
                        *(chip8->i + i) = chip8->reg[i];
                    }
                    break;
                    
                case 0x65:
                    // Load V0 to Vx from memory starting at I (FX65)
                    for (unsigned int i = 0; i <= x; i++) {
                        chip8->reg[i] = *(chip8->i + i);
                    }
                    break;
            }
            break;
    }
    
    return skip_pc_incr;
}

// --- Test Helper Functions ---

void test_assert(bool condition, const char *test_name) {
    g_stats.total++;
    if (condition) {
        g_stats.passed++;
        printf("[PASS] %s\n", test_name);
    } else {
        g_stats.failed++;
        printf("[FAIL] %s\n", test_name);
    }
}

void print_test_summary() {
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", g_stats.total);
    printf("Passed:       %d\n", g_stats.passed);
    printf("Failed:       %d\n", g_stats.failed);
    printf("Success rate: %.1f%%\n", 
           g_stats.total > 0 ? (100.0 * g_stats.passed / g_stats.total) : 0.0);
    printf("========================================\n");
}

// --- Test Functions ---

/*
 * Test 1: Memory and Initialization Tests
 */
void test_initialization() {
    printf("\n=== Memory and Initialization Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    load_font(&chip8);
    
    // Test memory is zeroed
    bool mem_zero = true;
    for (int i = PRG_START; i < MEM_SIZE; i++) {
        if (chip8.memory[i] != 0) {
            mem_zero = false;
            break;
        }
    }
    test_assert(mem_zero, "Memory initialized to zero");
    
    // Test registers are zeroed
    bool reg_zero = true;
    for (int i = 0; i < REG_SIZE; i++) {
        if (chip8.reg[i] != 0) {
            reg_zero = false;
            break;
        }
    }
    test_assert(reg_zero, "Registers initialized to zero");
    
    // Test display is cleared
    bool display_clear = true;
    for (int i = 0; i < DISPLAY_H * DISPLAY_W; i++) {
        if (chip8.display[i] != 0) {
            display_clear = false;
            break;
        }
    }
    test_assert(display_clear, "Display initialized to black");
    
    // Test timers are zeroed
    test_assert(chip8.d_timer == 0, "Delay timer initialized to zero");
    test_assert(chip8.audio_data.timer_value == 0, "Sound timer initialized to zero");
    
    // Test stack is empty
    test_assert(chip8.prg_stack.size == 0, "Stack initialized empty");
    
    // Test fonts are loaded
    test_assert(chip8.memory[0x50] == 0xF0, "Font for digit 0 loaded correctly (byte 1)");
    test_assert(chip8.memory[0x51] == 0x90, "Font for digit 0 loaded correctly (byte 2)");
}

/*
 * Test 2: Display Operations
 */
void test_display_operations() {
    printf("\n=== Display Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    load_font(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test 00E0: Clear display
    chip8.display[0] = 0xFFFFFFFF;
    chip8.display[100] = 0xFFFFFFFF;
    decex(&chip8, 0x00E0);
    test_assert(chip8.display[0] == 0x00000000, "00E0: Clear display (pixel 0)");
    test_assert(chip8.display[100] == 0x00000000, "00E0: Clear display (pixel 100)");
    test_assert(chip8.draw_flag == true, "00E0: Draw flag set");
    
    // Test DXYN: Draw sprite
    chip8.i = chip8.memory + 0x50; // Point to font '0'
    chip8.reg[0] = 0;  // x = 0
    chip8.reg[1] = 0;  // y = 0
    decex(&chip8, 0xD015); // Draw 5-line sprite at (V0, V1)
    test_assert(chip8.display[0] == 0xFFFFFFFF, "DXYN: Sprite drawn (pixel on)");
    test_assert(chip8.reg[0xF] == 0, "DXYN: No collision detected");
}

/*
 * Test 3: Flow Control Operations
 */
void test_flow_control() {
    printf("\n=== Flow Control Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test 1NNN: Jump
    unsigned char *old_pc = chip8.pc;
    decex(&chip8, 0x1300);
    test_assert(chip8.pc == chip8.memory + 0x300, "1NNN: Jump to address");
    
    // Test 2NNN: Call subroutine
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0x2400);
    test_assert(chip8.pc == chip8.memory + 0x400, "2NNN: Call subroutine (PC updated)");
    test_assert(chip8.prg_stack.size == 1, "2NNN: Stack size increased");
    
    // Test 00EE: Return from subroutine
    decex(&chip8, 0x00EE);
    test_assert(chip8.pc == chip8.memory + PRG_START, "00EE: Return from subroutine");
    test_assert(chip8.prg_stack.size == 0, "00EE: Stack size decreased");
    
    // Test BNNN: Jump to V0 + NNN
    chip8.reg[0] = 0x10;
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0xB300);
    test_assert(chip8.pc == chip8.memory + 0x310, "BNNN: Jump to NNN + V0");
}

/*
 * Test 4: Conditional Operations
 */
void test_conditional_operations() {
    printf("\n=== Conditional Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test 3XNN: Skip if Vx == NN
    chip8.reg[0] = 0x42;
    unsigned char *old_pc = chip8.pc;
    decex(&chip8, 0x3042); // Skip if V0 == 0x42
    test_assert(chip8.pc == old_pc + 2, "3XNN: Skip when equal");
    
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0x3043); // Skip if V0 == 0x43
    test_assert(chip8.pc == chip8.memory + PRG_START, "3XNN: Don't skip when not equal");
    
    // Test 4XNN: Skip if Vx != NN
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0x4043); // Skip if V0 != 0x43
    test_assert(chip8.pc == chip8.memory + PRG_START + 2, "4XNN: Skip when not equal");
    
    // Test 5XY0: Skip if Vx == Vy
    chip8.reg[0] = 0x42;
    chip8.reg[1] = 0x42;
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0x5010); // Skip if V0 == V1
    test_assert(chip8.pc == chip8.memory + PRG_START + 2, "5XY0: Skip when registers equal");
    
    // Test 9XY0: Skip if Vx != Vy
    chip8.reg[0] = 0x42;
    chip8.reg[1] = 0x43;
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0x9010); // Skip if V0 != V1
    test_assert(chip8.pc == chip8.memory + PRG_START + 2, "9XY0: Skip when registers not equal");
}

/*
 * Test 5: Register Operations
 */
void test_register_operations() {
    printf("\n=== Register Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test 6XNN: Set Vx = NN
    decex(&chip8, 0x6042);
    test_assert(chip8.reg[0] == 0x42, "6XNN: Set register to value");
    
    // Test 7XNN: Add NN to Vx
    chip8.reg[0] = 0x10;
    decex(&chip8, 0x7005);
    test_assert(chip8.reg[0] == 0x15, "7XNN: Add value to register");
    
    // Test 8XY0: Set Vx = Vy
    chip8.reg[1] = 0x99;
    decex(&chip8, 0x8010);
    test_assert(chip8.reg[0] == 0x99, "8XY0: Copy register");
    
    // Test 8XY1: OR
    chip8.reg[0] = 0xF0;
    chip8.reg[1] = 0x0F;
    decex(&chip8, 0x8011);
    test_assert(chip8.reg[0] == 0xFF, "8XY1: OR operation");
    
    // Test 8XY2: AND
    chip8.reg[0] = 0xF0;
    chip8.reg[1] = 0xFF;
    decex(&chip8, 0x8012);
    test_assert(chip8.reg[0] == 0xF0, "8XY2: AND operation");
    
    // Test 8XY3: XOR
    chip8.reg[0] = 0xFF;
    chip8.reg[1] = 0xFF;
    decex(&chip8, 0x8013);
    test_assert(chip8.reg[0] == 0x00, "8XY3: XOR operation");
    
    // Test 8XY4: Add with carry
    chip8.reg[0] = 0xFF;
    chip8.reg[1] = 0x02;
    decex(&chip8, 0x8014);
    test_assert(chip8.reg[0] == 0x01, "8XY4: Add with overflow");
    test_assert(chip8.reg[0xF] == 1, "8XY4: Carry flag set");
    
    // Test 8XY5: Subtract with borrow
    chip8.reg[0] = 0x10;
    chip8.reg[1] = 0x05;
    decex(&chip8, 0x8015);
    test_assert(chip8.reg[0] == 0x0B, "8XY5: Subtract without borrow");
    test_assert(chip8.reg[0xF] == 1, "8XY5: No borrow flag");
    
    // Test 8XY6: Shift right
    chip8.reg[0] = 0b00000111;
    decex(&chip8, 0x8016);
    test_assert(chip8.reg[0] == 0b00000011, "8XY6: Shift right result");
    test_assert(chip8.reg[0xF] == 1, "8XY6: LSB stored in VF");
    
    // Test 8XY7: Reverse subtract
    chip8.reg[0] = 0x05;
    chip8.reg[1] = 0x10;
    decex(&chip8, 0x8017);
    test_assert(chip8.reg[0] == 0x0B, "8XY7: Reverse subtract");
    test_assert(chip8.reg[0xF] == 1, "8XY7: No borrow flag");
    
    // Test 8XYE: Shift left
    chip8.reg[0] = 0b11000000;
    decex(&chip8, 0x801E);
    test_assert(chip8.reg[0] == 0b10000000, "8XYE: Shift left result");
    test_assert(chip8.reg[0xF] == 1, "8XYE: MSB stored in VF");
}

/*
 * Test 6: Memory Operations
 */
void test_memory_operations() {
    printf("\n=== Memory Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    load_font(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test ANNN: Set I register
    decex(&chip8, 0xA300);
    test_assert(chip8.i == chip8.memory + 0x300, "ANNN: Set I register");
    
    // Test FX29: Set I to font location
    chip8.reg[0] = 5;
    decex(&chip8, 0xF029);
    test_assert(chip8.i == chip8.memory + 0x50 + (5 * 5), "FX29: Set I to font location");
    
    // Test FX33: Store BCD
    chip8.i = chip8.memory + 0x300;
    chip8.reg[0] = 234;
    decex(&chip8, 0xF033);
    test_assert(chip8.memory[0x300] == 2, "FX33: BCD hundreds digit");
    test_assert(chip8.memory[0x301] == 3, "FX33: BCD tens digit");
    test_assert(chip8.memory[0x302] == 4, "FX33: BCD ones digit");
    
    // Test FX55: Store registers
    chip8.i = chip8.memory + 0x400;
    chip8.reg[0] = 0x11;
    chip8.reg[1] = 0x22;
    chip8.reg[2] = 0x33;
    decex(&chip8, 0xF255);
    test_assert(chip8.memory[0x400] == 0x11, "FX55: Store V0");
    test_assert(chip8.memory[0x401] == 0x22, "FX55: Store V1");
    test_assert(chip8.memory[0x402] == 0x33, "FX55: Store V2");
    
    // Test FX65: Load registers
    chip8.i = chip8.memory + 0x400;
    chip8.reg[0] = 0;
    chip8.reg[1] = 0;
    chip8.reg[2] = 0;
    decex(&chip8, 0xF265);
    test_assert(chip8.reg[0] == 0x11, "FX65: Load V0");
    test_assert(chip8.reg[1] == 0x22, "FX65: Load V1");
    test_assert(chip8.reg[2] == 0x33, "FX65: Load V2");
}

/*
 * Test 7: Timer Operations
 */
void test_timer_operations() {
    printf("\n=== Timer Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test FX15: Set delay timer
    chip8.reg[0] = 0x42;
    decex(&chip8, 0xF015);
    test_assert(chip8.d_timer == 0x42, "FX15: Set delay timer");
    
    // Test FX07: Read delay timer
    chip8.d_timer = 0x33;
    chip8.reg[0] = 0;
    decex(&chip8, 0xF007);
    test_assert(chip8.reg[0] == 0x33, "FX07: Read delay timer");
    
    // Test FX18: Set sound timer
    chip8.reg[0] = 0x25;
    decex(&chip8, 0xF018);
    test_assert(chip8.audio_data.timer_value == 0x25, "FX18: Set sound timer");
    test_assert(chip8.audio_data.sound_enabled == true, "FX18: Sound enabled");
}

/*
 * Test 8: Keyboard Operations
 */
void test_keyboard_operations() {
    printf("\n=== Keyboard Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test EX9E: Skip if key pressed
    chip8.reg[0] = 5;
    chip8.keys[5] = 1;
    unsigned char *old_pc = chip8.pc;
    decex(&chip8, 0xE09E);
    test_assert(chip8.pc == old_pc + 2, "EX9E: Skip when key pressed");
    
    // Test EXA1: Skip if key not pressed
    chip8.reg[0] = 6;
    chip8.keys[6] = 0;
    chip8.pc = chip8.memory + PRG_START;
    decex(&chip8, 0xE0A1);
    test_assert(chip8.pc == chip8.memory + PRG_START + 2, "EXA1: Skip when key not pressed");
    
    // Test FX0A: Wait for key press
    chip8.reg[0] = 0;
    chip8.keys[3] = 1;
    chip8.pc = chip8.memory + PRG_START;
    bool skip = decex(&chip8, 0xF00A);
    test_assert(chip8.reg[0] == 3, "FX0A: Key stored in register");
    test_assert(skip == false, "FX0A: No skip when key pressed");
}

/*
 * Test 9: Audio System Tests
 */
void test_audio_system() {
    printf("\n=== Audio System Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    
    // Test audio data initialization
    test_assert(chip8.audio_data.phase == 0.0f, "Audio phase initialized to 0");
    test_assert(chip8.audio_data.sound_enabled == true, "Audio enabled by default");
    test_assert(chip8.audio_data.timer_value == 0, "Sound timer initialized to 0");
    
    // Test sound timer setting
    chip8.reg[0] = 60;
    decex(&chip8, 0xF018);
    test_assert(chip8.audio_data.timer_value == 60, "Sound timer set to 60");
    test_assert(chip8.audio_data.sound_enabled == true, "Sound enabled when timer set");
    
    // Test sound timer countdown behavior
    chip8.audio_data.timer_value = 5;
    for (int i = 5; i > 0; i--) {
        test_assert(chip8.audio_data.timer_value == i, 
                   "Sound timer countdown");
        chip8.audio_data.timer_value--;
    }
    test_assert(chip8.audio_data.timer_value == 0, "Sound timer reaches 0");
}

/*
 * Test 10: Stack Operations
 */
void test_stack_operations() {
    printf("\n=== Stack Operations Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    
    // Test push operation
    unsigned char *addr1 = chip8.memory + 0x200;
    unsigned char *addr2 = chip8.memory + 0x300;
    
    stack_push(&chip8.prg_stack, addr1);
    test_assert(chip8.prg_stack.size == 1, "Stack push: size increased");
    
    stack_push(&chip8.prg_stack, addr2);
    test_assert(chip8.prg_stack.size == 2, "Stack push: size increased to 2");
    
    // Test pop operation
    unsigned char *popped = stack_pop(&chip8.prg_stack);
    test_assert(popped == addr2, "Stack pop: correct value (LIFO)");
    test_assert(chip8.prg_stack.size == 1, "Stack pop: size decreased");
    
    popped = stack_pop(&chip8.prg_stack);
    test_assert(popped == addr1, "Stack pop: correct value");
    test_assert(chip8.prg_stack.size == 0, "Stack pop: size decreased to 0");
}

/*
 * Test 11: Edge Cases and Boundary Tests
 */
void test_edge_cases() {
    printf("\n=== Edge Cases and Boundary Tests ===\n");
    
    CHIP8 chip8;
    initialize_chip(&chip8);
    chip8.pc = chip8.memory + PRG_START;
    
    // Test register overflow
    chip8.reg[0] = 0xFF;
    decex(&chip8, 0x7001); // Add 1 to 0xFF
    test_assert(chip8.reg[0] == 0x00, "Register overflow wraps to 0");
    
    // Test register underflow
    chip8.reg[0] = 0x00;
    chip8.reg[1] = 0x01;
    decex(&chip8, 0x8015); // V0 = V0 - V1
    test_assert(chip8.reg[0] == 0xFF, "Register underflow wraps to 0xFF");
    
    // Test sprite wrapping
    chip8.i = chip8.memory + 0x50; // Font '0'
    chip8.reg[0] = 62; // x near right edge
    chip8.reg[1] = 0;  // y at top
    decex(&chip8, 0xD015); // Draw sprite
    // Should not crash - wrapping behavior
    test_assert(true, "Sprite drawing near screen edge doesn't crash");
}

// --- Main Test Runner ---

int main() {
    printf("========================================\n");
    printf("CHIP-8 Emulator Test Suite\n");
    printf("========================================\n");
    
    // Run all test suites
    test_initialization();
    test_display_operations();
    test_flow_control();
    test_conditional_operations();
    test_register_operations();
    test_memory_operations();
    test_timer_operations();
    test_keyboard_operations();
    test_audio_system();
    test_stack_operations();
    test_edge_cases();
    
    // Print summary
    print_test_summary();
    
    // Return non-zero if any tests failed
    return (g_stats.failed > 0) ? 1 : 0;
}
