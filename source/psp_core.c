
#include "psp_core.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define ELF_HEADER_SIZE 52
#define ELF_PROGRAM_HEADER_SIZE 32
#define ELF_PT_LOAD 1
#define ELF_MACHINE_MIPS 8
#define ELF_MAX_SEGMENTS 64

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)p[0] |
           ((uint16_t)p[1] << 8);
}

static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int pspSystemInit(PSPSystem *system) {
    if (system == NULL)
        return PSP_CORE_ERROR;

    memset(system, 0, sizeof(*system));

    if (pspMemoryInit(&system->memory) != 0)
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

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return PSP_CORE_FILE_ERROR;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return PSP_CORE_FILE_ERROR;
    }

    long fileLength = ftell(file);

    if (fileLength < ELF_HEADER_SIZE) {
        fclose(file);
        return PSP_CORE_INVALID_ELF;
    }

    rewind(file);

    uint8_t header[ELF_HEADER_SIZE];

    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return PSP_CORE_FILE_ERROR;
    }

    if (header[0] != 0x7F ||
        header[1] != 'E' ||
        header[2] != 'L' ||
        header[3] != 'F') {
        fclose(file);
        return PSP_CORE_INVALID_ELF;
    }

    if (header[4] != 1 || header[5] != 1) {
        fclose(file);
        return PSP_CORE_UNSUPPORTED;
    }

    if (read16(&header[18]) != ELF_MACHINE_MIPS) {
        fclose(file);
        return PSP_CORE_UNSUPPORTED;
    }

    uint16_t elfType = read16(&header[16]);

    if (elfType != 2 && elfType != 3) {
        fclose(file);
        return PSP_CORE_UNSUPPORTED;
    }

    uint32_t entry = read32(&header[24]);
    uint32_t phoff = read32(&header[28]);

    uint16_t phentsize = read16(&header[42]);
    uint16_t phnum = read16(&header[44]);

    if (phentsize < ELF_PROGRAM_HEADER_SIZE ||
        phnum == 0 ||
        phnum > ELF_MAX_SEGMENTS) {
        fclose(file);
        return PSP_CORE_INVALID_ELF;
    }

    uint64_t tableEnd =
        (uint64_t)phoff +
        (uint64_t)phentsize * phnum;

    if (tableEnd > (uint64_t)fileLength) {
        fclose(file);
        return PSP_CORE_INVALID_ELF;
    }

    unsigned loadedSegments = 0;

    for (uint16_t i = 0; i < phnum; i++) {
        uint64_t position =
            (uint64_t)phoff +
            (uint64_t)i * phentsize;

        if (position > LONG_MAX ||
            fseek(file, (long)position, SEEK_SET) != 0) {
            fclose(file);
            return PSP_CORE_FILE_ERROR;
        }

        uint8_t ph[ELF_PROGRAM_HEADER_SIZE];

        if (fread(ph, 1, sizeof(ph), file) != sizeof(ph)) {
            fclose(file);
            return PSP_CORE_FILE_ERROR;
        }

        if (read32(&ph[0]) != ELF_PT_LOAD)
            continue;

        uint32_t offset = read32(&ph[4]);
        uint32_t address = read32(&ph[8]);
        uint32_t fileSize = read32(&ph[16]);
        uint32_t memorySize = read32(&ph[20]);

        if (fileSize > memorySize || memorySize == 0) {
            fclose(file);
            return PSP_CORE_INVALID_ELF;
        }

        if ((uint64_t)offset + fileSize > (uint64_t)fileLength) {
            fclose(file);
            return PSP_CORE_INVALID_ELF;
        }

        uint8_t *destination = pspMemoryTranslate(
            &system->memory,
            address,
            memorySize
        );

        if (destination == NULL) {
            fclose(file);
            return PSP_CORE_MEMORY_ERROR;
        }

        memset(destination, 0, memorySize);

        if (fileSize > 0) {
            if (offset > LONG_MAX ||
                fseek(file, (long)offset, SEEK_SET) != 0 ||
                fread(destination, 1, fileSize, file) != fileSize) {
                fclose(file);
                return PSP_CORE_FILE_ERROR;
            }
        }

        loadedSegments++;
    }

    fclose(file);

    if (loadedSegments == 0)
        return PSP_CORE_INVALID_ELF;

    if (pspMemoryTranslate(&system->memory, entry, 4) == NULL)
        return PSP_CORE_MEMORY_ERROR;

    pspCpuReset(&system->cpu, entry);

    system->programLoaded = true;

    return PSP_CORE_OK;
}

int pspSystemStep(PSPSystem *system) {
    if (system == NULL ||
        !system->initialized ||
        !system->programLoaded) {
        return PSP_CORE_ERROR;
    }

    return pspCpuStep(&system->cpu, &system->memory);
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
