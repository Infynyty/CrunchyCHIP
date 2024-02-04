#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "CHIP-8.h"

#define MAX_PATH_LEN 4096

int main(int argc, char** argv) {
    if (argc != 2 || strlen(argv[1]) > MAX_PATH_LEN) {
        printf("Usage:\n\ncrispychip <Path to CHIP-8 executable>\n\n");
        exit(EXIT_FAILURE);
    }
    FILE* file = fopen(argv[1], "rb");
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
