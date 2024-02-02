#include "CHIP-8.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdbool.h>


#define FONT_MEMORY_OFFSET 0x50
#define FONT_SIZE 16
#define BYTES_PER_FONT_CHARACTER 5

static uint8_t font[] = {
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


struct MachineState* create_machine() {
    struct RegisterFile* registerFile = malloc(sizeof(struct RegisterFile));
    struct Memory* memory = malloc(sizeof(struct Memory));
    struct Screen* screen = malloc(sizeof(struct Screen));
    struct MachineState* state = malloc(sizeof(struct MachineState));
    if (!registerFile || !memory || !screen || !state) {
        exit(EXIT_FAILURE);
    }

    // initialize memory
    memory->stack_base_pointer = (uint16_t**) (memory->mem + STACK_OFFSET);
    memory->stack_pointer = memory->stack_base_pointer;

    // initialize screen
    SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    screen->renderer = renderer;
    screen->event = event;
    screen->window = window;

    state->rf = registerFile;
    state->mem = memory;
    state->screen = screen;
    return state;
}

#define CLEAR_OR_RETURN_NIBBLE  0x0
#define CLEAR_SCREEN_NIBBLE 0x0
#define RETURN_NIBBLE       0xe
#define JUMP_NIBBLE             0x1
#define CALL_SUBROUTINE_NIBBLE  0x2
#define SKIP_IF_EQ_IMM          0x3
#define SKIP_IF_NEQ_IMM         0x4
#define SKIP_IF_EQ_REG          0x5
#define SET_REGISTER_NIBBLE     0x6
#define ADD_IMMEDIATE_NIBBLE    0x7

#define ARITH_LOGIC_NIBBLE      0x8
#define LOAD_NIBBLE             0x0
#define OR_NIBBLE               0x1
#define AND_NIBBLE              0x2
#define XOR_NIBBLE              0x3
#define ADD_NIBBLE              0x4
#define SUBTRACT_NIBBLE         0x5
#define RIGHT_SHIFT_NIBBLE      0x6
#define SUBTRACT_N_NIBBLE       0x7
#define LEFT_SHIFT_NIBBLE       0xe


#define SKIP_IF_NE_NIBBLE       0x9
#define SET_INDEX_REG_NIBBLE    0xa
#define JUMP_OFFSET_NIBBLE      0xb
#define GENERATE_RANDOM_NIBBLE  0xc
#define DRAW_NIBBLE             0xd

#define MASK_NIBBLES(instruction, n) (instruction & (0xffffU >> n * 4))
#define GET_NIBBLE(instruction, n) ((instruction & (0xf000U >> n * 4)) >> (3 - n) * 4)

void prepare_memory(struct MachineState* state, uint8_t* binary, size_t binary_size) {
    if (binary_size > MAX_BINARY_LENGTH) {
        fprintf(stderr, "Binary is too long: %zu bytes", binary_size);
        exit(EXIT_FAILURE);
    }
    memmove(state->mem->mem + PROGRAM_OFFSET, binary, binary_size);

    //move font data into memory
    memmove(state->mem->mem + FONT_MEMORY_OFFSET, font, FONT_SIZE * BYTES_PER_FONT_CHARACTER);
    state->rf->pc = (uint16_t*) &(state->mem->mem[PROGRAM_OFFSET]);
}

bool check_program_quit(struct MachineState* state) {
    while (SDL_PollEvent(&(state->screen->event))) {
        switch (state->screen->event.type) {
            case SDL_KEYDOWN: {
                if (state->screen->event.key.keysym.sym == SDLK_ESCAPE) {
                    return true;
                }
                break;
            }
            case SDL_QUIT:
            {
                return true;
            }
        }
    }
    return false;
}

void read_sprite_data(struct MachineState* state, uint16_t instruction) {
    SDL_RenderClear(state->screen->renderer);
    SDL_RenderPresent(state->screen->renderer);
    state->rf->d_reg[0xf] = 1;
    uint8_t x_start = state->rf->d_reg[GET_NIBBLE(instruction, 1)] % SCREEN_WIDTH;
    uint8_t y_start = state->rf->d_reg[GET_NIBBLE(instruction, 2)] % SCREEN_HEIGHT;
    uint8_t rows = GET_NIBBLE(instruction, 3);
    for (int row = 0; row < rows; ++row) {
        if (row + y_start >= SCREEN_HEIGHT) break;
        uint8_t pixels = state->mem->mem[state->rf->I + row];
        for (int column = 0; column < 8; ++column) {
            if (column + x_start >= SCREEN_WIDTH) break;
            bool pixel = (pixels & (0xff >> column)) >> (7 - column);
            if (pixel) {
                state->screen->pixels[row + y_start][column + x_start] = !state->screen->pixels[row + y_start][column + x_start];
            }
        }
    }
}

void render_image(struct MachineState* state) {
    for (int i = 0; i < SCREEN_HEIGHT / PIXEL_SIZE; ++i) {
        for (int j = 0; j < SCREEN_WIDTH / PIXEL_SIZE; ++j) {
            if (!state->screen->pixels[i][j]) continue;
            SDL_Rect rect;
            rect.x = j * PIXEL_SIZE;
            rect.y = i * PIXEL_SIZE;
            rect.w = PIXEL_SIZE;
            rect.h = PIXEL_SIZE;
            SDL_SetRenderDrawColor(state->screen->renderer, 0, 255, 0, 255);
            SDL_RenderFillRect(state->screen->renderer, &rect);
        }
    }
    SDL_SetRenderDrawColor(state->screen->renderer, 0, 0, 0, 255);
    SDL_RenderPresent(state->screen->renderer);
}

void stack_push_pc(struct MachineState* state) {
    if (state->mem->stack_pointer + sizeof(state->rf->pc) > state->mem->stack_base_pointer + STACK_SIZE) {
        fprintf(stderr, "Stack overflow!");
        exit(EXIT_FAILURE);
    }
    *(state->mem->stack_pointer) = state->rf->pc + 1;
    state->mem->stack_pointer++;
}

uint16_t* stack_pop_pc(struct MachineState* state) {
    if (state->mem->stack_pointer == state->mem->stack_base_pointer) {
        fprintf(stderr, "Cannot pop from empty stack!");
        exit(EXIT_FAILURE);
    }
    state->mem->stack_pointer--;
    uint16_t* result = *(state->mem->stack_pointer);
    return result;
}

void execute_instruction_cycle(struct MachineState* state) {
    uint16_t instruction = *((uint16_t*) (state->rf->pc));
    // instructions are stored in big endian format
    instruction = be16toh(instruction);

    uint8_t first_nibble = GET_NIBBLE(instruction, 0);
    bool increment_pc = true;
    switch (first_nibble) {
        case CLEAR_OR_RETURN_NIBBLE: {
            if (GET_NIBBLE(instruction, 3) == CLEAR_SCREEN_NIBBLE) {
                for (int i = 0; i < SCREEN_HEIGHT / PIXEL_SIZE; ++i) {
                    for (int j = 0; j < SCREEN_WIDTH / PIXEL_SIZE; ++j) {
                        state->screen->pixels[i][j] = 0;
                    }
                }
                SDL_RenderClear(state->screen->renderer);
                SDL_RenderPresent(state->screen->renderer);
                printf("Cleared screen!\n");
                break;
            } else if (GET_NIBBLE(instruction, 3) == RETURN_NIBBLE){
                increment_pc = false;
                uint16_t* return_address = stack_pop_pc(state);
                state->rf->pc = return_address;
                break;
            }
        }
        case JUMP_NIBBLE: {
            increment_pc = false;
            uint16_t jump_address = MASK_NIBBLES(instruction, 1);
            //printf("Jumped to %x!\n", jump_address);
            state->rf->pc = (uint16_t*) (state->mem->mem + jump_address);
            break;
        }
        case CALL_SUBROUTINE_NIBBLE: {
            increment_pc = false;
            uint16_t jump_address = MASK_NIBBLES(instruction, 1);
            stack_push_pc(state);
            state->rf->pc = (uint16_t*) (state->mem->mem + jump_address);
            break;
        }
        case SKIP_IF_EQ_IMM: {
            uint8_t reg = GET_NIBBLE(instruction, 1);
            uint8_t value = MASK_NIBBLES(instruction, 2);
            if (state->rf->d_reg[reg] == value) {
                state->rf->pc++;
            }
            break;
        }
        case SKIP_IF_NEQ_IMM: {
            uint8_t reg = GET_NIBBLE(instruction, 1);
            uint8_t value = MASK_NIBBLES(instruction, 2);
            if (state->rf->d_reg[reg] != value) {
                state->rf->pc++;
            }
            break;
        }
        case SKIP_IF_EQ_REG: {
            uint8_t x = GET_NIBBLE(instruction, 1);
            uint8_t y = GET_NIBBLE(instruction, 2);
            if (state->rf->d_reg[x] != state->rf->d_reg[y]) {
                state->rf->pc++;
            }
            break;
        }
        case SET_REGISTER_NIBBLE: {
            uint8_t reg = GET_NIBBLE(instruction, 1);
            uint8_t value = MASK_NIBBLES(instruction, 2);
            state->rf->d_reg[reg] = value;
            printf("Set register %x to %u!\n", reg, value);
            break;
        }
        case ADD_IMMEDIATE_NIBBLE: {
            uint8_t reg = GET_NIBBLE(instruction, 1);
            uint8_t value = MASK_NIBBLES(instruction, 2);
            state->rf->d_reg[reg] += value;
            printf("Add %d to register %x: currently: %d!\n", value, reg, state->rf->d_reg[reg]);
            break;
        }
        case ARITH_LOGIC_NIBBLE: {
            uint8_t operation = GET_NIBBLE(instruction, 3);
            uint8_t x = GET_NIBBLE(instruction, 1);
            uint8_t y = GET_NIBBLE(instruction, 2);
            switch (operation) {
                case LOAD_NIBBLE: {
                    state->rf->d_reg[x] = state->rf->d_reg[y];
                    break;
                }
                case OR_NIBBLE: {
                    state->rf->d_reg[x] = state->rf->d_reg[x] | state->rf->d_reg[y];
                    break;
                }
                case AND_NIBBLE: {
                    state->rf->d_reg[x] = state->rf->d_reg[x] & state->rf->d_reg[y];
                    break;
                }
                case XOR_NIBBLE: {
                    state->rf->d_reg[x] = state->rf->d_reg[x] ^ state->rf->d_reg[y];
                    break;
                }
                case ADD_NIBBLE: {
                    uint16_t res = state->rf->d_reg[x] + state->rf->d_reg[y];
                    if (res > 0xff) {
                        state->rf->d_reg[0xf] = 1;
                    } else {
                        state->rf->d_reg[0xf] = 0;
                    }
                    state->rf->d_reg[x] = (uint8_t) res;
                    break;
                }
                case SUBTRACT_NIBBLE: {
                    if (state->rf->d_reg[x] > state->rf->d_reg[y]) {
                        state->rf->d_reg[0xf] = 1;
                    } else {
                        state->rf->d_reg[0xf] = 0;
                    }
                    state->rf->d_reg[x] = state->rf->d_reg[x] - state->rf->d_reg[y];
                    break;
                }
                case RIGHT_SHIFT_NIBBLE: {
                    if (state->rf->d_reg[x] & 0x1) {
                        state->rf->d_reg[0xf] = 1;
                    } else {
                        state->rf->d_reg[0xf] = 0;
                    }
                    state->rf->d_reg[x] >>= 1;
                    break;
                }
                case SUBTRACT_N_NIBBLE: {
                    if (state->rf->d_reg[y] > state->rf->d_reg[x]) {
                        state->rf->d_reg[0xf] = 1;
                    } else {
                        state->rf->d_reg[0xf] = 0;
                    }
                    state->rf->d_reg[x] = state->rf->d_reg[y] - state->rf->d_reg[x];
                    break;
                }
                case LEFT_SHIFT_NIBBLE: {
                    if ((state->rf->d_reg[x] >> 7) & 0x1) {
                        state->rf->d_reg[0xf] = 1;
                    } else {
                        state->rf->d_reg[0xf] = 0;
                    }
                    state->rf->d_reg[x] <<= 1;
                    break;
                }
                default:
                    fprintf(stderr, "Instruction 0x%x not implemented!\n", instruction);
            }
            break;
        }
        case SKIP_IF_NE_NIBBLE: {
            uint8_t x = GET_NIBBLE(instruction, 1);
            uint8_t y = GET_NIBBLE(instruction, 2);
            if (state->rf->d_reg[x] != state->rf->d_reg[y]) {
                state->rf->pc++;
            }
            break;
        }
        case SET_INDEX_REG_NIBBLE: {
            uint16_t address = MASK_NIBBLES(instruction, 1);
            state->rf->I = address;
            printf("Set index register to %d!\n", state->rf->I);
            break;
        }
        case JUMP_OFFSET_NIBBLE: {
            increment_pc = false;
            uint16_t jump_address = MASK_NIBBLES(instruction, 1) + state->rf->d_reg[0];
            //printf("Jumped to %x!\n", jump_address);
            state->rf->pc = (uint16_t*) (state->mem->mem + jump_address);
            break;
        }
        case GENERATE_RANDOM_NIBBLE: {
            uint8_t x = state->rf->d_reg[GET_NIBBLE(instruction, 1)];
            uint8_t kk = MASK_NIBBLES(instruction, 2);
            int random = rand() % 256;
            state->rf->d_reg[x] = random & kk;
            break;
        }
        case DRAW_NIBBLE: {
            read_sprite_data(state, instruction);
            render_image(state);
            printf("Drew!\n");
            break;
        }
        default:
            fprintf(stderr, "Instruction 0x%x not implemented!\n", instruction);
    }
    if (increment_pc) {
        state->rf->pc++;
    }
}


void run_machine(struct MachineState* state, uint8_t* binary, size_t binary_size) {
    prepare_memory(state, binary, binary_size);
    while (state->rf->pc >= (uint16_t*) &state->mem->mem[PROGRAM_OFFSET] && state->rf->pc < (uint16_t*) &state->mem->mem[PROGRAM_OFFSET + MAX_BINARY_LENGTH]) {
        if (check_program_quit(state)) break;
        execute_instruction_cycle(state);
    }
}

void delete_machine(struct MachineState** state) {
    if (!(*state)) return;

    SDL_DestroyRenderer((*state)->screen->renderer);
    SDL_DestroyWindow((*state)->screen->window);
    SDL_Quit();

    if ((*state)->mem) free((*state)->mem);
    if ((*state)->rf) free((*state)->rf);
    if ((*state)->screen) free((*state)->screen);
    (*state)->rf = NULL;
    (*state)->mem = NULL;
    (*state)->screen = NULL;
    free((*state));
    *state = NULL;
}



