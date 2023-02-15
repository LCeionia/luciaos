#pragma once 
#include "file_s.h"

// Returns 0 on success, non-zero error code on error. Fills provided struct FILE
int file_open(FILE *file, char *path, char mode);

// Returns 0 on success, non-zero error code on error.
int file_seek(FILE *file, uint32_t offset);

// Returns 0 on error, bytes read on success.
int file_read(FILE *file, uint8_t *dest, uint32_t len);

// Returns 0 on error, bytes written on success.
int file_write(FILE *file, uint8_t *src, uint32_t len);

void file_close(FILE *file);

// Returns 0 on success, non-zero error code on error. Fills provided struct DIR
int dir_open(DIR *dir, char *path);

// Return 0 on success, non-zero error code on error. Fills provided struct dirent.
int dir_nextentry(DIR *dir, dirent *ent);

void dir_close(DIR *dir);

// Returns 0 on success, non-zero error code on error. Fills provided struct dirent.
int path_getinfo(char *path, dirent *ent);

// Returns 0 on success, non-zero error code on error. 
int path_mkdir(char *path);

// Returns 0 on success, non-zero error code on error. 
int path_rmdir(char *path);

// Returns 0 on success, non-zero error code on error. 
int path_rmfile(char *path);
