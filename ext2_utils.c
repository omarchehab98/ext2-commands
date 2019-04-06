/**
 * Copyright (C) 2019
 * Omar Chehab (omarchehab98@gmail.com)
 * University of Toronto
 */
#include "ext2_utils.h"


unsigned char *load_disk(char *path) {
	int fd = open(path, O_RDWR);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	unsigned char *disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	return disk;
}


struct ext2_inode *inode_from_index(unsigned char *disk, unsigned int inode) {
	unsigned char *inode_tbl = (unsigned char *) DISK_INODE_TABLE(disk);
	return (struct ext2_inode *)(inode_tbl + (sizeof(struct ext2_inode) * inode));
}


struct ext2_dir_entry *dir_entry_from_index(unsigned char *disk, unsigned int block) {
	return (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block);
}


bool is_inode_reserved(unsigned int inode) {
	return inode != EXT2_ROOT_INO - 1 && inode <= 10;
}


void inode_block_foreach(unsigned char *disk, unsigned int inode, void (*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg) {
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	inode_block_foreach_helper(disk, inode, inode_entry->i_block, 12, 0, callback, arg)
		&& inode_block_foreach_helper(disk, inode, inode_entry->i_block + 12, 1, 1, callback, arg)
		&& inode_block_foreach_helper(disk, inode, inode_entry->i_block + 13, 1, 2, callback, arg)
		&& inode_block_foreach_helper(disk, inode, inode_entry->i_block + 14, 1, 3, callback, arg);
}


bool inode_block_foreach_helper(unsigned char *disk, unsigned int inode, unsigned int *iblocks_tbl, unsigned int nblocks, unsigned int indirection, void (*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg) {
	for (int i = 0; i < nblocks; i++) {
		unsigned int block = iblocks_tbl[i] - 1;
		if (block != -1) {
			if (indirection) {
				unsigned int *ib1 = (unsigned int *)(disk + EXT2_BLOCK_SIZE * block);
				inode_block_foreach_helper(disk, inode, ib1, 256, indirection - 1, callback, arg);
			} else {
				(*callback)(disk, inode, block, arg);
			}
		} else {
			return false;
		}
	}
	return true;
}


struct ext2_dir_entry *inode_dir_entry_find(unsigned char *disk, unsigned int inode, struct ext2_dir_entry *(*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg) {
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	struct ext2_dir_entry *result = inode_dir_entry_find_helper(disk, inode, inode_entry->i_block, 12, 0, callback, arg);
	if (result != NULL) return result;
	result = inode_dir_entry_find_helper(disk, inode, inode_entry->i_block + 12, 1, 1, callback, arg);
	if (result != NULL) return result;
	// TODO: Haven't figured out how to make this more efficient, it currently searches through all
	// dir entries, can optimize later.

	// result = inode_dir_entry_find_helper(disk, inode, inode_entry->i_block + 13, 1, 2, callback, arg);
	// if (result != NULL) return result;
	// result = inode_dir_entry_find_helper(disk, inode, inode_entry->i_block + 14, 1, 3, callback, arg);
	// if (result != NULL) return result;
	return NULL;
}


struct ext2_dir_entry *inode_dir_entry_find_helper(unsigned char *disk, unsigned int inode, unsigned int *iblocks_tbl, unsigned int nblocks, unsigned int indirection, struct ext2_dir_entry *(*callback)(unsigned char *, unsigned int, unsigned int, void *), void *arg) {
	for (int i = 0; i < nblocks; i++) {
		unsigned int block = iblocks_tbl[i];
		if (block) {
			if (indirection) {
				unsigned int *ib1 = (unsigned int *)(disk + EXT2_BLOCK_SIZE * block);
				struct ext2_dir_entry *dir_entry = inode_dir_entry_find_helper(disk, inode, ib1, 256, indirection - 1, callback, arg);
				if (dir_entry != NULL) {
					return dir_entry;
				}
			} else {
				struct ext2_dir_entry *dir_entry = (*callback)(disk, inode, block, arg);
				if (dir_entry != NULL) {
					return dir_entry->inode ? dir_entry : NULL;
				}
			}
		} else {
			return NULL;
		}
	}
	return NULL;
}


void inode_foreach(unsigned char *disk, void (*callback)(unsigned char *, unsigned int, void *), void *arg) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	for (unsigned int i = 0; i < super_block->s_inodes_count / 8; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			unsigned int inode = i * 8 + j;
			(*callback)(disk, inode, arg);
		}
	}
}


void block_foreach(unsigned char *disk, void (*callback)(unsigned char *, unsigned int, void *), void *arg) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	for (unsigned int i = 0; i < super_block->s_blocks_count / 8; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			unsigned int block = i * 8 + j;
			(*callback)(disk, block, arg);
		}
	}
}


unsigned int inode_find(unsigned char *disk, bool (*callback)(unsigned char *, unsigned int, void *), void *arg) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	for (unsigned int i = 0; i < super_block->s_inodes_count / 8; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			unsigned int inode = i * 8 + j;
			if ((*callback)(disk, inode, arg)) {
				return inode;
			}
		}
	}
	return -1;
}


unsigned int block_find(unsigned char *disk, bool (*callback)(unsigned char *, unsigned int, void *), void *arg) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	for (unsigned int i = 0; i < super_block->s_blocks_count / 8; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			unsigned int block = i * 8 + j;
			if ((*callback)(disk, block, arg)) {
				return block;
			}
		}
	}
	return -1;
}


void directory_entry_foreach(unsigned char *disk, unsigned int inode, void (*callback)(unsigned char *, struct ext2_dir_entry *, void *), void *arg) {
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);

	if (S_ISDIR(inode_entry->i_mode)) {
		struct directory_entry_foreach_helper_arg *helper_arg = malloc(sizeof (struct directory_entry_foreach_helper_arg));
		if (helper_arg == NULL) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		helper_arg->arg = arg;
		helper_arg->callback = callback;

		inode_block_foreach(disk, inode, &directory_entry_foreach_helper, helper_arg);
	}
}


void directory_entry_foreach_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	struct directory_entry_foreach_helper_arg *helper_arg = arg;
	void (*callback)(unsigned char *, struct ext2_dir_entry *, void *) = helper_arg->callback;
	
	struct ext2_dir_entry *dir_entry = dir_entry_from_index(disk, block + 1);
	int total = 0;

	while (total < EXT2_BLOCK_SIZE) {
		(*callback)(disk, dir_entry, helper_arg->arg);
		
		total += dir_entry->rec_len;
		dir_entry = (void *) dir_entry + dir_entry->rec_len;
	}
}


bool is_inode_free(unsigned char *disk, unsigned int inode) {
	if (is_inode_reserved(inode)) return false;
	unsigned char *inode_bitmap = DISK_INODE_BITMAP(disk);
	unsigned short index = inode / 8;
	unsigned short offset = inode % 8;
	return !(1 & inode_bitmap[index] >> offset);
}


bool is_block_free(unsigned char *disk, unsigned int block) {
	unsigned char *block_bitmap = DISK_BLOCK_BITMAP(disk);
	unsigned short index = block / 8;
	unsigned short offset = block % 8;
	return !(1 & block_bitmap[index] >> offset);
}


unsigned int next_free_inode(unsigned char *disk) {
	return inode_find(disk, &next_free_inode_helper, NULL);
}


bool next_free_inode_helper(unsigned char *disk, unsigned int inode, void *arg) {
	return is_inode_free(disk, inode);
}


unsigned int next_free_block(unsigned char *disk) {
	return block_find(disk, &next_free_block_helper, NULL);
}


bool next_free_block_helper(unsigned char *disk, unsigned int block, void *arg) {
	return is_block_free(disk, block);
}


unsigned short rec_len_boundary(unsigned short rec_len) {
	return rec_len + 4 - rec_len % 4;
}


bool is_abs_path(char *path) {
	return path[0] == '/';
}


void trim_trailing_slash(char *path) {
	size_t len = strlen(path);
	while (len != 1 && path[len - 1] == '/') {
		path[len - 1] = '\0';
		len -= 1;
	}
}


char *get_filepath(char *abspath) {
	int i = strlen(abspath);
	char *filepath = malloc(sizeof (char) * (i + 1));
	if (filepath == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	strcpy(filepath, abspath);
	for (; i > 0 && filepath[i] != '/'; i--);
	filepath[i] = '\0';
	return filepath;
}


char *get_filename(char *abspath) {
	int i = strlen(abspath);
	char *filename = malloc(sizeof (char) * (i + 1));
	if (filename == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	strcpy(filename, abspath);
	for (; i >= 0 && filename[i] != '/'; i--);
	filename += i + 1;
	return filename;
}


char *shift_filepath(char **abspath) {
	while ((*abspath)[0] == '/') *abspath += 1;
	char *filename = *abspath;
	int i = 0;
	for (; (*abspath)[i] != '/' && (*abspath)[i] != '\0'; i++);
	if (filename[0] != '\0' && ((*abspath)[i] == '/' || (*abspath)[i] == '\0')) {
		(*abspath)[i] = '\0';
		*abspath += i + 1;
		return filename;
	}  else {
		return NULL;
	}
}


unsigned int inode_by_filepath(unsigned char *disk, char *abspath) {
	char *abspath_clone = malloc(sizeof (char) * (strlen(abspath) + 1));
	if (abspath_clone == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	strcpy(abspath_clone, abspath);

	unsigned int inode = EXT2_ROOT_INO - 1;
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	
	char *filename = NULL;
	while ((filename = shift_filepath(&abspath_clone))) {
		if (S_ISDIR(inode_entry->i_mode)) {
			struct ext2_dir_entry *dir_entry = inode_dir_entry_find(disk, inode, &inode_by_filepath_helper, filename);
			if (dir_entry == NULL) return -1;
			inode = dir_entry->inode - 1;
			inode_entry = inode_from_index(disk, inode);
		} else if (!S_ISDIR(inode_entry->i_mode) && abspath_clone[0] != '\0') {
			return -1;
		}
	}

	return inode;
}


struct ext2_dir_entry *inode_by_filepath_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	char *filename = arg;
	struct ext2_dir_entry *dir_entry = dir_entry_from_index(disk, block);
	struct ext2_dir_entry *found = dir_entry_by_name(dir_entry, filename);
	return found;
}


struct ext2_dir_entry *dir_entry_by_name(struct ext2_dir_entry *dir_entry, char *filename) {
	int total = 0;
	size_t filename_len = strlen(filename);

	while (total < EXT2_BLOCK_SIZE) {
		if (filename_len == dir_entry->name_len && strncmp(filename, dir_entry->name, dir_entry->name_len) == 0) {
			return dir_entry;
		}

		total += dir_entry->rec_len;
		dir_entry = (void *) dir_entry + dir_entry->rec_len;
	}

	return NULL;
}


struct ext2_dir_entry *dir_entry_before_name(struct ext2_dir_entry *dir_entry, char *filename) {
	int total = 0;
	size_t filename_len = strlen(filename);
	struct ext2_dir_entry *prev_dir_entry = dir_entry;

	while (total < EXT2_BLOCK_SIZE) {
		if (filename_len == dir_entry->name_len && strncmp(filename, dir_entry->name, dir_entry->name_len) == 0) {
			return prev_dir_entry;
		}
		prev_dir_entry = dir_entry;

		total += dir_entry->rec_len;
		dir_entry = (void *) dir_entry + dir_entry->rec_len;
	}

	return NULL;
}


void set_bit_by_index(unsigned char *bitmap, unsigned int n) {
	unsigned short index = n / 8;
	unsigned short offset = n % 8;
	*(bitmap + index) |= 1 << offset;
}


void unset_bit_by_index(unsigned char *bitmap, unsigned int n) {
	unsigned short index = n / 8;
	unsigned short offset = n % 8;
	*(bitmap + index) &= ~(1 << offset);
}


bool is_bit_set_by_index(unsigned char *bitmap, unsigned int n) {
	unsigned short index = n / 8;
	unsigned short offset = n % 8;
	return 1 & (*(bitmap + index) >> offset);
}


unsigned int new_inode(unsigned char *disk) {
	unsigned int inode = next_free_inode(disk);
	if (inode == -1) {
		errno = ENOSPC;
		return -1;
	}

	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *group_desc = DISK_GROUP_DESC(disk);
	unsigned char *inode_bitmap = DISK_INODE_BITMAP(disk);

	set_bit_by_index(inode_bitmap, inode);
	super_block->s_free_inodes_count--;
	group_desc->bg_free_inodes_count--;

	return inode;
}


unsigned int new_block(unsigned char *disk) {
	unsigned int block = next_free_block(disk);
	if (block == -1) {
		errno = ENOSPC;
		return -1;
	}
		
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *group_desc = DISK_GROUP_DESC(disk);
	unsigned char *block_bitmap = DISK_BLOCK_BITMAP(disk);

	set_bit_by_index(block_bitmap, block);
	super_block->s_free_blocks_count--;
	group_desc->bg_free_blocks_count--;

	return block;
}


void rm_inode(unsigned char *disk, unsigned int inode) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *group_desc = DISK_GROUP_DESC(disk);
	unsigned char *inode_bitmap = DISK_INODE_BITMAP(disk);

	unset_bit_by_index(inode_bitmap, inode);
	super_block->s_free_inodes_count++;
	group_desc->bg_free_inodes_count++;
}


void rm_block(unsigned char *disk, unsigned int block) {
	struct ext2_super_block *super_block = DISK_SUPER_BLOCK(disk);
	struct ext2_group_desc *group_desc = DISK_GROUP_DESC(disk);
	unsigned char *block_bitmap = DISK_BLOCK_BITMAP(disk);

	unset_bit_by_index(block_bitmap, block);
	super_block->s_free_blocks_count++;
	group_desc->bg_free_blocks_count++;
}


unsigned int new_inode_dir(unsigned char *disk) {
	unsigned int inode = new_inode(disk);
	if (inode == -1) {
		return -1;
	}
	
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	inode_entry->i_links_count = 0;
	inode_entry->i_mode = EXT2_S_IFDIR;
	inode_entry->i_size = 0;
	inode_entry->i_ctime = time(NULL);
	inode_entry->i_dtime = 0;
	inode_entry->i_blocks = 0;
	for (int i = 0; i < 15; i++) inode_entry->i_block[i] = 0;

	return inode;
}


unsigned int new_inode_file(unsigned char *disk) {
	unsigned int inode = new_inode(disk);
	if (inode == -1) {
		return -1;
	}
	
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	inode_entry->i_links_count = 0;
	inode_entry->i_mode = EXT2_S_IFREG;
	inode_entry->i_size = 0;
	inode_entry->i_ctime = time(NULL);
	inode_entry->i_dtime = 0;
	inode_entry->i_blocks = 0;
	for (int i = 0; i < 15; i++) inode_entry->i_block[i] = 0;

	return inode;
}


unsigned int new_inode_link(unsigned char *disk) {
	unsigned int inode = new_inode(disk);
	if (inode == -1) {
		return -1;
	}
	
	struct ext2_inode *inode_entry = inode_from_index(disk, inode);
	inode_entry->i_links_count = 0;
	inode_entry->i_mode = EXT2_S_IFLNK;
	inode_entry->i_size = 0;
	inode_entry->i_ctime = time(NULL);
	inode_entry->i_dtime = 0;
	inode_entry->i_blocks = 0;
	for (int i = 0; i < 15; i++) inode_entry->i_block[i] = 0;

	return inode;
}


struct ext2_dir_entry *new_dir_entry(unsigned char *disk, unsigned int parent_inode, unsigned int child_inode, char *name, unsigned char file_type) {
	size_t name_len = strlen(name);

	struct ext2_inode *parent_inode_entry = inode_from_index(disk, parent_inode);
	struct ext2_inode *child_inode_entry = inode_from_index(disk, child_inode);
	unsigned int new_dir_entry_size = rec_len_boundary(sizeof(struct ext2_dir_entry) + name_len);
	struct ext2_dir_entry *dir_entry = NULL;
	unsigned short rec_len = 0;

	unsigned int i = parent_inode_entry->i_blocks / 2;
	if (i > 0) {
		i--;
		if (i < 12) {
			dir_entry = dir_entry_from_index(disk, parent_inode_entry->i_block[i]);
			for (unsigned int total = 0; total + dir_entry->rec_len < EXT2_BLOCK_SIZE; total += dir_entry->rec_len) {
				dir_entry = (void *) dir_entry + dir_entry->rec_len;
			}
			
			unsigned int dir_entry_size = rec_len_boundary(sizeof(struct ext2_dir_entry) + dir_entry->name_len);
			unsigned int padding = dir_entry->rec_len - dir_entry_size;
			if (new_dir_entry_size <= padding) {
				dir_entry->rec_len = dir_entry_size;
				rec_len = padding;
			}
		} else {
			// TODO: handle indirection at i = 11
		}
	}

	if (rec_len == 0) {
		int block = new_block(disk);
		if (block == -1) {
			return NULL;
		}
		parent_inode_entry->i_block[i] = block + 1;
		parent_inode_entry->i_blocks += EXT2_BLOCK_SIZE / 512;
		parent_inode_entry->i_size += 1024;

		dir_entry = dir_entry_from_index(disk, parent_inode_entry->i_block[i]);
		rec_len = EXT2_BLOCK_SIZE;
	} else {
		dir_entry = (void *) dir_entry + dir_entry->rec_len;
	}

	if (rec_len > 0) {
		strncpy(dir_entry->name, name, name_len);
		dir_entry->name_len = name_len;
		dir_entry->inode = child_inode + 1;
		dir_entry->file_type = file_type;
		dir_entry->rec_len = rec_len;
		
		child_inode_entry->i_links_count++;
		return dir_entry;
	} else {
		errno = ENOSPC;
		return NULL;
	}
}


struct ext2_dir_entry *rm_dir_entry(unsigned char *disk, unsigned int parent_inode, char *filename) {
	struct ext2_inode *parent_inode_entry = inode_from_index(disk, parent_inode);
	struct ext2_dir_entry *dir = NULL;
	struct ext2_dir_entry *before_file = NULL;
	
	for (int i = 0; before_file == NULL && i < 12 && parent_inode_entry->i_block[i]; i++) {
		// TODO: handle indirection at i = 11
		dir = dir_entry_from_index(disk, parent_inode_entry->i_block[i]);
		before_file = dir_entry_before_name(dir, filename);
	}

	if (before_file) {
		struct ext2_dir_entry *file = (struct ext2_dir_entry *)((void *) before_file + before_file->rec_len);
		unsigned int file_inode = file->inode - 1;
		struct ext2_inode *file_inode_entry = inode_from_index(disk, file_inode);

		bool isFirstEntry = file->name_len == before_file->name_len && strncmp(file->name, before_file->name, before_file->name_len) == 0;
		if (isFirstEntry) {
			dir_entry_to_blank(file);
		} else {
			dir_entry_rm_next(before_file);
		}

		file_inode_entry->i_links_count--;
		if (file_inode_entry->i_links_count == 0) {
			inode_block_foreach(disk, file_inode, &rm_dir_entry_helper, NULL);
			rm_inode(disk, file_inode);
		}
	} else {
		errno = ENOENT;
	}

	return before_file;
}


void rm_dir_entry_helper(unsigned char *disk, unsigned int inode, unsigned int block, void *arg) {
	rm_block(disk, block);
}


char *read_to_memory(char *path) {
	char *buffer = NULL;
	FILE *fp = fopen(path, "r");
	long size = 0L;

	if (fp == NULL) {
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	buffer = calloc(1, size + 1);
	if (buffer == NULL || (size > 0 && 1 != fread(buffer, size, 1, fp))) {
		return NULL;
	}

	fclose(fp);
	return buffer;
}


char *path_join(char *path1, char *path2) {
	char *new_path = malloc(strlen(path1) + strlen(path2) + 2);
	if (new_path == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	sprintf(new_path, "%s/%s", path1, path2);
	return new_path;
}


struct ext2_dir_entry *write_string_to_blocks(unsigned char *disk, struct ext2_dir_entry *dir_entry, char *source) {
	size_t source_len = strlen(source);
	struct ext2_inode *inode_entry = inode_from_index(disk, dir_entry->inode - 1);
	inode_entry->i_size = source_len;
	int i, size;

	for (i = 0, size = 0; size < inode_entry->i_size && i < 12; i++, size += EXT2_BLOCK_SIZE) {
		// TODO: if (inode_entry->i_block[i] != 0), then delete block
		unsigned int block = new_block(disk);
		if (block == -1) {
			return NULL;
		}
		inode_entry->i_block[i] = block + 1;
		inode_entry->i_blocks += EXT2_BLOCK_SIZE / 512;

		void *destination = (void *)(disk + EXT2_BLOCK_SIZE * (block + 1));
		unsigned int bytes = MIN(inode_entry->i_size - size, EXT2_BLOCK_SIZE);
		memcpy(destination, source, bytes);
		source += EXT2_BLOCK_SIZE;
	}

	unsigned int *indirect_i_block = NULL;
	if (size < inode_entry->i_size) {
		unsigned int block = new_block(disk);
		if (block == -1) {
			return NULL;
		}
		inode_entry->i_block[i] = block + 1;
		inode_entry->i_blocks += EXT2_BLOCK_SIZE / 512;
		indirect_i_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * (block + 1));
	}

	for (i = 0; size < inode_entry->i_size && i < 256; i++, size += EXT2_BLOCK_SIZE) {
		unsigned int block = new_block(disk);
		if (block == -1) {
			return NULL;
		}
		indirect_i_block[i] = block + 1;
		inode_entry->i_blocks += EXT2_BLOCK_SIZE / 512;

		void *destination = (void *)(disk + EXT2_BLOCK_SIZE * (block + 1));
		unsigned int bytes = MIN(inode_entry->i_size - size, EXT2_BLOCK_SIZE);
		memcpy(destination, source, bytes);
		source += EXT2_BLOCK_SIZE;
	}

	return dir_entry;
}


struct ext2_dir_entry *dir_entry_rm_next(struct ext2_dir_entry *dir_entry) {
	struct ext2_dir_entry *next_dir_entry = (void *) dir_entry + dir_entry->rec_len;
	dir_entry->rec_len += next_dir_entry->rec_len;
	return dir_entry;
}


struct ext2_dir_entry *dir_entry_to_blank(struct ext2_dir_entry *dir_entry) {
	dir_entry->inode = 0;
	return dir_entry;
}


unsigned int unsigned_abs_diff(unsigned int x, unsigned int y) {
	return MAX(x, y) - MIN(x, y);
}


bool is_dot_or_dot_dot(char *name, unsigned char name_len) {
	return (name_len == 1 && name[0] == '.') || (name_len == 2 && name[0] == '.' && name[1] == '.');
}
