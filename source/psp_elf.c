
#include "psp_elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define ELF_HEADER_SIZE 52
#define ELF_PROGRAM_HEADER_SIZE 32
#define ELF_PT_LOAD 1
#define ELF_MACHINE_MIPS 8

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)p[0] |
           ((uint16_t)p[1] << 8);
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int readAt(
    FILE *file,
    uint32_t offset,
    void *buffer,
    size_t size
)
{
    if (fseek(file, (long)offset, SEEK_SET) != 0)
        return 0;

    return fread(buffer, 1, size, file) == size;
}

static int getFileSize(FILE *file, uint32_t *size)
{
    if (fseek(file, 0, SEEK_END) != 0)
        return 0;

    long length = ftell(file);

    if (length < ELF_HEADER_SIZE ||
        (unsigned long)length > UINT32_MAX)
        return 0;

    *size = (uint32_t)length;
    return 1;
}

static int validFileRange(
    uint32_t offset,
    uint32_t size,
    uint32_t fileSize
)
{
    return (uint64_t)offset + size <= fileSize;
}

PSPELFError pspElfInspect(
    const char *path,
    PSPELFInfo *info
)
{
    if (path == NULL || info == NULL)
        return PSP_ELF_ERROR_HEADER;

    memset(info, 0, sizeof(*info));

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return PSP_ELF_ERROR_OPEN;

    uint32_t fileSize;

    if (!getFileSize(file, &fileSize))
    {
        fclose(file);
        return PSP_ELF_ERROR_HEADER;
    }

    uint8_t header[ELF_HEADER_SIZE];

    if (!readAt(file, 0, header, sizeof(header)))
    {
        fclose(file);
        return PSP_ELF_ERROR_READ;
    }

    if (header[0] != 0x7F ||
        header[1] != 'E' ||
        header[2] != 'L' ||
        header[3] != 'F')
    {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    if (header[4] != 1 ||
        header[5] != 1 ||
        header[6] != 1)
    {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    uint16_t type = read16(header + 16);
    uint16_t machine = read16(header + 18);
    uint32_t version = read32(header + 20);

    if (machine != ELF_MACHINE_MIPS)
    {
        fclose(file);
        return PSP_ELF_ERROR_MACHINE;
    }

    if ((type != 2 && type != 3) || version != 1)
    {
        fclose(file);
        return PSP_ELF_ERROR_FORMAT;
    }

    uint32_t entryPoint = read32(header + 24);
    uint32_t programOffset = read32(header + 28);

    uint16_t headerSize = read16(header + 40);
    uint16_t programEntrySize = read16(header + 42);
    uint16_t programCount = read16(header + 44);

    if (headerSize < ELF_HEADER_SIZE ||
        programEntrySize < ELF_PROGRAM_HEADER_SIZE ||
        programCount == 0 ||
        programCount > 128)
    {
        fclose(file);
        return PSP_ELF_ERROR_HEADER;
    }

    uint64_t tableEnd =
        (uint64_t)programOffset +
        (uint64_t)programEntrySize * programCount;

    if (tableEnd > fileSize)
    {
        fclose(file);
        return PSP_ELF_ERROR_SEGMENTS;
    }

    info->entryPoint = entryPoint;
    info->fileSize = fileSize;
    info->machine = machine;

    int executableEntryFound = 0;

    for (uint16_t i = 0; i < programCount; i++)
    {
        uint64_t position =
            (uint64_t)programOffset +
            (uint64_t)i * programEntrySize;

        if (position > UINT32_MAX)
        {
            fclose(file);
            return PSP_ELF_ERROR_SEGMENTS;
        }

        uint8_t ph[ELF_PROGRAM_HEADER_SIZE];

        if (!readAt(
            file,
            (uint32_t)position,
            ph,
            sizeof(ph)
        ))
        {
            fclose(file);
            return PSP_ELF_ERROR_READ;
        }

        if (read32(ph) != ELF_PT_LOAD)
            continue;

        if (info->segmentCount >= PSP_ELF_MAX_SEGMENTS)
        {
            fclose(file);
            return PSP_ELF_ERROR_SEGMENTS;
        }

        PSPELFSegment *segment =
            &info->segments[info->segmentCount];

        segment->offset = read32(ph + 4);
        segment->virtualAddress = read32(ph + 8);
        segment->fileSize = read32(ph + 16);
        segment->memorySize = read32(ph + 20);
        segment->flags = read32(ph + 24);
        segment->alignment = read32(ph + 28);

        if (segment->fileSize > segment->memorySize ||
            !validFileRange(
                segment->offset,
                segment->fileSize,
                fileSize
            ) ||
            (uint64_t)segment->virtualAddress +
            segment->memorySize > UINT32_MAX)
        {
            fclose(file);
            return PSP_ELF_ERROR_SEGMENTS;
        }

        if (segment->memorySize == 0)
            continue;

        if ((segment->flags & 1) &&
            entryPoint >= segment->virtualAddress &&
            (uint64_t)entryPoint <
            (uint64_t)segment->virtualAddress +
            segment->memorySize)
        {
            executableEntryFound = 1;
        }

        info->segmentCount++;
    }

    fclose(file);

    if (info->segmentCount == 0)
        return PSP_ELF_ERROR_SEGMENTS;

    if (!executableEntryFound)
        return PSP_ELF_ERROR_FORMAT;

    return PSP_ELF_OK;
}

PSPELFError pspElfLoad(
    const char *path,
    PSPMemory *memory,
    PSPELFInfo *info
)
{
    if (path == NULL || memory == NULL || info == NULL)
        return PSP_ELF_ERROR_HEADER;

    PSPELFInfo parsed;

    PSPELFError result =
        pspElfInspect(path, &parsed);

    if (result != PSP_ELF_OK)
        return result;

    if (!memory->initialized)
        return PSP_ELF_ERROR_MEMORY;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return PSP_ELF_ERROR_OPEN;

    /* Validar todos los segmentos antes de escribir */

    for (uint16_t i = 0; i < parsed.segmentCount; i++)
    {
        PSPELFSegment *segment = &parsed.segments[i];

        if (pspMemoryTranslate(
            memory,
            segment->virtualAddress,
            segment->memorySize
        ) == NULL)
        {
            fclose(file);
            return PSP_ELF_ERROR_MEMORY;
        }
    }

    /* Cargar segmentos y limpiar BSS */

    for (uint16_t i = 0; i < parsed.segmentCount; i++)
    {
        PSPELFSegment *segment = &parsed.segments[i];

        uint8_t *destination = pspMemoryTranslate(
            memory,
            segment->virtualAddress,
            segment->memorySize
        );

        if (destination == NULL)
        {
            fclose(file);
            return PSP_ELF_ERROR_MEMORY;
        }

        if (segment->fileSize > 0)
        {
            if (!readAt(
                file,
                segment->offset,
                destination,
                segment->fileSize
            ))
            {
                fclose(file);
                return PSP_ELF_ERROR_READ;
            }
        }

        if (segment->memorySize > segment->fileSize)
        {
            memset(
                destination + segment->fileSize,
                0,
                segment->memorySize - segment->fileSize
            );
        }
    }

    fclose(file);

    *info = parsed;

    return PSP_ELF_OK;
}
