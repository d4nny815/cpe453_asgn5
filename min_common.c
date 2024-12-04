#include "min_common.h"



void parse_args(int argc, char** argv, bool minls, MinArgs_t* args) {
    args->partnum = -1;
    args->subnum = -1;
    
    // min args len

    // minls [ -v ] [ -p part [ -s subpart ] ] imagefile [ path ]
    // minget [ -v ] [ -p part [ -s subpart ] ] imagefile srcpath [ dstpath ]
    // h flag?
    char opt;
    while ((opt = getopt(argc, argv, "vp:s:")) != -1) {
        switch (opt) {
        case 'v':
            args->verbose = true;
            break;
        case 'p':
            if (strlen(optarg) == 0) {
                perror("invalid part num");
                exit(EXIT_FAILURE);
            }
            args->partnum = atoi(optarg);
            if (args->partnum < 0 || args->partnum > 3) {
                // error
            }
            break;
        case 's':
            if (strlen(optarg) == 0) {
                perror("invalid part num");
                exit(EXIT_FAILURE);
            }
            args->subnum = atoi(optarg);
            if (args->subnum < 0 || args->subnum > 3) {
                // error
            }
            break;
        default:
            // print you did this wrong
            break;
        }
    }

    //TODO: SUBNUM PARTNUM CHECK

    // image_file
    if (optind < argc) {
        args->image_file = fopen(argv[optind], "r");
        if (!args->image_file) {
            perror("file doesnt exist");
            exit(EXIT_FAILURE);
        }
    }

    if (minls) {
        // pathname
        optind++;
        if (optind < argc) {
            strncpy(args->path, argv[optind], PATH_MAX);
        }
    }
    else {
        optind++;
        strncpy(args->src_path, argv[optind], PATH_MAX);
        optind++;
        if (optind < argc) {
            strncpy(args->dst_path, argv[optind], PATH_MAX);
        }
    }

    return;
}

// 1 if valid, zero if not
// FIXME
bool isvalid_minix_fs(FILE* file) {
    // uint8_t block[BLOCK_SIZE];
    // fseek(file, 0, SEEK_SET);
    // size_t bytes = fread(block, 1, BLOCK_SIZE, file);

    // SuperBlock_t* super = (SuperBlock_t*) (block + 1024);

    // return super->magic == MINIX_MAGIC_NUM;
    return false;
}

//
bool isvalid_partition_table(uint8_t* boot_block) {
    return boot_block[510] == VALID_BYTE510 && boot_block[511] == VALID_BYTE511;
}


void get_superblock(MinArgs_t* args, SuperBlock_t* sup_block) {
    uint8_t block[BOOT_BLOCK_SIZE];
    size_t bytes;
    PartitionTableEntry_t* entry;

    if (args->partnum < 0) {
        fseek(args->image_file, SB_OFFSET, SEEK_SET);
        bytes = fread(sup_block, sizeof(SuperBlock_t), 1, args->image_file);
        //TODO: ERRORCHECK FREAD
        return;
    }

    fseek(args->image_file, 0, SEEK_SET);
    bytes = fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
    if (!isvalid_partition_table(block)) {
        perror("invalid partition table");
        exit(EXIT_FAILURE);
    }

    entry = (PartitionTableEntry_t*)(block + PART_TBL_OFFSET) + args->partnum;
    if (args->subnum < 0) {
        if (entry->type != MINIX_PART_TYPE) {
            perror("not a minix partition type");
            exit(EXIT_FAILURE);
        }

        fseek(args->image_file, entry->lFirst * SECTOR_SIZE + SB_OFFSET, SEEK_SET);
        bytes = fread(sup_block, sizeof(SuperBlock_t), 1, args->image_file);
        return;
    }

    fseek(args->image_file, entry->lFirst * SECTOR_SIZE, SEEK_SET);
    bytes = fread(block, sizeof(uint8_t), BOOT_BLOCK_SIZE, args->image_file);
    if (!isvalid_partition_table(block)) {
        perror("invalid partition table");
        exit(EXIT_FAILURE);
    }

    entry = (PartitionTableEntry_t*)(block + PART_TBL_OFFSET) + args->subnum;
    if (entry->type != MINIX_PART_TYPE) {
        perror("not a minix partition type");
        exit(EXIT_FAILURE);
    }

    fseek(args->image_file, entry->lFirst * SECTOR_SIZE + SB_OFFSET, SEEK_SET);
    bytes = fread(sup_block, sizeof(SuperBlock_t), 1, args->image_file);
    return;
}
