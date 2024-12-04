#include "min_common.h"


// 0 if valid, non-zero else
int validate_partion_table(const FILE* file) {
    uint8_t block[BLOCK_SIZE];

    fseek(file, 0, SEEK_SET);
    size_t bytes = fread(block, 1, BLOCK_SIZE, file);
    // TODO: error check

    if (block[510] != VALID_BYTE510 || block[511] != VALID_BYTE511) {
        return -1;
    }

    return 0;
}

PartitionTableEntry_t get_partion_entry(const FILE* file, int partnum, int subnum) {
    PartitionTableEntry_t ret;
    uint8_t block[BLOCK_SIZE];

    fseek(file, 0, SEEK_SET);
    size_t bytes = fread(block, 1, BLOCK_SIZE, file);

    PartitionTableEntry_t* entry = (PartitionTableEntry_t*) (block + PART_TBL_OFFSET); 
    entry += partnum;
    ret = *entry;
    
    // TODO: sumting with subnum; 

    return ret;
}
