#ifndef PSP_TEST_H
#define PSP_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include "psp_cpu.h"

typedef struct {
    uint32_t instructionsExecuted;
    uint32_t finalPC;
    uint32_t result;
    int error;
    bool passed;
} PSPTestResult;

bool pspTestRunBasic(PSPMemory *memory, PSPTestResult *result);

#endif
