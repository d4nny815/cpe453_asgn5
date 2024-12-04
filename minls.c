#include "min_common.h"

// int parse_args();
// void read_fs();
// void read_block();

int main(int argc, char** argv) {
    // read FS
    FILE* fp = fopen("HardDisk", "r");
    if (!fp) {
        perror("file doesnt exist");
        exit(EXIT_FAILURE);
    }

    validate_partion_table(fp);
    PartitionTableEntry_t entry = get_partion_entry(fp, 0, 0);

    printf("%x %x", entry.bootind, entry.type);

    return 0;
}

// void read_fs() {
// 
    // return;
// }