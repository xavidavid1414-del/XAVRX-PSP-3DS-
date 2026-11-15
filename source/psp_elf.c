
#include "psp_elf.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define ELF_HEADER_SIZE 52
#define ELF_PROGRAM_HEADER_SIZE 32
#define ELF_PT_LOAD 1
#define ELF_MACHINE_MIPS 8

static uint16_t readLE16(const uint8_t *p) {
    return (uint16_t)p[0] |
           ((uint16_t)p[1] << 8);
}

static uint32_t readLE32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

PSPELFError pspElfInspect(
    const char *path,
    PSPELFInfo *info
) {
    if (!path || !info)
        return PSP_ELF_ERROR_HEADER;

    memset(info, 0, sizeof(PSPELFInfo));

    FILE *file = fopen(path, "rb");

    if (!file)
        return PSP_ELF_ERROR_OPEN;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return PSP_ELF_ERROR_READ;
    }

    long length = ftell(file);

    if (length < ELF_HEADER_SIZE || (unsigned long)length > UINT32_MAX) {
        fclose(file);
        return PSP_ELF_ERROR_HEADER;
    }

    uint32_t fileSize = (uint32_t)length;

    uint8_t header[ELF_HEADER_SIZE];

    if (fseek(file, 0, SEEK_SET) != 0 ||
        fread(header, 1, ELF_HEADER_SIZE, file) != ELF_HEADER_SIZE) {
        fclose(file);
        return PSP_ELF_ERROR_READ;
    }

    if (header[0] != 0x7F ||
        header[1] != 'E' ||
        header[2] != 'L' ||
        header[3] != 'F') {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    if (header[4] != 1 ||
        header[5] != 1 ||
        header[6] != 1) {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    uint16_t type = readLE16(&header[16]);
    uint16_t machine = readLE16(&header[18]);

    if (type != 2 && type != 3) {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    if (machine != ELF_MACHINE_MIPS) {
        fclose(file);
        return PSP_ELF_ERROR_MACHINE;
    }

    uint32_t entryPoint = readLE32(&header[24]);
    uint32_t programOffset = readLE32(&header[28]);

    uint16_t programEntrySize = readLE16(&header[42]);
    uint16_t programCount = readLE16(&header[44]);

    if (programEntrySize < ELF_PROGRAM_HEADER_SIZE ||
        programCount > PSP_ELF_MAX_SEGMENTS ||
        programOffset > fileSize ||
        (uint64_t)programOffset +
        (uint64_t)programEntrySize * programCount > fileSize) {
        fclose(file);
        return PSP_ELF_ERROR_SEGMENTS;
    }

    info->entryPoint = entryPoint;
    info->fileSize = fileSize;
    info->machine = machine;

    for (uint16_t i = 0; i < programCount; i++) {
        uint8_t ph[ELF_PROGRAM_HEADER_SIZE];

        uint32_t position =
            programOffset + (uint32_t)i * programEntrySize;

        if (fseek(file, (long)position, SEEK_SET) != 0 ||
            fread(ph, 1, ELF_PROGRAM_HEADER_SIZE, file)
                != ELF_PROGRAM_HEADER_SIZE) {
            fclose(file);
            return PSP_ELF_ERROR_READ;
        }

        uint32_t segmentType = readLE32(&ph[0]);

        if (segmentType != ELF_PT_LOAD)
            continue;

        PSPELFSegment segment;

        segment.offset = readLE32(&ph[4]);
        segment.virtualAddress = readLE32(&ph[8]);
        segment.fileSize = readLE32(&ph[16]);
        segment.memorySize = readLE32(&ph[20]);
        segment.flags = readLE32(&ph[24]);
        segment.alignment = readLE32(&ph[28]);

        if (segment.memorySize < segment.fileSize ||
            (uint64_t)segment.offset + segment.fileSize > fileSize) {
            fclose(file);
            return PSP_ELF_ERROR_SEGMENTS;
        }

        if (info->segmentCount >= PSP_ELF_MAX_SEGMENTS) {
            fclose(file);
            return PSP_ELF_ERROR_SEGMENTS;
        }

        info->segments[info->segmentCount++] = segment;
    }

    fclose(file);

    return info->segmentCount > 0
        ? PSP_ELF_OK
        : PSP_ELF_ERROR_SEGMENTS;
}
