#include "disk.h"
#include "fs.h"
#include "dosfs/dosfs.h"

// Implementations for DOSFS

uint32_t DFS_ReadSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count) {
    return Disk_ReadSector(unit, buffer, sector, count);
}

uint32_t DFS_WriteSector(uint8_t unit, uint8_t *buffer, uint32_t sector, uint32_t count) {
    return Disk_WriteSector(unit, buffer, sector, count);
}


// System Implementations

typedef struct fsdat {
	VOLINFO vi;
} fsdat;

int file83ToPath(uint8_t *src, char *path) {
    uint8_t tmp, trailingSpace;
    for (trailingSpace=0, tmp = 0; tmp < 8 && src[tmp]; tmp++) {
        path[tmp] = src[tmp];
        if (src[tmp] == ' ') trailingSpace++;
        else trailingSpace = 0;
    }
    tmp -= trailingSpace;
    path[tmp++] = '.';
    trailingSpace = 0;
    for (int i = 8; i < 11 && src[i]; i++, tmp++) {
        path[tmp] = src[i];
        if (src[i] == ' ') trailingSpace++;
        else trailingSpace = 0;
    }
    tmp -= trailingSpace;
    if (trailingSpace == 3) tmp--;
    path[tmp] = 0;
    return tmp;
}

int dos_file_open(uint8_t *dat, FILE *f, char *path, char mode) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    uint8_t dfs_mode =
        (mode & OPENREAD ? DFS_READ : 0) |
        (mode & OPENWRITE ? DFS_WRITE : 0);
    return DFS_OpenFile(&fs->vi, (uint8_t *)path, dfs_mode, scratch, (FILEINFO *)f->bytes);
}

int dos_file_seek(uint8_t *dat, FILE *f, uint32_t offset) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    DFS_Seek((FILEINFO *)f->bytes, offset, scratch);
    if (((FILEINFO *)f->bytes)->pointer != offset) return -1;
    return 0;
}

int dos_file_read(uint8_t *dat, FILE *f, uint8_t *dest, uint32_t len) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    uint32_t successcount;
    uint32_t err = DFS_ReadFile((FILEINFO *)f->bytes, scratch, dest, &successcount, len);
    // Error
    if (err != 0 && err != DFS_EOF)
        return 0;
    // Success or EOF
    return successcount;
}

int dos_file_write(uint8_t *dat, FILE *f, uint8_t *src, uint32_t len) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    uint32_t successcount;
    uint32_t err = DFS_WriteFile((FILEINFO *)f->bytes, scratch, src, &successcount, len);
    // Error
    if (err != 0) return 0;
    // Success
    return successcount;
}

// DOSFS doesn't have anything to clean up
void dos_file_close(uint8_t *dat, FILE *f) { return; }

int dos_dir_open(uint8_t *dat, DIR *d, char *path) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    ((DIRINFO *)d->bytes)->scratch = scratch;
    return DFS_OpenDir(&fs->vi, (uint8_t *)path, (DIRINFO *)d->bytes);
}

int dos_dir_nextentry(uint8_t *dat, DIR *d, dirent *ent) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    DIRENT de;
    for (;;) {
        uint32_t code = DFS_GetNext(&fs->vi, (DIRINFO *)d->bytes, &de);
        if (code == DFS_EOF) return 1;
        if (code != DFS_OK) return -1;
        // Deleted file, continue to next entry
        if (de.name[0] == 0) continue;
        break;
    }
    // Copy info
    ent->type = de.attr & ATTR_DIRECTORY ? FT_DIR : FT_REG;
    ent->size = (uint32_t)de.filesize_0 +
                ((uint32_t)de.filesize_1 << 8) +
                ((uint32_t)de.filesize_2 << 16) +
                ((uint32_t)de.filesize_3 << 24);
    // Haven't decided format on these yet
    ent->last_modified = 0;
    ent->last_accessed = 0;
    ent->created = 0;
    
    ent->namelen = file83ToPath(de.name, ent->name);

    return 0;
}

// DOSFS doesn't have anything to clean up
void dos_dir_close(uint8_t *dat, DIR *d) { return; }

// TODO Unimplemented
int dos_path_getinfo(uint8_t *dat, char *path, dirent *d) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    return -1;
}

// TODO Unimplemented
int dos_path_mkdir(uint8_t *dat, char *path) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    return -1;
}

// TODO Unimplemented
int dos_path_rmdir(uint8_t *dat, char *path) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    return -1;
}

// TODO Unimplemented
int dos_path_rmfile(uint8_t *dat, char *path) {
    fsdat *fs = (fsdat *)dat;
    uint8_t *scratch = (uint8_t *)0x20000;
    return -1;
}

// DOSFS doesn't have anything to clean up
void dos_endfs(uint8_t *dat) { return; }

// Not yet decided
char DetectDosPart() {
    return 1;
}

int InitDosFs(filesystem *fs, char partition) {
    uint8_t *diskReadBuf = (uint8_t *)0x20000;
    uint8_t pactive, ptype;
    uint32_t pstart, psize;
    pstart = DFS_GetPtnStart(0, diskReadBuf, partition, &pactive, &ptype, &psize);
    if (pstart == -1) return -1;

    VOLINFO *vi = (VOLINFO *)fs->fs_data;
    if (DFS_GetVolInfo(0, diskReadBuf, pstart, (VOLINFO *)fs->fs_data)) {
        return -1;
    }

    int i;
    for (i = 0; vi->label[i] && i < sizeof(vi->label); i++)
        fs->label[i] = vi->label[i];
    fs->labellen = i;

	fs->ops.file_open = dos_file_open;
    fs->ops.file_seek = dos_file_seek;
    fs->ops.file_read = dos_file_read;
    fs->ops.file_write = dos_file_write;
    fs->ops.file_close = dos_file_close;
    fs->ops.dir_open = dos_dir_open;
    fs->ops.dir_nextentry = dos_dir_nextentry;
    fs->ops.dir_close = dos_dir_close;
    fs->ops.path_getinfo = dos_path_getinfo;
    fs->ops.path_mkdir = dos_path_mkdir;
    fs->ops.path_rmdir = dos_path_rmdir;
    fs->ops.path_rmfile = dos_path_rmfile;
    fs->ops.endfs = dos_endfs;

	return 0;
}
