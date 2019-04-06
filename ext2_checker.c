/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"

unsigned char *disk;

void usage(char *program) {
	fprintf(stderr, "usage: %s <image file name>\n", program);
}


struct ext2_checker_data {
	unsigned int total_fixes;
	unsigned int free_inodes;
	unsigned int free_blocks;
};


void check_inode_bit(unsigned char *disk, unsigned int inode, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;
	
	checker->free_inodes += is_inode_free(disk, inode);
}


void check_block_bit(unsigned char *disk, unsigned int block, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;

	checker->free_blocks += is_block_free(disk, block);
}


void check_dir_entry_file_type(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;

	struct ext2_inode *inode_entry = inode_from_index(disk, dir_entry->inode - 1);
	
	if (dir_entry->file_type != EXT2_FT_DIR && S_ISDIR(inode_entry->i_mode)) {
		printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
		dir_entry->file_type = EXT2_FT_DIR;
		checker->total_fixes++;
	}

	if (dir_entry->file_type != EXT2_FT_SYMLINK && S_ISLNK(inode_entry->i_mode)) {
		printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
		dir_entry->file_type = EXT2_FT_SYMLINK;
		checker->total_fixes++;
	}

	if (dir_entry->file_type != EXT2_FT_REG_FILE && S_ISREG(inode_entry->i_mode)) {
		printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
		dir_entry->file_type = EXT2_FT_REG_FILE;
		checker->total_fixes++;
	}
}


void check_dir_entry_inode(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;

	struct ext2_super_block *s = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);
	unsigned char *ib = DISK_INODE_BITMAP(disk);

	if (!is_bit_set_by_index(ib, dir_entry->inode - 1)) {
		printf("Fixed: inode [%d] not marked as in-use\n", dir_entry->inode);
		set_bit_by_index(ib, dir_entry->inode - 1);
		s->s_free_inodes_count--;
		bg->bg_free_inodes_count--;
		checker->free_inodes++;
		checker->total_fixes++;
	}
}


void check_dir_entry_i_dtime(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;

	struct ext2_inode *inode_entry = inode_from_index(disk, dir_entry->inode - 1);
	
	if (inode_entry->i_dtime) {
		printf("Fixed: valid inode marked for deletion: [%d]\n", dir_entry->inode);
		inode_entry->i_dtime = 0;
		checker->total_fixes++;
	}
}


void count_inconsistent_blocks(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	int *inconsistent_blocks = (int *) arg;

	struct ext2_super_block *s = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);
	unsigned char *bb = DISK_BLOCK_BITMAP(disk);

	if (!is_bit_set_by_index(bb, block)) {
		set_bit_by_index(bb, block);
		(*inconsistent_blocks)++;
		s->s_free_blocks_count--;
		bg->bg_free_blocks_count--;
	}
}


void check_dir_entry_datablocks(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	struct ext2_checker_data *checker = (struct ext2_checker_data *) arg;

	int inconsistent_blocks = 0;

	inode_block_foreach(disk, dir_entry->inode, &count_inconsistent_blocks, &inconsistent_blocks);

	if (inconsistent_blocks) {
		printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", inconsistent_blocks, dir_entry->inode);
		checker->free_blocks -= inconsistent_blocks;
		checker->total_fixes += inconsistent_blocks;
	}
}


void check_dir_entry(unsigned char *disk, struct ext2_dir_entry *dir_entry, void *arg) {
	check_dir_entry_file_type(disk, dir_entry, arg);
	check_dir_entry_inode(disk, dir_entry, arg);
	check_dir_entry_i_dtime(disk, dir_entry, arg);
	check_dir_entry_datablocks(disk, dir_entry, arg);

	struct ext2_inode *inode_entry = inode_from_index(disk, dir_entry->inode - 1);

	if (S_ISDIR(inode_entry->i_mode) && !is_dot_or_dot_dot(dir_entry->name, dir_entry->name_len) && !is_inode_reserved(dir_entry->inode - 1)) {
		directory_entry_foreach(disk, dir_entry->inode - 1, &check_dir_entry_file_type, arg);
	}
}


int main(int argc, char **argv) {
	if (argc != 2) {
		usage(get_filename(argv[0]));
		exit(EXIT_FAILURE);
	}

	disk = load_disk(argv[1]);

	struct ext2_super_block *s = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *bg = DISK_GROUP_DESC(disk);

	struct ext2_checker_data *checker = malloc(sizeof (struct ext2_checker_data));
	if (checker == NULL) {
		perror(argv[0]);
		exit(EXIT_FAILURE);
	}
	checker->free_inodes = 0;
	checker->free_blocks = 0;
	checker->total_fixes = 0;

	inode_foreach(disk, &check_inode_bit, (void *) checker);
	block_foreach(disk, &check_block_bit, (void *) checker);

	if (s->s_free_inodes_count != checker->free_inodes) {
		unsigned int fixes = unsigned_abs_diff(s->s_free_inodes_count, checker->free_inodes);
		printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n", fixes);
		s->s_free_inodes_count = checker->free_inodes;
		checker->total_fixes++;
	}

	if (s->s_free_blocks_count != checker->free_blocks) {
		unsigned int fixes = unsigned_abs_diff(s->s_free_blocks_count, checker->free_blocks);
		printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", fixes);
		s->s_free_blocks_count = checker->free_blocks;
		checker->total_fixes++;
	}

	if (bg->bg_free_inodes_count != checker->free_inodes) {
		unsigned int fixes = unsigned_abs_diff(bg->bg_free_inodes_count, checker->free_inodes);
		printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", fixes);
		bg->bg_free_inodes_count = checker->free_inodes;
		checker->total_fixes++;
	}

	if (bg->bg_free_blocks_count != checker->free_blocks) {
		unsigned int fixes = unsigned_abs_diff(bg->bg_free_blocks_count, checker->free_blocks);
		printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", fixes);
		bg->bg_free_blocks_count = checker->free_blocks;
		checker->total_fixes++;
	}

	directory_entry_foreach(disk, EXT2_ROOT_INO - 1, &check_dir_entry, (void *) checker);

	if (checker->total_fixes) {
		printf("%d file system inconsistencies repaired!\n", checker->total_fixes);
	} else {
		printf("No file system inconsistencies detected!\n");
	}

	return 0;
}
