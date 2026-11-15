
#include "psp_cpu.h"
#include <string.h>

static uint32_t getReg(PSPCPU *cpu, unsigned index) {
    return index == 0 ? 0 : cpu->r[index & 31];
}

void pspCpuReset(PSPCPU *cpu, uint32_t entryPoint) {
    memset(cpu, 0, sizeof(PSPCPU));
    cpu->pc = entryPoint;
    cpu->halted = false;
    cpu->exception = 0;
}

int pspCpuStep(PSPCPU *cpu, uint32_t *memory, uint32_t memorySize) {
    if (!cpu || !memory || cpu->halted)
        return -1;

    if (cpu->pc & 3)
        return -2;

    uint32_t index = cpu->pc / 4;

    if (index >= memorySize / 4)
        return -3;

    uint32_t instruction = memory[index];

    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 31;
    uint32_t rt = (instruction >> 16) & 31;
    uint32_t rd = (instruction >> 11) & 31;
    uint32_t shift = (instruction >> 6) & 31;
    uint32_t funct = instruction & 63;
    int32_t immediate = (int16_t)(instruction & 0xFFFF);

    uint32_t a = getReg(cpu, rs);
    uint32_t b = getReg(cpu, rt);
    uint32_t nextPC = cpu->pc + 4;

    switch (opcode) {
        case 0:
            switch (funct) {
                case 0x00: cpu->r[rd] = b << shift; break;
                case 0x02: cpu->r[rd] = b >> shift; break;
                case 0x08: nextPC = a; break;
                case 0x21: cpu->r[rd] = a + b; break;
                case 0x23: cpu->r[rd] = a - b; break;
                case 0x24: cpu->r[rd] = a & b; break;
                case 0x25: cpu->r[rd] = a | b; break;
                case 0x26: cpu->r[rd] = a ^ b; break;
                case 0x0C:
                case 0x0D:
                    cpu->halted = true;
                    cpu->exception = (int)funct;
                    break;
                default:
                    cpu->exception = 1;
                    return -4;
            }
            break;

        case 0x09:
            cpu->r[rt] = a + immediate;
            break;

        case 0x0D:
            cpu->r[rt] = a | (instruction & 0xFFFF);
            break;

        case 0x0F:
            cpu->r[rt] = (instruction & 0xFFFF) << 16;
            break;

        case 0x04:
            if (a == b)
                nextPC += immediate * 4;
            break;

        case 0x05:
            if (a != b)
                nextPC += immediate * 4;
            break;

        case 0x02:
            nextPC = (nextPC & 0xF0000000) |
                     ((instruction & 0x03FFFFFF) << 2);
            break;

        case 0x03:
            cpu->r[31] = cpu->pc + 8;
            nextPC = (nextPC & 0xF0000000) |
                     ((instruction & 0x03FFFFFF) << 2);
            break;

        default:
            cpu->exception = 1;
            return -4;
    }

    cpu->r[0] = 0;
    cpu->pc = nextPC;
    cpu->instructions++;

    return 0;
}

int pspCpuRun(PSPCPU *cpu, uint32_t *memory,
              uint32_t memorySize, uint32_t maxInstructions) {
    if (!cpu || !memory)
        return -1;

    for (uint32_t i = 0; i < maxInstructions; i++) {
        if (cpu->halted)
            break;

        int result = pspCpuStep(cpu, memory, memorySize);

        if (result != 0)
            return result;
    }

    return 0;
}
