#ifndef PSP_CORE_H
#define PSP_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include "psp_memory.h"
#include "psp_cpu.h"

typedef struct {
    PSPMemory memory;
    PSPCPU cpu;
    bool initialized;
    bool programLoaded;
} PSPSystem;

enum {
    PSP_CORE_OK = 0,
    PSP_CORE_ERROR = -1,
    PSP_CORE_INVALID_ELF = -2,
    PSP_CORE_UNSUPPORTED = -3,
    PSP_CORE_MEMORY_ERROR = -4,
    PSP_CORE_FILE_ERROR = -5
};

int pspSystemInit(PSPSystem *system);
void pspSystemDestroy(PSPSystem *system);

int pspSystemLoadELF(
    PSPSystem *system,
    const char *path
);

int pspSystemStep(PSPSystem *system);

int pspSystemRun(
    PSPSystem *system,
    uint32_t maxInstructions
);

#endif
