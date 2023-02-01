#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096))); // 0x00000000 - 0x00400000
uint32_t second_page_table[1024] __attribute__((aligned(4096))); // 0x00400000 - 0x00800000

void enable_paging() {
    asm(
        "mov %%eax, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
    ::"a"(page_directory));
}

void init_paging() {
    for (int i = 0; i < 1024; i++)
        // Supervisor, R/W, Not Present
        page_directory[i] = 2;

    // First Page Table
    // First MB: Real Mode
    {
        int i;
        // Up to 0xC0000
        // TODO make some areas here Read Only
        for (i = 0;i < 16*0xC; i++)
            // User, R/W, Present
            first_page_table[i] = (i * 0x1000) |4|2|1;
        // Remainder of first MB BIOS Area (writable?)
        for (;i < 256; i++)
            // User, R/W, Present
            first_page_table[i] = (i * 0x1000) |4|2|1;
    }
    // Next 3MB: Kernel
    for (int i = 256; i < 1024; i++)
        // Supervisor, R/W, Present
        first_page_table[i] = (i * 0x1000) |2|1;

    // Usermode Page Table
    for (int i = 0; i < 1024; i++)
        // User, R/W, Present
        second_page_table[i] = (i * 0x1000 + 0x400000) |4|2|1;

    // User, R/W, Present
    page_directory[0] = ((uintptr_t)first_page_table)|4|2|1;
    page_directory[1] = ((uintptr_t)second_page_table)|4|2|1;

    enable_paging();
}
