#include "min_common.h"

void print_file();
void print_dir();
char** split_path(const char* path);
void free_split_path(char** path_components);



Inode_t* inode_list;
size_t cur_inode_ind;

int main(int argc, char** argv) {
    size_t bytes;

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

    // load in inode list
    // inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    // cur_inode_ind = ROOT_INODE;
    // err = fseek(args.image_file, root_inode_addr, SEEK_SET);
    // if (err == -1) {
    //     exit(1);
    // }
    // bytes = fread(inode_list, sizeof(Inode_t), 
    //             super_block.ninodes, args.image_file);

    // split the path
    // size_t zone_size = super_block.blocksize << super_block.log_zone_size;

    // char* path_copy = strdup(args.path);
    // char* token = strtok(path_copy, "/");
    // DirEntry_t* dir_entries = (DirEntry_t*) malloc(zone_size);

    // size_t zone_addr = partition_addr + 
    //     (zone_size * inode_list[cur_inode_ind - 1].zone[0]);
    // err = fseek(args.image_file, zone_addr, SEEK_SET);
    // if (err == -1) {
    //     exit(1);
    // }
    // fread(dir_entries, zone_size, 1, args.image_file);

    // while (token) {
    //     // search for inode
    //     int inodes_size = inode_list[cur_inode_ind - 1].size 
    //         / sizeof(DirEntry_t);
    //     cur_inode_ind = get_inode(token, dir_entries, inodes_size);

    //     if (cur_inode_ind == 0) {
    //         fprintf(stderr, "File %s not found!\n", args.path);
    //         exit(1);
    //     }

    //     //if its a dir go deeper
    //     if (inode_list[cur_inode_ind - 1].mode & DIRECTORY) {
    //         // printf("zone: %u \n", inode_list[cur_inode_ind - 1].zone[0]);
    //         // get new dir_entries
            
    //         zone_addr = partition_addr + 
    //             (zone_size * inode_list[cur_inode_ind - 1].zone[0]);
    //         err = fseek(args.image_file, zone_addr, SEEK_SET);
    //         if (err == -1) {
    //             exit(1);
    //         }
    //         fread(dir_entries, zone_size, 1, args.image_file);
    //     }

    //     token = strtok(NULL, "/");
    // }

    size_t zone_size = super_block.blocksize << super_block.log_zone_size;
    uint8_t* zone_buffer = (uint8_t*)malloc(zone_size);
    uint32_t zone_array[INDIRECT_ZONES];
    uint32_t indirect_zone_array[INDIRECT_ZONES];

    // load in inode list
    inode_list = (Inode_t*) malloc(sizeof(Inode_t) * super_block.ninodes);
    
    err = fseek(args.image_file, root_inode_addr, SEEK_SET);
    if (err == -1) {
        exit(EXIT_FAILURE);
    }
    bytes = fread(inode_list, sizeof(Inode_t), 
                super_block.ninodes, args.image_file);
    cur_inode_ind = ROOT_INODE - 1;
    DirEntry_t* cur_dir_entry = NULL;


    // split the path
    // strdup mallocs
    char* path_copy = strdup(args.path);
    char* token;
    if (SAME_STR(path_copy, DEFAULT_PATH)) {
        intptr_t seek_addr = partition_addr + 
                        (inode_list[cur_inode_ind].zone[0] * zone_size);
        fseek(args.image_file, seek_addr, SEEK_SET);
        fread(zone_buffer, zone_size, 1, args.image_file);
        cur_dir_entry = (DirEntry_t*)zone_buffer;
    } 
    token = strtok(path_copy, "/");


    bool found_token = false;
    while (token) {
        uint32_t num_bytes_left = inode_list[cur_inode_ind].size;

        // direct zones
        for (int i = 0; i < DIRECT_ZONES && num_bytes_left > 0; i++) {
            uint32_t bytes_to_read = (num_bytes_left < zone_size) ? 
                                    num_bytes_left : zone_size;
            
            if (inode_list[cur_inode_ind].zone[i] == 0) {
                num_bytes_left -= bytes_to_read;
                continue;
            }

            intptr_t seek_addr = partition_addr + 
                (inode_list[cur_inode_ind].zone[i] * zone_size);
            fseek(args.image_file, seek_addr, SEEK_SET);
            fread(zone_buffer, bytes_to_read, 1, args.image_file);
            num_bytes_left -= bytes_to_read;

            uint32_t dirs_per_zone = bytes_to_read / sizeof(DirEntry_t);
            for (int j = 0; j < dirs_per_zone; j++) {
                cur_dir_entry = (DirEntry_t*)zone_buffer + j;

                if (cur_dir_entry->inode == 0) continue;
                if (SAME_STR(cur_dir_entry->name,token)) {
                    found_token = true;
                    cur_inode_ind = cur_dir_entry->inode - 1;
                    break;
                }
            }

            if (inode_list[cur_inode_ind].mode & DIRECTORY) {
                intptr_t seek_addr = partition_addr + 
                    (zone_size * inode_list[cur_inode_ind].zone[i]);
                err = fseek(args.image_file, seek_addr, SEEK_SET);
                if (err == -1) {
                    exit(1);
                }
                fread(cur_dir_entry, zone_size, 1, args.image_file);
            }

            if (found_token) break;
        }

        if (!found_token) {
            free(inode_list);
            free(zone_buffer);
            // free(path_copy);

            perror("path does not exist");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, "/");
    }

    // ! THIS WORKS 

    cur_inode_ind = traverse(args.path, ROOT_INODE - 1, 1);
    if (cur_inode_ind == INVALID_INODE) {
        exit(1);
    }

    Inode_t file_inode = inode_list[cur_inode_ind];

    if (args.verbose) {
        printf("\n");
        print_superblock(&super_block);
        printf("\n");
        print_inode(&file_inode);
        printf("\n");
    }

    if (file_inode.mode & DIRECTORY) {
        printf("%s:\n", args.path);
        print_dir(file_inode, cur_dir_entry);
    } 
    else {
        print_file(file_inode, args.path);
    }

    

    // print_file(cur_inode_ind);
    // print_dir(cur_inode_ind, dir)

    // walk the path
    // if file
        // print file
    // else if dir
        // print dir
    // ?something for special file

    // clean up and bring er home 


    // walk the path
    free(inode_list);
    // fclose(args.image_file);

    return 0;
}

