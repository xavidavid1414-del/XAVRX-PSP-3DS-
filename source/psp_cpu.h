
#ifndef PSP_CPU_H
#define PSP_CPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t r[32];
    uint32_t pc;
    uint32_t hi;
    uint32_t lo;
    uint64_t instructions;
    bool halted;
    int exception;
} PSPCPU;

void pspCpuReset(PSPCPU *cpu, uint32_t entryPoint);

int pspCpuStep(
    PSPCPU *cpu,
    uint32_t *memory,
    uint32_t memorySize
);

int pspCpuRun(
    PSPCPU *cpu,
    uint32_t *memory,
    uint32_t memorySize,
    uint32_t maxInstructions
);

#endif
