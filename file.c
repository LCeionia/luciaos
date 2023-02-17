#include "file.h"
#include "fs.h"

// TODO Replace with something better
extern uint8_t ActiveFsId;

// Sets adjusted_path to the path without filesystem info,
// returns the filesystem calculated
uint8_t GetFsFromPath(char *path, char **adjusted_path) {
    // On active filsystem
    if (*path == '/') {
        *adjusted_path = path;
        return GetActiveFilesystemId();
    }
    // Read the filesystem ID
    uint8_t id = 0;
    char *tmp_path = path;
    // Find first /
    for (;*tmp_path != '/';tmp_path++);
    *adjusted_path = tmp_path;
    // Read octal num
    tmp_path--;
    for (uint8_t bit_off = 0; tmp_path >= path; tmp_path--, bit_off+=3) {
        // Outside of octal range, error
        if (*tmp_path < '0' || *tmp_path > '8') return 0;
        id += (*tmp_path - '0') << bit_off;

    }
    // Return filesystem
    return id;
}

// Returns 0 on success, non-zero error code on error. Fills provided struct FILE
int file_open(FILE *file, char *path, char mode) {
    char *adj_path;
	uint8_t fsid = GetFsFromPath(path, &adj_path);
    filesystem *fs = GetFilesystem(fsid);
	int err = fs->ops.file_open(fs->fs_data, file, adj_path, mode);
    file->filesystem_id = fsid;
    return err;
}

// Returns 0 on success, non-zero error code on error.
int file_seek(FILE *file, uint32_t offset) {
    filesystem *fs = GetFilesystem(file->filesystem_id);
    return fs->ops.file_seek(fs->fs_data, file, offset);
}

// Returns 0 on error, bytes read on success.
int file_read(FILE *file, uint8_t *dest, uint32_t len) {
    filesystem *fs = GetFilesystem(file->filesystem_id);
    return fs->ops.file_read(fs->fs_data, file, dest, len);
}

// Returns 0 on error, bytes written on success.
int file_write(FILE *file, uint8_t *src, uint32_t len) {
    filesystem *fs = GetFilesystem(file->filesystem_id);
    return fs->ops.file_write(fs->fs_data, file, src, len);
}

void file_close(FILE *file) {
    filesystem *fs = GetFilesystem(file->filesystem_id);
    return fs->ops.file_close(fs->fs_data, file);
}

// Returns 0 on success, non-zero error code on error. Fills provided struct DIR
int dir_open(DIR *dir, char *path) {
    char *adj_path;
	uint8_t fsid = GetFsFromPath(path, &adj_path);
    filesystem *fs = GetFilesystem(fsid);
    int err = fs->ops.dir_open(fs->fs_data, dir, adj_path);
    dir->filesystem_id = fsid;
    return err;
}

// Return 0 on success, non-zero error code on error. Fills provided struct dirent.
int dir_nextentry(DIR *dir, dirent *ent) {
    filesystem *fs = GetFilesystem(dir->filesystem_id);
    return fs->ops.dir_nextentry(fs->fs_data, dir, ent);
}

void dir_close(DIR *dir) {
    filesystem *fs = GetFilesystem(dir->filesystem_id);
    return fs->ops.dir_close(fs->fs_data, dir);
}

// Returns 0 on success, non-zero error code on error. Fills provided struct dirent.
int path_getinfo(char *path, dirent *ent) {
    char *adj_path;
	filesystem *fs = GetFilesystem(GetFsFromPath(path, &adj_path));
    return fs->ops.path_getinfo(fs->fs_data, adj_path, ent);
}

// Returns 0 on success, non-zero error code on error. 
int path_mkdir(char *path) {
    char *adj_path;
	filesystem *fs = GetFilesystem(GetFsFromPath(path, &adj_path));
    return fs->ops.path_mkdir(fs->fs_data, adj_path);
}

// Returns 0 on success, non-zero error code on error. 
int path_rmdir(char *path) {
    char *adj_path;
	filesystem *fs = GetFilesystem(GetFsFromPath(path, &adj_path));
    return fs->ops.path_rmdir(fs->fs_data, adj_path);
}

// Returns 0 on success, non-zero error code on error. 
int path_rmfile(char *path) {
    char *adj_path;
	filesystem *fs = GetFilesystem(GetFsFromPath(path, &adj_path));
    return fs->ops.path_rmfile(fs->fs_data, adj_path);
}
