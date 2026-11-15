
#ifndef PSP_CPU_H
#define PSP_CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "psp_memory.h"

typedef struct {
    uint32_t r[32];

    uint32_t pc;
    uint32_t hi;
    uint32_t lo;

    uint32_t branchTarget;
    bool branchPending;

    uint64_t instructions;

    bool halted;
    int exception;
} PSPCPU;

void pspCpuReset(PSPCPU *cpu, uint32_t entryPoint);

int pspCpuStep(PSPCPU *cpu, PSPMemory *memory);

int pspCpuRun(
    PSPCPU *cpu,
    PSPMemory *memory,
    uint32_t maxInstructions
);

#endif
