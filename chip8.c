#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>


// --- Constants for CHIP-8 Struct ---
#define MEM_SIZE 4096 // 4 KB
#define REG_SIZE 16
#define PRG_START 512 // 0x200 Start of the program counter
#define STACK_SIZE 16
#define KEYBOARD_SIZE 16

// --- Constants for Audio Generation ---
#define SAMPLE_RATE 44100
#define TONE_FREQUENCY 440.0f
#define AMPLITUDE 16000

// --- Constants for CHIP-8 Display ---
#define DISPLAY_H 32
#define DISPLAY_W 64
#define SCALE_FACTOR 10

// Macro to safely convert small integers to pointers
#define GET_NIBBLE(value, index) (((opcode >> ((4 - index) * 4)) & 0x000F))

// Constant array containing the scancodes for the keyboard keys.
// Here we use scancode to take into account the possibility of having
// different layouts from the standard QWERTY.
// This way even someone with a different keyboard layout can play using
// this interpreter without having to touch the source code.
const SDL_Scancode KEYMAP[16] = {
    SDL_SCANCODE_X,    // Key 0
    SDL_SCANCODE_1,    // Key 1
    SDL_SCANCODE_2,    // Key 2
    SDL_SCANCODE_3,    // Key 3
    SDL_SCANCODE_Q,    // Key 4
    SDL_SCANCODE_W,    // Key 5
    SDL_SCANCODE_E,    // Key 6
    SDL_SCANCODE_A,    // Key 7
    SDL_SCANCODE_S,    // Key 8
    SDL_SCANCODE_D,    // Key 9
    SDL_SCANCODE_Z,    // Key A
    SDL_SCANCODE_C,    // Key B
    SDL_SCANCODE_4,    // Key C
    SDL_SCANCODE_R,    // Key D
    SDL_SCANCODE_F,    // Key E
    SDL_SCANCODE_V     // Key F
};

// Struct for managing audio within the CHIP-8.
// Audio is managed using the SDL2 audio API and the emulator emits a sound
// every time the sound timer is greater than 0.
typedef struct {
    float phase;
    bool sound_enabled;
    int timer_value;
    bool audio_working;
} AudioData;


// Audio callback function for SDL2 real-time audio generation.
//
// This function is automatically called by SDL2's audio subsystem whenever
// it needs more audio data to send to the speakers/headphones. The callback
// runs in a separate audio thread.
//
// The function generates a continuous sine wave tone at 440 Hz (A4 musical note)
// when the CHIP-8 sound timer is active. It uses phase accumulation to create
// smooth, mathematically accurate waveforms.
//
// CHIP-8 Audio Behavior:
// - Sound plays continuously while sound_timer > 0
// - Sound stops immediately when sound_timer reaches 0
// - Only one tone frequency is supported (authentic CHIP-8 behavior)
void audio_callback(void *userdata, Uint8 *stream, int len) {
    // Cast the generic userdata pointer back to our specific AudioData structure
    // This contains all the state information needed for audio generation.
    AudioData *audio = (AudioData*) userdata;

    // Cast the byte stream to 16-bit signed integer samples
    // SDL provides a generic byte buffer, but we're using AUDIO_S16SYS format
    // which means 16-bit signed integers in system byte order.
    Sint16 *buffer = (Sint16*) stream;

    // Calculate the number of audio samples to generate
    // Since each sample is 2 bytes (16-bit), we divide buffer length by 2
    // Typical values: len=2048 bytes → num_samples=1024 samples
    int num_samples = len / sizeof(Sint16);

    // Initialize the entire buffer to silence (zero values)
    // This ensures clean audio output and prevents garbage data from being heard
    // Always clear the buffer first, then selectively add audio content
    memset(stream, 0, len);

    // Set flag to indicate that the audio callback is being executed
    // This is useful for debugging and verifying that audio system is working
    // The main thread can check this flag to confirm audio thread activity
    audio->audio_working = true;

    // Generate audio samples for the entire buffer
    // Each iteration produces one sample point in the continuous waveform
    for (int i = 0; i < num_samples; i++) {
        // Initialize sample value to silence
        // Will remain zero if no sound should be generated
        Sint16 sample = 0;

        // Check if sound should be generated based on CHIP-8 sound timer state
        // Sound plays only when both conditions are met:
        // 1. sound_enabled flag is true (audio system is active)
        // 2. timer_value > 0 (CHIP-8 sound timer hasn't expired)
        if (audio->sound_enabled && audio->timer_value > 0) {

            // Generate sine wave sample using current phase (the position on
            // the sin function that we want to sample).
            // Mathematical formula: amplitude × sin(phase)
            // - sinf(phase) returns value between -1.0 and +1.0
            // - AMPLITUDE (16000) scales to audible volume level
            // - Sint16 cast converts float to 16-bit integer format
            sample = (Sint16)(AMPLITUDE * sinf(audio->phase));

            // Advance phase for next sample (phase accumulation)
            // This is the core of digital audio synthesis:
            //
            // Formula breakdown:
            // - TONE_FREQUENCY (440 Hz): desired frequency in cycles per second
            // - SAMPLE_RATE (44100 Hz): how many samples we generate per second
            // - 2π: one complete sine wave cycle in radians
            //
            // Phase increment = (2π × frequency) / sample_rate
            // Example: (2π × 440) / 44100 ≈ 0.0628 radians per sample
            //
            // After 44100/440 ≈ 100 samples, phase accumulates to 2π (full cycle)
            // This creates exactly 440 complete cycles per second = 440 Hz tone
            audio->phase += 2.0f * M_PI * TONE_FREQUENCY / SAMPLE_RATE;

            // Prevent phase accumulation overflow by wrapping at 2π
            // Since sin(x) = sin(x + 2π), we can safely subtract 2π
            // This keeps the phase value in a reasonable range and prevents
            // floating-point precision issues that could cause audio artifacts
            if (audio->phase >= 2.0f * M_PI) {
                audio->phase -= 2.0f * M_PI;
            }
        } else {
            // Reset phase when not generating sound
            // This ensures clean audio startup when sound begins again
            // Without this reset, there could be audio pops or clicks
            // when transitioning from silence to sound
            audio->phase = 0.0f;
        }

        // Store the generated sample in the output buffer
        // SDL will automatically send this data to the audio hardware
        // The buffer represents a small time slice of continuous audio
        buffer[i] = sample;
    }

    // Function completes and returns control to SDL's audio system
    // SDL immediately starts playing the samples we just generated
    // Meanwhile, SDL prepares to call this function again for the next buffer
}

// Struct that defines a simple stack data structure.
// It has been designed to be extremely simple and just with a push and pop
// methods.
//
// It is used for managing the simple function calls stack of the CHIP-8.
typedef struct {
    int size; // Number of elements currently in the stack.
    unsigned char *data[STACK_SIZE];
} Stack;


// Pop method for the stack. It removes the element at the top of the stack
// and returns it. It also performs a check to make sure there is an element and
// reduces size by one when popping.
unsigned char *stack_pop(Stack *s) {
    if (s->size >= 1) return s->data[--s->size];

    perror("The stack is empty.");
    return NULL;
}


// Push method for the stack. It adds an element at the top of the stack
// and increases the stack size.
// It also performs a check to make sure there is an element.
void stack_push(Stack *s, unsigned char *val) {
    if (s->size <= 15) s->data[s->size++] = val;
    else perror("The stack is full.");
}


// CHIP-8 Main Struct
//
// It defines all basic elements of the CHIP-8 interpreter:
// - Memory: 4096 kb of addressable memory
// - Display: a 32 x 64 pixels display
// - Program Stack: Stack for containing all function calls
// - Program counter (PC): Pointer to the next instruction to execute
// - I pointer to memory: Pointer to a specific location of the memory
// - Delay timer: Timer that is decreased 60 times per second
// - Sound timer: It is decreased as the delay timer. A sound is emitted when above 0
// - Registers: Array containing all registers values
// - Keyboard keys: Array containing keyboard keys value
//
// This struct is used as a the main access point of the CHIP-8.
// An instance of this struct is passed as the first argument in most of
// the interpreter functions.
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


// Initialization function.
//
// This function initializes the CHIP-8 struct values to their defaults
// to avoid possible errors due to a missing initialization.
void initialize_chip(CHIP8 *chip8) {
    memset(chip8->memory, 0, MEM_SIZE);
    memset(chip8->display, 0, DISPLAY_H * DISPLAY_W * sizeof(unsigned int));

    chip8->pc = NULL;
    chip8->i = NULL;
    chip8->prg_stack.size = 0;
    memset(chip8->prg_stack.data, 0, STACK_SIZE);

    chip8->d_timer = 0;
    chip8->audio_data.phase = 0.0f;
    chip8->audio_data.sound_enabled = true;  // Enable sound by default
    chip8->audio_data.timer_value = 0;
    chip8->audio_data.audio_working = false;

    memset(chip8->reg, 0, REG_SIZE);
    memset(chip8->keys, 0, KEYBOARD_SIZE);

    chip8->draw_flag = true;
}


// Loading fonts into memory.
//
// Since the first 200 bytes in the CHIP-8 memory has to be free,
// it is a good practice to use the initial space for storing the fonts.
//
// The font we are loading here is the standard one
// that is found usually on online guides/tutorials.
void load_font(CHIP8 *chip8) {
    // Here we define an array for containing all the fonts for simplicity
    // Each character is expressed through a sequence of 5 hexadecimal numbers.
    // Each number is a byte (8-bit number) that represents one row
    // of the font number on the display.
    //
    // For example the number 0 is represented in the following way:
    //
    // 0xF0 --> 1111 0000
    // 0x90 --> 1001 0000
    // 0x90 --> 1001 0000
    // 0x90 --> 1001 0000
    // 0xF0 --> 1111 0000
    //
    // As you can see we are forming the number with the first half of bits.
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


// Function for loading the program into memory.
//
// It simply reads the file from memory and copy it into the CHIP-8 memory
// array starting from position 200, which is the standard starting position
// for CHIP-8 programs.
//
// To be more efficient here we are reading and copying into memory using
// two unique operations. This way we reduce memory read and write overhead and
// perform a really fast loading of the program.
void load_program(CHIP8 *chip8, char *filename) {
    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("Error in opening the file.");
    }

    // Reading file content.
    //
    // Specifically we use fseek to go to the end of file. This way we get
    // the exact number of bytes that we have to read. Then we use rewind
    // function to make our pointer go back to the file beginning.
    fseek(file, 0, SEEK_END);
    long size = ftell(file); // Retrieves file size.
    rewind(file);
    // Here we use the file size computed to perform a single read from memory.
    // We also make sure that the size of the program is not bigger than
    // the available space.
    if (size < (MEM_SIZE - PRG_START)) {
        fread(chip8->memory + PRG_START, 1, size, file);
    }

    fclose(file);
}


// Clear display function.
// This function simply make the display all black by setting all display
// pixels values to 0. Here I have used the hexadecimal value to highlight
// the fact that we are using SDL RGBA format for colors,
// where the each byte corresponds to one channel:
//
//    R  G  B  A
// 0x 00 00 00 00
void clear_display(unsigned int display[], int size) {
    for (int i = 0; i < size; i++) {
        display[i] = 0x00000000; // RGBA Format for black color.
    }
}


// Function for drawing sprites on the CHIP-8 display.
//
// This function implements the DXYN instruction which draws an 8-pixel wide sprite
// at coordinates (x, y) with a specified height. The sprite data is read from memory
// starting at the address stored in the I register.
//
// CHIP-8 sprites are always 8 pixels wide but can be 1-15 pixels tall.
// Each byte in memory represents one row of 8 pixels, where each bit corresponds
// to one pixel (1 = white/on, 0 = black/off).
//
// The function uses XOR logic for drawing, which means:
// - Drawing on a black pixel turns it white
// - Drawing on a white pixel turns it black (collision detection)
// - Drawing the same sprite twice at the same location erases it
//
// Collision detection: If any pixel is turned OFF during drawing (white to black),
// the VF register is set to 1, otherwise it's set to 0.
//
// Screen wrapping: Sprites that go beyond screen boundaries wrap around to the
// opposite edge, which is standard CHIP-8 behavior.
void draw(CHIP8 *chip8, unsigned char x, unsigned char y, unsigned char height) {
    // Pointer to sprite data in memory starting at address I
    unsigned char *val = chip8->i;

    // Initialize collision flag to 0 (no collision detected yet)
    // VF register is used as a flag to indicate if any pixels were erased
    chip8->reg[0xF] = 0;

    // Handle screen wrapping by using modulo operation
    // This ensures coordinates wrap around if they exceed screen boundaries
    // For example, if x=70 and DISPLAY_W=64, wrap_x becomes 6
    int wrap_x = x % DISPLAY_W;
    int wrap_y = y % DISPLAY_H;

    // Position index for the linear display buffer
    // The 2D display is stored as a 1D array, so we need to calculate positions
    int pos = 0;

    // CHIP-8 sprites are always 8 pixels wide
    const int width = 8;

    // Iterate through each row of the sprite
    // We draw from top to bottom, and stop if we hit the bottom screen edge
    for (int row = 0; row < height && row + wrap_y < DISPLAY_H; row++) {

        // Iterate through each column (bit) in the current row
        // We draw from left to right, and stop if we hit the right screen edge
        for (int col = 0; col < width && col + wrap_x < DISPLAY_W; col++) {

            // Check if the current bit (pixel) in the sprite data is set to 1
            // We use bit masking: 0b10000000 (0x80) shifted right by col positions
            // This isolates each bit from left to right:
            // col=0: 0b10000000 (checks leftmost bit)
            // col=1: 0b01000000 (checks second bit)
            // col=2: 0b00100000 (checks third bit)
            // ... and so on
            if (*val & (0b10000000 >> col)) {

                // Calculate the linear position in the display buffer
                // Formula: row_offset + column_offset
                // row_offset = (wrap_y + row) * DISPLAY_W
                // column_offset = (wrap_x + col)
                pos = DISPLAY_W * (wrap_y + row) + (wrap_x + col);

                // XOR the pixel with 0xFFFFFFFF (all white in RGBA format)
                // This implements the toggle behavior:
                // - If pixel was 0x00000000 (black), it becomes 0xFFFFFFFF (white)
                // - If pixel was 0xFFFFFFFF (white), it becomes 0x00000000 (black)
                chip8->display[pos] ^= 0xFFFFFFFF;

                // Collision detection: check if a pixel was turned OFF (erased)
                // If the pixel is now black (0), it means we overwrote a white pixel
                // This sets the VF flag to indicate a collision occurred
                if (!chip8->display[pos]) chip8->reg[0xF] = 1;
            }
        }

        // Move to the next byte in memory for the next row of sprite data
        // Each byte represents one horizontal row of 8 pixels
        val++;
    }
}

// Function for decoding and executing CHIP-8 instructions.
//
// Here I have decided to merge the decode and exeute passes into a single one,
// as the decode pass is in this case extremely simple and uses only a bunch
// of switch cases. Into each case I then execute the specific command.
bool decex(CHIP8 *chip8, uint16_t opcode) {
    // Here we retrieve the operation code from op code.
    // The real opcode lies in the first four bits of the opcode, therefore
    // to extract it we simply perform a byte shift operation to the right
    //
    // e.g. opcode = 0x10101010
    unsigned char op = opcode >> 12;
    unsigned char sub_op = 0;

    // Here x and y represent values that might be used when executing
    // operations as the register indices.
    unsigned int x = 0;
    unsigned int y = 0;

    x = GET_NIBBLE(opcode, 2);
    y = GET_NIBBLE(opcode, 3);

    // Boolean flag used for instructions in which we do not have to
    // increase the program counter.
    bool skip_pc_incr = false;

    switch (op) {
        case 0x0:
            if (opcode & 0x000F) {
                // Return from subroutine (00EE)
                printf("00EE: Return from subroutine\n");
                chip8->pc = stack_pop(&(chip8->prg_stack));
            } else {
                // Clear display (00E0)
                printf("00E0: Clear display\n");
                clear_display(chip8->display, DISPLAY_H * DISPLAY_W);
                chip8->draw_flag = true;
            }
            break;

        case 0x1:
            // Jump to address NNN (1NNN)
            printf("1%03X: Jump to address 0x%03X\n", opcode & 0x0FFF, opcode & 0x0FFF);
            chip8->pc = chip8->memory + (opcode & 0x0FFF);
            skip_pc_incr = true;
            break;

        case 0x2:
            // Call subroutine at NNN (2NNN)
            printf("2%03X: Call subroutine at 0x%03X\n", opcode & 0x0FFF, opcode & 0x0FFF);
            stack_push(&(chip8->prg_stack), chip8->pc);
            chip8->pc = chip8->memory + (opcode & 0x0FFF);
            skip_pc_incr = true;
            break;

        case 0x3:
            // Skip next instruction if Vx == NN (3XNN)
            printf("3%X%02X: Skip if V%X (0x%02X) == 0x%02X\n",
                   x, opcode & 0x00FF, x, chip8->reg[x], opcode & 0x00FF);
            if (chip8->reg[x] == (opcode & 0x00FF)) chip8->pc += 2;
            break;

        case 0x4:
            // Skip next instruction if Vx != NN (4XNN)
            printf("4%X%02X: Skip if V%X (0x%02X) != 0x%02X\n",
                   x, opcode & 0x00FF, x, chip8->reg[x], opcode & 0x00FF);
            if (chip8->reg[x] != (opcode & 0x00FF)) chip8->pc += 2;
            break;

        case 0x5:
            // Skip next instruction if Vx == Vy (5XY0)
            printf("5%X%X0: Skip if V%X (0x%02X) == V%X (0x%02X)\n",
                   x, y, x, chip8->reg[x], y, chip8->reg[y]);
            if (chip8->reg[x] == chip8->reg[y]) chip8->pc += 2;
            break;

        case 0x6:
            // Set Vx = NN (6XNN)
            printf("6%X%02X: Set V%X = 0x%02X\n",
                   x, opcode & 0x00FF, x, opcode & 0x00FF);
            chip8->reg[x] = (opcode & 0x00FF);
            break;

        case 0x7:
            // Add NN to Vx (7XNN)
            printf("7%X%02X: Add 0x%02X to V%X (was 0x%02X, now 0x%02X)\n",
                   x, opcode & 0x00FF, opcode & 0x00FF, x,
                   chip8->reg[x], (chip8->reg[x] + (opcode & 0x00FF)) & 0xFF);
            chip8->reg[x] += (opcode & 0x00FF);
            break;

        case 0x8:
            sub_op = opcode & 0x000F;

            switch (sub_op) {
                case 0x0:
                    // Set Vx = Vy (8XY0)
                    printf("8%X%X0: Set V%X = V%X (0x%02X)\n",
                           x, y, x, y, chip8->reg[y]);
                    chip8->reg[x] = chip8->reg[y];
                    break;

                case 0x1:
                    // Set Vx = Vx OR Vy (8XY1)
                    printf("8%X%X1: Set V%X = V%X (0x%02X) OR V%X (0x%02X) = 0x%02X\n",
                           x, y, x, x, chip8->reg[x], y, chip8->reg[y],
                           chip8->reg[x] | chip8->reg[y]);
                    chip8->reg[x] |= chip8->reg[y];
                    break;

                case 0x2:
                    // Set Vx = Vx AND Vy (8XY2)
                    printf("8%X%X2: Set V%X = V%X (0x%02X) AND V%X (0x%02X) = 0x%02X\n",
                           x, y, x, x, chip8->reg[x], y, chip8->reg[y],
                           chip8->reg[x] & chip8->reg[y]);
                    chip8->reg[x] &= chip8->reg[y];
                    break;

                case 0x3:
                    // Set Vx = Vx XOR Vy (8XY3)
                    printf("8%X%X3: Set V%X = V%X (0x%02X) XOR V%X (0x%02X) = 0x%02X\n",
                           x, y, x, x, chip8->reg[x], y, chip8->reg[y],
                           chip8->reg[x] ^ chip8->reg[y]);
                    chip8->reg[x] ^= chip8->reg[y];
                    break;

                case 0x4:
                    // Add Vy to Vx, set VF = carry (8XY4)
                    printf("8%X%X4: Add V%X (0x%02X) + V%X (0x%02X)",
                           x, y, x, chip8->reg[x], y, chip8->reg[y]);
                    if (chip8->reg[x] > UCHAR_MAX - chip8->reg[y]) {
                        chip8->reg[0xF] = 1;
                        printf(" = 0x%02X (overflow, VF=1)\n",
                               (chip8->reg[x] + chip8->reg[y]) & 0xFF);
                    } else {
                        chip8->reg[0xF] = 0;
                        printf(" = 0x%02X (no overflow, VF=0)\n",
                               chip8->reg[x] + chip8->reg[y]);
                    }
                    chip8->reg[x] += chip8->reg[y];
                    break;

                case 0x5:
                    // Subtract Vy from Vx, set VF = NOT borrow (8XY5)
                    printf("8%X%X5: Subtract V%X (0x%02X) - V%X (0x%02X)",
                           x, y, x, chip8->reg[x], y, chip8->reg[y]);
                    if (chip8->reg[x] >= chip8->reg[y]) {
                        chip8->reg[0xF] = 1;
                        printf(" = 0x%02X (no borrow, VF=1)\n",
                               chip8->reg[x] - chip8->reg[y]);
                    } else {
                        chip8->reg[0xF] = 0;
                        printf(" = 0x%02X (borrow, VF=0)\n",
                               (chip8->reg[x] - chip8->reg[y]) & 0xFF);
                    }
                    chip8->reg[x] -= chip8->reg[y];
                    break;

                case 0x6:
                    // Shift Vx right by 1, VF = LSB (8XY6)
                    printf("8%X%X6: Shift V%X (0x%02X) right, LSB=%d\n",
                           x, y, x, chip8->reg[x], chip8->reg[x] & 1);
                    chip8->reg[0xF] = chip8->reg[x] & 1;  // Store LSB in VF
                    chip8->reg[x] = chip8->reg[x] >> 1;
                    break;

                case 0x7:
                    // Set Vx = Vy - Vx, set VF = NOT borrow (8XY7)
                    printf("8%X%X7: Set V%X = V%X (0x%02X) - V%X (0x%02X)",
                           x, y, x, y, chip8->reg[y], x, chip8->reg[x]);
                    if (chip8->reg[y] >= chip8->reg[x]) {
                        chip8->reg[0xF] = 1;
                        printf(" = 0x%02X (no borrow, VF=1)\n",
                               chip8->reg[y] - chip8->reg[x]);
                    } else {
                        chip8->reg[0xF] = 0;
                        printf(" = 0x%02X (borrow, VF=0)\n",
                               (chip8->reg[y] - chip8->reg[x]) & 0xFF);
                    }
                    chip8->reg[x] = chip8->reg[y] - chip8->reg[x];
                    break;

                case 0xE:
                    // Shift Vx left by 1, VF = MSB (8XYE)
                    printf("8%X%XE: Shift V%X (0x%02X) left, MSB=%d\n",
                           x, y, x, chip8->reg[x], (chip8->reg[x] & 0x80) >> 7);
                    chip8->reg[0xF] = (chip8->reg[x] & 0x80) >> 7;  // Store MSB in VF
                    chip8->reg[x] = chip8->reg[x] << 1;
                    break;
            }
            break;

        case 0x9:
            // Skip next instruction if Vx != Vy (9XY0)
            printf("9%X%X0: Skip if V%X (0x%02X) != V%X (0x%02X)\n",
                   x, y, x, chip8->reg[x], y, chip8->reg[y]);
            if (chip8->reg[x] != chip8->reg[y]) chip8->pc += 2;
            break;

        case 0xA:
            // Set I = NNN (ANNN)
            printf("A%03X: Set I = 0x%03X\n", opcode & 0x0FFF, opcode & 0x0FFF);
            chip8->i = chip8->memory + (opcode & 0x0FFF);
            break;

        case 0xB:
            // Jump to NNN + V0 (BNNN)
            printf("B%03X: Jump to 0x%03X + V0 (0x%02X) = 0x%03X\n",
                   opcode & 0x0FFF, opcode & 0x0FFF, chip8->reg[0],
                   (opcode & 0x0FFF) + chip8->reg[0]);
            chip8->pc = chip8->memory + (opcode & 0x0FFF) + chip8->reg[0];
            break;

        case 0xC:
            // Set Vx = random byte AND NN (CXNN)
            printf("C%X%02X: Setting 0x%02X to a random value\n",
                   x, opcode & 0x00FF, x);

            unsigned char random_val = rand() % 256;
            unsigned char masked_val = random_val & (opcode & 0x00FF);

            chip8->reg[x] = masked_val;
            break;

        case 0xD:
            // Draw sprite (DXYN)
            printf("D%X%X: Draw sprite at V%X\n",
                   x, y, chip8->reg[x]);

            unsigned char height = GET_NIBBLE(opcode, 4);
            draw(chip8, chip8->reg[x], chip8->reg[y], height);

            chip8->draw_flag = true;
            break;

        case 0xE:
            sub_op = opcode & 0x00FF;

            switch (sub_op) {
                case 0x9E:
                    // Skip if key Vx is pressed (EX9E)
                    printf("E%X9E: Skip if key V%X (0x%02X) is pressed\n",
                           x, x, chip8->reg[x]);
                    if (chip8->keys[chip8->reg[x]]) {
                        chip8->pc += 2;
                    }
                    break;

                case 0xA1:
                    // Skip if key Vx is not pressed (EXA1)
                    printf("E%XA1: Skip if key V%X (0x%02X) is not pressed\n",
                           x, x, chip8->reg[x]);
                    if (!chip8->keys[chip8->reg[x]]) {
                        chip8->pc += 2;
                    }
                    break;

                default:
                    printf("E%X%02X: Unknown E instruction\n", x, sub_op);
                    break;
            }
            break;

        case 0xF:
            sub_op = opcode & 0x00FF;

            switch (sub_op) {
                case 0x07:
                    // Set Vx = delay timer (FX07)
                    printf("F%X07: Set V%X = delay_timer (0x%02X)\n",
                           x, x, chip8->d_timer);
                    chip8->reg[x] = chip8->d_timer;
                    break;

                case 0x0A:
                    // Wait for key press, store in Vx (FX0A)
                    printf("F%X0A: Wait for key press, store in V%X\n", x, x);
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
                    printf("F%X15: Set delay_timer = V%X (0x%02X)\n",
                           x, x, chip8->reg[x]);
                    chip8->d_timer = chip8->reg[x];
                    break;

                case 0x18:
                    printf("F%X18: Set sound_timer = V%X (0x%02X) - AUDIO SHOULD START\n",
                        x, x, chip8->reg[x]);
                    chip8->audio_data.timer_value = chip8->reg[x];
                    // Enable sound when timer is set to a non-zero value
                    if (chip8->reg[x] > 0) {
                        chip8->audio_data.sound_enabled = true;
                        printf("  >> Audio enabled with timer value %d\n", chip8->reg[x]);
                    }
                    break;

                case 0x1E:
                    // Add Vx to I (FX1E)
                    printf("F%X1E: Add V%X (0x%02X) to I (was 0x%03lX)",
                           x, x, chip8->reg[x], (uintptr_t)chip8->i - (uintptr_t)chip8->memory);
                    // Check if adding Vx to I would exceed memory bounds
                    if ((uintptr_t)(chip8->i - chip8->memory) > MEM_SIZE - chip8->reg[x]) {
                        chip8->reg[0xF] = 1;
                        printf(" - overflow detected, VF=1\n");
                    } else {
                        chip8->reg[0xF] = 0;
                        printf(" - no overflow, VF=0\n");
                    }
                    chip8->i += chip8->reg[x];
                    break;

                case 0x29:
                    // Set I = location of sprite for digit Vx (FX29)
                    printf("F%X29: Set I = font location for digit V%X (0x%02X) = 0x%03X\n",
                           x, x, chip8->reg[x], 0x50 + (chip8->reg[x] * 5));
                    chip8->i = chip8->memory + 0x50 + (chip8->reg[x] * 5);
                    break;

                case 0x33:
                    // Store BCD of Vx at I, I+1, I+2 (FX33)
                    printf("F%X33: Store BCD of V%X (0x%02X) at I: %d %d %d\n",
                           x, x, chip8->reg[x],
                           chip8->reg[x] / 100,
                           (chip8->reg[x] / 10) % 10,
                           chip8->reg[x] % 10);
                    *(chip8->i) = chip8->reg[x] / 100;        // Hundreds
                    *(chip8->i + 1) = (chip8->reg[x] / 10) % 10;  // Tens
                    *(chip8->i + 2) = chip8->reg[x] % 10;     // Ones
                    break;

                case 0x55:
                    // Store V0 to Vx in memory starting at I (FX55)
                    printf("F%X55: Store V0 to V%X in memory starting at I\n", x, x);
                    for (unsigned int i = 0; i <= x; i++) {
                        *(chip8->i + i) = chip8->reg[i];
                        printf("  Memory[I+%d] = V%X (0x%02X)\n", i, i, chip8->reg[i]);
                    }
                    break;

                case 0x65:
                    // Load V0 to Vx from memory starting at I (FX65)
                    printf("F%X65: Load V0 to V%X from memory starting at I\n", x, x);
                    for (unsigned int i = 0; i <= x; i++) {
                        chip8->reg[i] = *(chip8->i + i);
                        printf("  V%X = Memory[I+%d] (0x%02X)\n", i, i, chip8->reg[i]);
                    }
                    break;

                default:
                    printf("F%X%02X: Unknown F instruction\n", x, sub_op);
                    break;
            }
            break;

        default:
            printf("Unknown instruction: 0x%04X\n", opcode);
            break;
    }

    return skip_pc_incr;
}


int main(int argc, char **argv) {
    CHIP8 chip8;
    initialize_chip(&chip8);
    load_font(&chip8);

    srand(time(NULL)); // Initializing random generator seed.

    // Checking if a file name is passed as an argument of the program.
    // If nothing has been given the program halts.
    if (argc > 1) {
        printf("Loading program into memory...\n");
        load_program(&chip8, argv[1]);
        printf("The program has been loaded.\n");
    } else {
        printf("You need to provide a CHIP-8 executable as argument.");
        return 1;
    }

    // Set the program counter to the starting point (position 200).
    chip8.pc = chip8.memory + PRG_START;

    // Initializing SDL video and audio.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_init error: %s/n", SDL_GetError());
        return 1;
    }

    // Creating SDL Window.
    SDL_Window *window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DISPLAY_W * SCALE_FACTOR,
        DISPLAY_H * SCALE_FACTOR,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("SDL_CreateWindow error: %s/n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Creating SDL Renderer.
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Creating SDL Texture.
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_W,
        DISPLAY_H
    );

    if (!texture) {
        printf("SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Setting up SDL Audio
    SDL_AudioSpec want, have;

    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.freq = SAMPLE_RATE;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = &chip8.audio_data;

    SDL_AudioDeviceID deviceId = SDL_OpenAudioDevice(
        NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE
    );
    SDL_PauseAudioDevice(deviceId, 0);

    clear_display(chip8.display, DISPLAY_H * DISPLAY_W);

    bool running = true;
    SDL_Event event;

    // Here we set up all the variables for managing the execution cycle
    // of the interpreter. Specifically we would like to have more or less
    // 1000 cycles per second.
    const int TICK_MS = 2; // from seconds to milliseconds
    const int TIMER_TICK_MS = 17; // 1000 / 60

    Uint32 last_tick = SDL_GetTicks();
    Uint32 cur_tick = 0;
    Uint32 last_timer_tick = SDL_GetTicks();
    Uint32 cur_timer_tick = 0;

    while (running) {
        // Here we are checking for SDL events.
        // Specifically we want to check for keydown and keyup events.
        while (SDL_PollEvent(&event)) {
            SDL_Scancode sc = event.key.keysym.scancode;

            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                for (int i = 0; i < KEYBOARD_SIZE; i++) {
                    if (sc == KEYMAP[i]) chip8.keys[i] = 1;
                }
            } else if (event.type == SDL_KEYUP) {
                for (int i = 0; i < KEYBOARD_SIZE; i++) {
                    if (sc == KEYMAP[i]) chip8.keys[i] = 0;
                }
            }
        }

        cur_tick = SDL_GetTicks();
        if (cur_tick - last_tick >= TICK_MS) {
            last_tick = cur_tick;

            // If the program counter exceeds memory the program stops.
            if (chip8.pc == (chip8.memory + MEM_SIZE) - 1) break;

            // Here we extract the opcode code from the instruction
            // that the program counter points to.
            uint16_t opcode = (*chip8.pc << 8) + *(chip8.pc + 1);

            // Performing decode/execute pass and if it returns false
            // we increase the program counter.
            if (!decex(&chip8, opcode)) {
                chip8.pc += 2;
            }

            // We have to update the texture only if the draw flag is enabled.
            // This happens only when we draw something new on the screen and
            // it is necessary as, otherwise, we would incur into scattering.
            if (chip8.draw_flag) {
                // Update texture with display buffer
                SDL_UpdateTexture(texture, NULL, chip8.display,
                                 DISPLAY_W * sizeof(unsigned int));

                // Clear renderer
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);

                // Render texture to screen
                SDL_RenderCopy(renderer, texture, NULL, NULL);

                // Present to screen
                SDL_RenderPresent(renderer);

                chip8.draw_flag = false;
            }
        }

        // Here we manage delay and sound timers. Specifically we decrease their
        // values 60 times per second.
        //
        // If the sound timer is above 0 we emit a sound.
        cur_timer_tick = SDL_GetTicks();
        if (cur_timer_tick - last_timer_tick >= TIMER_TICK_MS) {
            last_timer_tick = cur_timer_tick;

            if (chip8.d_timer > 0) chip8.d_timer--;

            // Audio timer management
            static int last_audio_timer = -1;
            if (chip8.audio_data.timer_value > 0) {
                chip8.audio_data.timer_value--;
                if (chip8.audio_data.timer_value != last_audio_timer) {
                    printf("Audio timer: %d (enabled: %s, working: %s)\n",
                           chip8.audio_data.timer_value,
                           chip8.audio_data.sound_enabled ? "YES" : "NO",
                           chip8.audio_data.audio_working ? "YES" : "NO");
                    last_audio_timer = chip8.audio_data.timer_value;
                }
            } else if (last_audio_timer != 0) {
                printf("Audio timer reached 0 - sound should stop\n");
                last_audio_timer = 0;
            }
        }
    }

    // Cleanup
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_CloseAudioDevice(deviceId);

    SDL_Quit();

    return 0;
}
