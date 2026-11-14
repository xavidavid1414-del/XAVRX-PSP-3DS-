
#include "psp_cpu.h"

void pspCpuReset(PSPCPU *cpu, uint32_t entry)
{
    for (int i = 0; i < 32; i++)
        cpu->r[i] = 0;

    cpu->pc = entry;
    cpu->hi = 0;
    cpu->lo = 0;
    cpu->instructions = 0;
    cpu->halted = 0;
    cpu->exception = 0;
}

int pspCpuStep(PSPCPU *cpu, const uint32_t *program,
               size_t programWords)
{
    if (cpu->halted || cpu->pc % 4 != 0)
        return 0;

    size_t index = cpu->pc / 4;

    if (index >= programWords)
    {
        cpu->exception = 1;
        cpu->halted = 1;
        return 0;
    }

    uint32_t instruction = program[index];

    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 31;
    uint32_t rt = (instruction >> 16) & 31;
    uint32_t rd = (instruction >> 11) & 31;
    uint32_t shamt = (instruction >> 6) & 31;
    uint32_t funct = instruction & 63;
    int32_t immediate = (int16_t)(instruction & 0xFFFF);

    uint32_t nextPC = cpu->pc + 4;
    uint32_t a = cpu->r[rs];
    uint32_t b = cpu->r[rt];

    switch (opcode)
    {
        case 0:
            switch (funct)
            {
                case 0x00: // SLL
                    cpu->r[rd] = b << shamt;
                    break;

                case 0x02: // SRL
                    cpu->r[rd] = b >> shamt;
                    break;

                case 0x08: // JR
                    nextPC = a;
                    break;

                case 0x21: // ADDU
                    cpu->r[rd] = a + b;
                    break;

                case 0x23: // SUBU
                    cpu->r[rd] = a - b;
                    break;

                case 0x24: // AND
                    cpu->r[rd] = a & b;
                    break;

                case 0x25: // OR
                    cpu->r[rd] = a | b;
                    break;

                case 0x26: // XOR
                    cpu->r[rd] = a ^ b;
                    break;

                case 0x0C: // SYSCALL
                case 0x0D: // BREAK
                    cpu->exception = 1;
                    cpu->halted = 1;
                    return 0;

                default:
                    cpu->exception = 1;
                    cpu->halted = 1;
                    return 0;
            }
            break;

        case 0x09: // ADDIU
            cpu->r[rt] = a + (uint32_t)immediate;
            break;

        case 0x0D: // ORI
            cpu->r[rt] = a | (instruction & 0xFFFF);
            break;

        case 0x0F: // LUI
            cpu->r[rt] = (instruction & 0xFFFF) << 16;
            break;

        case 0x04: // BEQ
            if (a == b)
                nextPC += (uint32_t)(immediate * 4);
            break;

        case 0x05: // BNE
            if (a != b)
                nextPC += (uint32_t)(immediate * 4);
            break;

        case 0x02: // J
            nextPC = (nextPC & 0xF0000000) |
                     ((instruction & 0x03FFFFFF) << 2);
            break;

        case 0x03: // JAL
            cpu->r[31] = nextPC + 4;
            nextPC = (nextPC & 0xF0000000) |
                     ((instruction & 0x03FFFFFF) << 2);
            break;

        default:
            cpu->exception = 1;
            cpu->halted = 1;
            return 0;
    }

    cpu->r[0] = 0;
    cpu->pc = nextPC;
    cpu->instructions++;

    return 1;
}

int pspCpuRun(PSPCPU *cpu, const uint32_t *program,
              size_t programWords, unsigned int maxSteps)
{
    unsigned int executed = 0;

    while (executed < maxSteps && !cpu->halted)
    {
        if (!pspCpuStep(cpu, program, programWords))
            break;

        executed++;
    }

    return (int)executed;
}
