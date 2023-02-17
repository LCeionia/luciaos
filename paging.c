#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t page_tables[4][1024] __attribute__((aligned(4096)));

void enable_paging() {
    asm(
        "mov %%eax, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
    ::"a"(page_directory));
}

// TODO Add a function which can dynamically add and remove pages,
// that keeps track internally of what's already used?

// NOTE This function cannot cross 4MB (1024 page) boundaries
void map_pages(uintptr_t page, uintptr_t num_pages, uintptr_t phy_address, char access_flags) {
    uintptr_t access = access_flags & 0x7;

    // Each page is 4K, Each page table is 4M
    uintptr_t page_dir_entry = page >> 10;
    if (page_dir_entry >= 4) return;
    uintptr_t page_index = page & 0x3ff;
    if (page_index + num_pages > 1024) return;

    // Address must be 4K aligned
    if (phy_address & 0x3ff) return;

    for (int i = 0; i < num_pages; i++)
        page_tables[page_dir_entry][page_index + i] = phy_address + (i * 0x1000) | access_flags;
}

// User, R/W, Present
#define USERACCESS (4|2|1)
// Supervisor, R/W, Present
#define SUPERACCESS (2|1)

void init_paging() {
    for (int i = 0; i < 1024; i++)
        // Supervisor, R/W, Not Present
        page_directory[i] = 2;

    // 1024 Pages = 1MB
    // First MB: Real Mode
    // TODO Make some of this not accessible to usermode?
    map_pages(0x00000000 >> 12, 256, 0x00000000, USERACCESS);
    // Next 3MB: Kernel
    map_pages(0x00100000 >> 12, 768, 0x00100000, SUPERACCESS);
    // Next 4MB: Kernel
    map_pages(0x00400000 >> 12, 1024, 0x00400000, SUPERACCESS);

    // Next 8MB: Usermode
    map_pages(0x00800000 >> 12, 1024, 0x00800000, USERACCESS);
    map_pages(0x00C00000 >> 12, 1024, 0x00C00000, USERACCESS);

    // We aren't using page directory permissions
    for (int i = 0; i < 4; i++)
        page_directory[i] = ((uintptr_t)&page_tables[i]) | USERACCESS;

    enable_paging();
}
