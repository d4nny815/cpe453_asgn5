#include "min_common.h"

// int parse_args();
// void read_fs();
// void read_block();

int main(int argc, char** argv) {
    SuperBlock_t super_block;
    MinArgs_t args;
    // parse args
    parse_args(argc, argv, true, &args);


    if (args.partnum < 0) {
        // go to superblock
    }
    else {
        if (!isvalid_partition_table(args.image_file)) {
            perror("Invalid partition table");
            exit(EXIT_FAILURE);
        }
        get_superblock(&args, &super_block);
    }

    /**
     * parse args
     * open file
     * 
     * if -p > 0
     *   if valid part table
     *     get part entry
     *     get superblock
     *   else
     *     error
     * else
     *   get superblock
     * 
     * get root inode from superblock
     * 
     * 
     */

    return 0;
}

// void read_fs() {
// 
    // return;
// }