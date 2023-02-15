#include "file.h"
#include "fs.h"

#define MAXFS (0x20000/sizeof(filesystem))
filesystem (*const FilesystemTable)[MAXFS] = (filesystem (* const)[MAXFS])0x240000;

// TODO Replace with something better
extern uint8_t ActiveFsId;

// Returns 0 on success, non-zero error code on error. Fills provided struct FILE
int file_open(FILE *file, char *path, char mode) {
	filesystem *fs = &(*FilesystemTable)[0];
	return fs->ops.file_open(fs->fs_data, file, path, mode);
}

// Returns 0 on success, non-zero error code on error.
int file_seek(FILE *file, uint32_t offset) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.file_seek(fs->fs_data, file, offset);
}

// Returns 0 on error, bytes read on success.
int file_read(FILE *file, uint8_t *dest, uint32_t len) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.file_read(fs->fs_data, file, dest, len);
}

// Returns 0 on error, bytes written on success.
int file_write(FILE *file, uint8_t *src, uint32_t len) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.file_write(fs->fs_data, file, src, len);
}

void file_close(FILE *file) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.file_close(fs->fs_data, file);
}

// Returns 0 on success, non-zero error code on error. Fills provided struct DIR
int dir_open(DIR *dir, char *path) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.dir_open(fs->fs_data, dir, path);
}

// Return 0 on success, non-zero error code on error. Fills provided struct dirent.
int dir_nextentry(DIR *dir, dirent *ent) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.dir_nextentry(fs->fs_data, dir, ent);
}

void dir_close(DIR *dir) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.dir_close(fs->fs_data, dir);
}

// Returns 0 on success, non-zero error code on error. Fills provided struct dirent.
int path_getinfo(char *path, dirent *ent) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.path_getinfo(fs->fs_data, path, ent);
}

// Returns 0 on success, non-zero error code on error. 
int path_mkdir(char *path) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.path_mkdir(fs->fs_data, path);
}

// Returns 0 on success, non-zero error code on error. 
int path_rmdir(char *path) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.path_rmdir(fs->fs_data, path);
}

// Returns 0 on success, non-zero error code on error. 
int path_rmfile(char *path) {
    filesystem *fs = &(*FilesystemTable)[0];
    return fs->ops.path_rmfile(fs->fs_data, path);
}
