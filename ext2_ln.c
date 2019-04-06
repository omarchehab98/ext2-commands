/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s [-s] <image file name> <source path> <dest path>\n", program);
}

int main(int argc, char **argv) {
	enum { HARD_LINK, SYM_LINK } mode = HARD_LINK;  // Default set
	int opt;

	while ((opt = getopt(argc, argv, "s")) != -1) {
		switch (opt) {
			case 's':
				mode = SYM_LINK;
				break;

			default:
				usage(get_filename(argv[0]));
				exit(EXIT_FAILURE);
		}
	}

	if (argc < 4) {
		usage(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}
	
	if (!is_abs_path(argv[optind + 1])) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[optind + 1], strerror(EINVAL));
		exit(EXIT_FAILURE);
	}

	if (!is_abs_path(argv[optind + 2])) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[optind + 2], strerror(EINVAL));
		exit(EXIT_FAILURE);
	}

	disk = load_disk(argv[optind]);

	char *dest_path = get_filepath(argv[optind + 2]);
	char *dest_name = get_filename(argv[optind + 2]);
	if (strlen(dest_name) > EXT2_NAME_LEN) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), dest_name, strerror(ENAMETOOLONG));
		exit(EXIT_FAILURE);
	}

	unsigned int source_inode = inode_by_filepath(disk, argv[optind + 1]);
	if (source_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[optind + 1], strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	unsigned int dest_path_inode = inode_by_filepath(disk, dest_path);
	if (dest_path_inode == -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), dest_path, strerror(ENOENT));
		exit(EXIT_FAILURE);
	}

	// TODO: can improve efficiency by using dest_path_inode to check if dest_name exists
	if (inode_by_filepath(disk, argv[optind + 2]) != -1) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[optind + 2], strerror(EEXIST));
		exit(EXIT_FAILURE);
	}
	
	struct ext2_inode *source_inode_entry = inode_from_index(disk, source_inode);
	if (S_ISDIR(source_inode_entry->i_mode)) {
		fprintf(stderr, "%s: %s: %s\n", get_filename(argv[0]), argv[optind + 1], strerror(EISDIR));
		exit(EXIT_FAILURE);
	}

	if (mode == HARD_LINK) {
		struct ext2_dir_entry *link_dir_entry = new_dir_entry(disk, dest_path_inode, source_inode, dest_name, EXT2_FT_REG_FILE);
		if (link_dir_entry == NULL) {
			perror(get_filename(argv[0]));
			exit(EXIT_FAILURE);
		}
	} else {
		unsigned int link_inode = new_inode_link(disk);
		struct ext2_dir_entry *link_dir_entry = new_dir_entry(disk, dest_path_inode, link_inode, dest_name, EXT2_FT_SYMLINK);
		if (link_dir_entry == NULL) {
			perror(get_filename(argv[0]));
			exit(EXIT_FAILURE);
		}
		
		if (write_string_to_blocks(disk, link_dir_entry, argv[optind + 1]) == NULL) {
			perror(get_filename(argv[0]));
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
