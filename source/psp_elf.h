
#ifndef PSP_ELF_H
#define PSP_ELF_H

#include <stdint.h>

#define PSP_ELF_MAX_SEGMENTS 16

typedef struct {
    uint32_t offset;
    uint32_t virtualAddress;
    uint32_t fileSize;
    uint32_t memorySize;
    uint32_t flags;
    uint32_t alignment;
} PSPELFSegment;

typedef struct {
    uint32_t entryPoint;
    uint32_t fileSize;
    uint16_t machine;
    uint16_t segmentCount;
    PSPELFSegment segments[PSP_ELF_MAX_SEGMENTS];
} PSPELFInfo;

typedef enum {
    PSP_ELF_OK = 0,
    PSP_ELF_ERROR_OPEN = -1,
    PSP_ELF_ERROR_HEADER = -2,
    PSP_ELF_ERROR_FORMAT = -3,
    PSP_ELF_ERROR_MACHINE = -4,
    PSP_ELF_ERROR_SEGMENTS = -5,
    PSP_ELF_ERROR_READ = -6
} PSPELFError;

PSPELFError pspElfInspect(
    const char *path,
    PSPELFInfo *info
);

#endif
