#include "min_common.h"

Inode_t* inode_list;
size_t cur_inode_ind;

int main(int argc, char** argv) {
    MinArgs_t args;
    PartitionTableEntry_t pt_entry;
    SuperBlock_t super_block;
    
    int err;

    // parse args and put them into a struct
    parse_args(argc, argv, true, &args);

    get_partition_entry(&args, &pt_entry);
    get_superblock(&args, &pt_entry, &super_block);

    // get the super block asked for
    if(!isvalid_minix_fs(&super_block)) {
        perror("MINIX magic number not found");
        exit(EXIT_FAILURE);
    }

    // start of the "partition"
    size_t partition_addr = pt_entry.lFirst * SECTOR_SIZE;
    size_t root_inode_addr = (partition_addr) +
        (2 + super_block.i_blocks + super_block.z_blocks) * 
        super_block.blocksize;

    size_t zone_size = super_block.blocksize << super_block.log_zone_size;

    // load in inode list
    inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    
    err = fseek(args.image_file, root_inode_addr, SEEK_SET);
    if (err == -1) {
        exit(EXIT_FAILURE);
    }
    fread(inode_list, sizeof(Inode_t), 
        super_block.ninodes, 
        args.image_file
    );

    uint32_t found_inode_num = traverse(
        args.image_file,
        args.path,
        ROOT_INODE, 
        partition_addr,
        zone_size, 
        super_block.blocksize
    );

    if (found_inode_num == INVALID_INODE) {
        perror("file not found");
        exit(EXIT_FAILURE);
    }

    Inode_t* found_inode = inode_list + (found_inode_num-1);

    if (args.verbose) {
        printf("\n");
        print_superblock(&super_block);
        printf("\n");
        print_inode(found_inode);
        printf("\n");
    }

    if (found_inode->mode & DIRECTORY) {
        printf("%s:\n", args.path);
        print_dir(
            &args, found_inode, partition_addr, 
            zone_size, super_block.blocksize
        );
    } 
    else {
        print_file(found_inode, args.path);
    }


    // walk the path
    free(inode_list);
    fclose(args.image_file);

    return 0;
}

