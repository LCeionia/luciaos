#include <stdint.h>

#include "file_s.h"

typedef struct filesystem {
    uint32_t resv0;
    uint32_t type;
    struct fs_operations {
        int (*file_open)(uint8_t *, FILE *, char *, char);
        int (*file_seek)(uint8_t *, FILE *, uint32_t);
        int (*file_read)(uint8_t *, FILE *, uint8_t *, uint32_t);
        int (*file_write)(uint8_t *, FILE *, uint8_t *, uint32_t);
        void (*file_close)(uint8_t *, FILE *);
        int (*dir_open)(uint8_t *, DIR *, char *);
        int (*dir_nextentry)(uint8_t *, DIR *, dirent *);
        void (*dir_close)(uint8_t *, DIR *);
        int (*path_getinfo)(uint8_t *, char *, dirent *);
        int (*path_mkdir)(uint8_t *, char *);
        int (*path_rmdir)(uint8_t *, char *);
        int (*path_rmfile)(uint8_t *, char *);
        void (*endfs)(uint8_t *);
    } ops;
    uint8_t labellen;
    char label[255];
    uint8_t fs_data[2048-4-4-44-256];
} __attribute__((packed)) filesystem;

filesystem *GetFilesystem(uint8_t idx);
filesystem *GetActiveFilesystem();
uint8_t GetActiveFilesystemId();
filesystem *SetActiveFilesystem(uint8_t idx);
void ActiveFilesystemBitmap(char *bitmap);
