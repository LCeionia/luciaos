#pragma once
#include <stdint.h>

typedef struct FILE {
    uint8_t filesystem_id;
    uint8_t bytes[0x3F];
} __attribute__((__packed__)) FILE;

typedef struct DIR {
    uint8_t filesystem_id;
    uint8_t bytes[0x3F];
} __attribute__((__packed__)) DIR;

typedef enum filetype {
    FT_UNKNOWN,
    FT_REG,
    FT_DIR
} filetype;

typedef struct dirent {
    filetype type;
    uint32_t size;
    uint32_t last_modified;
    uint32_t last_accessed;
    uint32_t created;
    uint8_t namelen;
    char name[255];
} dirent;

#define OPENREAD 1
#define OPENWRITE 2
