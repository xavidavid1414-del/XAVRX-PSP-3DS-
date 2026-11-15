
#include "psp_core.h"
#include "psp_elf.h"

#include <string.h>

int pspSystemInit(PSPSystem *system) {
    if (system == NULL)
        return PSP_CORE_ERROR;

    memset(system, 0, sizeof(*system));

    if (!pspMemoryInit(&system->memory))
        return PSP_CORE_MEMORY_ERROR;

    pspCpuReset(&system->cpu, 0);

    system->initialized = true;
    system->programLoaded = false;

    return PSP_CORE_OK;
}

void pspSystemDestroy(PSPSystem *system) {
    if (system == NULL)
        return;

    if (system->initialized)
        pspMemoryDestroy(&system->memory);

    memset(system, 0, sizeof(*system));
}

int pspSystemLoadELF(PSPSystem *system, const char *path) {
    if (system == NULL || path == NULL)
        return PSP_CORE_ERROR;

    if (!system->initialized)
        return PSP_CORE_ERROR;

    system->programLoaded = false;

    PSPELFInfo info;

    PSPELFError error = pspElfLoad(
        path,
        &system->memory,
        &info
    );

    if (error != PSP_ELF_OK) {
        switch (error) {
            case PSP_ELF_ERROR_OPEN:
            case PSP_ELF_ERROR_READ:
                return PSP_CORE_FILE_ERROR;

            case PSP_ELF_ERROR_MEMORY:
                return PSP_CORE_MEMORY_ERROR;

            case PSP_ELF_ERROR_FORMAT:
            case PSP_ELF_ERROR_HEADER:
            case PSP_ELF_ERROR_MACHINE:
            case PSP_ELF_ERROR_SEGMENTS:
            default:
                return PSP_CORE_INVALID_ELF;
        }
    }

    pspCpuReset(&system->cpu, info.entryPoint);

    system->programLoaded = true;

    return PSP_CORE_OK;
}

int pspSystemStep(PSPSystem *system) {
    if (system == NULL ||
        !system->initialized ||
        !system->programLoaded) {
        return PSP_CORE_ERROR;
    }

    return pspCpuStep(
        &system->cpu,
        &system->memory
    );
}

int pspSystemRun(PSPSystem *system, uint32_t maxInstructions) {
    if (system == NULL ||
        !system->initialized ||
        !system->programLoaded) {
        return PSP_CORE_ERROR;
    }

    return pspCpuRun(
        &system->cpu,
        &system->memory,
        maxInstructions
    );
}
