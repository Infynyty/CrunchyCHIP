#ifndef CHIP_8_CHIP_8_H
#define CHIP_8_CHIP_8_H

#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>

struct RegisterFile {
    uint8_t d_reg[16]; // data registers V0-VF
    uint16_t* pc; // program counter
    uint16_t I; // index register
    uint8_t delay_timer;
    uint8_t sound_timer;
};

#define STACK_SIZE 48
#define MEMORY_SIZE 4096
#define PROGRAM_OFFSET 0x200
#define STACK_OFFSET 0xEA0
#define DISPLAY_REFRESH_OFFSET 0xF00
#define MAX_BINARY_LENGTH (MEMORY_SIZE - PROGRAM_OFFSET - (MEMORY_SIZE - STACK_OFFSET))

struct Memory {
    uint8_t mem[MEMORY_SIZE];
    uint16_t** stack_base_pointer;
    uint16_t** stack_pointer;
};

#define PIXEL_SIZE 16
#define SCREEN_WIDTH (64 * PIXEL_SIZE)
#define SCREEN_HEIGHT (32 * PIXEL_SIZE)

struct Screen {
    uint8_t pixels[SCREEN_HEIGHT / PIXEL_SIZE][SCREEN_WIDTH / PIXEL_SIZE];
    SDL_Event event;
    SDL_Renderer* renderer;
    SDL_Window* window;
};

struct MachineState {
    struct RegisterFile* rf; // register file
    struct Memory* mem; // mem
    struct Screen* screen;
};

extern struct MachineState* create_machine();
extern void run_machine(struct MachineState* state, uint8_t* binary, size_t binary_size);
extern void delete_machine(struct MachineState** state);

#endif //CHIP_8_CHIP_8_H
