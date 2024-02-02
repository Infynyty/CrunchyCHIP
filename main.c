#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "CHIP-8.h"


int main() {
    FILE* file = fopen("../test_opcode.ch8", "rb");
    if (file == NULL) {
        fprintf(stderr, "File not found.");
        exit(EXIT_FAILURE);
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    uint8_t binary[size];
    fread(binary, sizeof(uint8_t), size, file);

    struct MachineState* state = create_machine();
    run_machine(state, binary, size);
    delete_machine(&state);
    return 0;
}
