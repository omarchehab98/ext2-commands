/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s <image file name> <path to source file> <path to dest>\n", program);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if (!is_abs_path(argv[3])) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[3], strerror(EINVAL));
		exit(EXIT_FAILURE);
	}

	disk = load_disk(argv[1]);

	char *source_name = get_filename(argv[2]);
	trim_trailing_slash(argv[3]);
	char *dest_path = get_filepath(argv[3]);
	char *dest_name = get_filename(argv[3]);

	unsigned int dest_inode = inode_by_filepath(disk, dest_path);
	if (dest_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), dest_path, strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	unsigned int dest_file_inode = inode_by_filepath(disk, argv[3]);
	if (dest_file_inode != -1) {
		struct ext2_inode *dest_file_inode_entry = inode_from_index(disk, dest_file_inode);
		if (S_ISDIR(dest_file_inode_entry->i_mode)) {
			dest_path = path_join(dest_path, dest_name);
			dest_name = source_name;
			dest_inode = inode_by_filepath(disk, dest_path);
		} else {
			fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[3], strerror(EEXIST));
			exit(EXIT_FAILURE);
		}
	}

	if (strlen(dest_name) > EXT2_NAME_LEN) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), dest_name, strerror(ENAMETOOLONG));
		exit(EXIT_FAILURE);
	}
	
	struct ext2_inode *dest_inode_entry = inode_from_index(disk, dest_inode);
	if (!S_ISDIR(dest_inode_entry->i_mode)) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), dest_path, strerror(ENOTDIR));
		exit(EXIT_FAILURE);
	}

	char *source = read_to_memory(argv[2]);
	if (source == NULL) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(errno));
		exit(EXIT_FAILURE);
	}

	unsigned int file_inode = new_inode_file(disk);
	if (file_inode == -1) {
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	struct ext2_dir_entry* file_dir_entry = new_dir_entry(disk, dest_inode, file_inode, dest_name, EXT2_FT_REG_FILE);
	if (file_dir_entry == NULL) {
		// TODO: cleanup file_inode
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	if (write_string_to_blocks(disk, file_dir_entry, source) == NULL) {
		// TODO: cleanup file_inode, file_dir_entry
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	return 0;
}
