/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s <image file name> <path>\n", program);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		usage(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	if (!is_abs_path(argv[2])) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(EINVAL));
		exit(EXIT_FAILURE);
	}

	disk = load_disk(argv[1]);

	char *path = get_filepath(argv[2]);
	char *name = get_filename(argv[2]);

	unsigned int file_inode = inode_by_filepath(disk, argv[2]);
	if (file_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	struct ext2_inode *file_inode_entry = inode_from_index(disk, file_inode);
	if (S_ISDIR(file_inode_entry->i_mode)) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[2], strerror(EISDIR));
		exit(EXIT_FAILURE);
	}

	unsigned int path_inode = inode_by_filepath(disk, path);
	if (path_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), path, strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	if (rm_dir_entry(disk, path_inode, name) == NULL) {
		perror(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	return 0;
}
