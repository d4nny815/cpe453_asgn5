#include "min_common.h"
#define DEFAULT_IMAGE ("HardDisk")

// int parse_args();
// void read_fs();
// void read_block();

int main(int argc, char** argv) {
    // read FS
    uint8_t block[BLOCK_SIZE];
    // char filename[100];
    FILE* fs = fopen(DEFAULT_IMAGE, "r");

    if (!fs) {
        fprintf(stderr, "HELP ME\n");
        exit(EXIT_FAILURE);
    }

    size_t bytes = fread(block, 1, BLOCK_SIZE, fs);
    printf("read %zu bytes\n", bytes);
    // for (int i = 0; i < (bytes / 32); i++) {
        // for (int j = 0; j < 32; j++) {
            // int ind = i * 32 + j;
            // printf("%02x ", block[ind]);
        // }
        // printf("\n");
    // }

    PartitionTableEntry_t* p_part = (PartitionTableEntry_t*)(block + PART_TBL_OFFSET); 
    printf("%p\n", p_part);
    printf("%x\n", p_part->bootind);
    printf("%x\n", p_part->type);

    printf("\n%02x, %02x\n", block[510], block[511]);

    return 0;
}

// void read_fs() {
// 
    // return;
// }