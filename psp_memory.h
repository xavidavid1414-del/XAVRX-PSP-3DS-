
#ifndef PSP_MEMORY_H
#define PSP_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PSP_RAM_BASE 0x08000000
#define PSP_RAM_SIZE (32 * 1024 * 1024)

typedef struct {
    uint8_t *ram;
    uint32_t base;
    uint32_t size;
    bool initialized;
} PSPMemory;

bool pspMemoryInit(PSPMemory *memory);
void pspMemoryDestroy(PSPMemory *memory);

bool pspMemoryRead8(PSPMemory *memory, uint32_t address, uint8_t *value);
bool pspMemoryRead16(PSPMemory *memory, uint32_t address, uint16_t *value);
bool pspMemoryRead32(PSPMemory *memory, uint32_t address, uint32_t *value);

bool pspMemoryWrite8(PSPMemory *memory, uint32_t address, uint8_t value);
bool pspMemoryWrite16(PSPMemory *memory, uint32_t address, uint16_t value);
bool pspMemoryWrite32(PSPMemory *memory, uint32_t address, uint32_t value);

bool pspMemoryLoad(PSPMemory *memory, uint32_t address,
                   const void *data, size_t size);

uint8_t *pspMemoryTranslate(PSPMemory *memory, uint32_t address,
                            size_t size);

#endif
