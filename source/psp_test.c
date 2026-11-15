
#include "psp_test.h"
#include <string.h>

bool pspTestRunBasic(PSPMemory *memory, PSPTestResult *result) {
    if (!memory || !result)
        return false;

    memset(result, 0, sizeof(*result));

    /*
     * Programa MIPS de prueba:
     *
     * addiu $t0, $zero, 10
     * addiu $t1, $zero, 20
     * addu  $t2, $t0, $t1
     * break
     *
     * Resultado esperado: $t2 = 30
     */

    const uint32_t program[] = {
        0x2408000A,
        0x24090014,
        0x01095021,
        0x0000000D
    };

    const uint32_t entry = PSP_RAM_BASE + 0x1000;

    if (!pspMemoryLoad(
            memory,
            entry,
            program,
            sizeof(program))) {
        result->error = 1;
        return false;
    }

    PSPCPU cpu;
    pspCpuReset(&cpu, entry);

    int execution = pspCpuRun(&cpu, memory, 16);

    result->instructionsExecuted =
        (uint32_t)cpu.instructions;
    result->finalPC = cpu.pc;
    result->result = cpu.r[10];
    result->error = execution;

    result->passed =
        execution == 0 &&
        cpu.halted &&
        cpu.exception == 4 &&
        cpu.r[10] == 30 &&
        cpu.instructions == 4;

    return result->passed;
}
