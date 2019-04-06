# EXT2 Commands

Implementation of extended filesystem 2 commands ext2_checker, ext2_cp, ext2_ln, ext2_mkdir, ext2_restore, and ext2_rm.

Checkout my automated blackbox test suite for them [omarchehab98/ext2-test-suite](https://github.com/omarchehab98/ext2-test-suite)

## Compile

```
make
```

## Usage

Requires disk images formatted using the ext2 filesystem specification.

You can download sample disk images from the test suite repository [omarchehab98/ext2-test-suite](https://github.com/omarchehab98/ext2-test-suite), find them in `src/fixtures/images`.

### ext2_mkdir

```
usage: ext2_mkdir <image file name> <path>
```

Creates a directory in the `image` at the `path`.

### ext2_cp

```
usage: "usage: %s <image file name> <path to source file> <path to dest>
```

Copies `source` from the host to the `image` at the `path`.

### ext2_ln

```
usage: ext2_ln [-s] <image file name> <source path> <dest path>
```

Creates a hard or symbolic link at `dest path` pointing to `source path`.

### ext2_rm

```
usage: ext2_rm <image file name> <path>
```

Removes a file from `image` at the `path`.

### ext2_restore

```
usage: ext2_restore <image file name> <path>
```

Restores a removed file from `image` at the `path`.

### ext2_checker

```
usage: ext2_checker <image file name>
```

Checks the ext2 filesystem for inconsistencies and fixes them.

### ext2_dump

```
usage: ext2_dump <image file name>
```

Dumps the super block, block groups, inodes, datablocks, and directory entries standard out.

## Resources

* https://www.nongnu.org/ext2-doc/ext2.html
* https://wiki.osdev.org/Ext2
