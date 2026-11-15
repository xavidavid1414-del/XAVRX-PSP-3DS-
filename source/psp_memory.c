
#include "psp_memory.h"
#include <stdlib.h>
#include <string.h>

bool pspMemoryInit(PSPMemory *memory) {
    if (!memory)
        return false;

    memset(memory, 0, sizeof(*memory));

    memory->ram = (uint8_t *)calloc(1, PSP_RAM_SIZE);

    if (!memory->ram)
        return false;

    memory->base = PSP_RAM_BASE;
    memory->size = PSP_RAM_SIZE;
    memory->initialized = true;

    return true;
}

void pspMemoryDestroy(PSPMemory *memory) {
    if (!memory)
        return;

    free(memory->ram);
    memset(memory, 0, sizeof(*memory));
}

uint8_t *pspMemoryTranslate(PSPMemory *memory,
                            uint32_t address, size_t size) {
    if (!memory || !memory->initialized || !memory->ram)
        return NULL;

    if (address < memory->base)
        return NULL;

    uint64_t offset = (uint64_t)address - memory->base;

    if (offset > memory->size ||
        size > (uint64_t)memory->size - offset)
        return NULL;

    return memory->ram + offset;
}

bool pspMemoryRead8(PSPMemory *memory,
                    uint32_t address, uint8_t *value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 1);

    if (!p || !value)
        return false;

    *value = p[0];
    return true;
}

bool pspMemoryRead16(PSPMemory *memory,
                     uint32_t address, uint16_t *value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 2);

    if (!p || !value)
        return false;

    *value = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return true;
}

bool pspMemoryRead32(PSPMemory *memory,
                     uint32_t address, uint32_t *value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 4);

    if (!p || !value)
        return false;

    *value = (uint32_t)p[0] |
             ((uint32_t)p[1] << 8) |
             ((uint32_t)p[2] << 16) |
             ((uint32_t)p[3] << 24);

    return true;
}

bool pspMemoryWrite8(PSPMemory *memory,
                     uint32_t address, uint8_t value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 1);

    if (!p)
        return false;

    p[0] = value;
    return true;
}

bool pspMemoryWrite16(PSPMemory *memory,
                      uint32_t address, uint16_t value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 2);

    if (!p)
        return false;

    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;

    return true;
}

bool pspMemoryWrite32(PSPMemory *memory,
                      uint32_t address, uint32_t value) {
    uint8_t *p = pspMemoryTranslate(memory, address, 4);

    if (!p)
        return false;

    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
    p[2] = (value >> 16) & 0xFF;
    p[3] = (value >> 24) & 0xFF;

    return true;
}

bool pspMemoryLoad(PSPMemory *memory, uint32_t address,
                   const void *data, size_t size) {
    if (!data && size != 0)
        return false;

    uint8_t *destination =
        pspMemoryTranslate(memory, address, size);

    if (!destination)
        return false;

    memcpy(destination, data, size);
    return true;
}
