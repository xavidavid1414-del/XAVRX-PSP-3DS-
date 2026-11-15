
#include "psp_cpu.h"
#include <string.h>
#include <limits.h>

enum {
    CPU_EXCEPTION_NONE = 0,
    CPU_EXCEPTION_ALIGNMENT = 1,
    CPU_EXCEPTION_MEMORY = 2,
    CPU_EXCEPTION_SYSCALL = 3,
    CPU_EXCEPTION_BREAK = 4,
    CPU_EXCEPTION_OPCODE = 5,
    CPU_EXCEPTION_DELAY_SLOT = 6,
    CPU_EXCEPTION_OVERFLOW = 7
};

static uint32_t getReg(PSPCPU *cpu, unsigned index) {
    return index == 0 ? 0 : cpu->r[index & 31];
}

static void setReg(PSPCPU *cpu, unsigned index, uint32_t value) {
    if (index != 0)
        cpu->r[index & 31] = value;
}

static int cpuError(PSPCPU *cpu, int error) {
    cpu->exception = error;
    cpu->halted = true;
    cpu->branchPending = false;
    return -error;
}

static bool addOverflow(int32_t a, int32_t b, int32_t *result) {
    int64_t value = (int64_t)a + (int64_t)b;

    if (value > INT32_MAX || value < INT32_MIN)
        return true;

    *result = (int32_t)value;
    return false;
}

static bool subOverflow(int32_t a, int32_t b, int32_t *result) {
    int64_t value = (int64_t)a - (int64_t)b;

    if (value > INT32_MAX || value < INT32_MIN)
        return true;

    *result = (int32_t)value;
    return false;
}

void pspCpuReset(PSPCPU *cpu, uint32_t entryPoint) {
    if (!cpu)
        return;

    memset(cpu, 0, sizeof(*cpu));
    cpu->pc = entryPoint;
}

int pspCpuStep(PSPCPU *cpu, PSPMemory *memory) {
    if (!cpu || !memory || cpu->halted)
        return -1;

    if (cpu->pc & 3)
        return cpuError(cpu, CPU_EXCEPTION_ALIGNMENT);

    uint32_t instruction;

    if (!pspMemoryRead32(memory, cpu->pc, &instruction))
        return cpuError(cpu, CPU_EXCEPTION_MEMORY);

    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 31;
    uint32_t rt = (instruction >> 16) & 31;
    uint32_t rd = (instruction >> 11) & 31;
    uint32_t shamt = (instruction >> 6) & 31;
    uint32_t funct = instruction & 63;

    int32_t simm = (int16_t)(instruction & 0xFFFF);
    uint32_t uimm = instruction & 0xFFFF;

    uint32_t a = getReg(cpu, rs);
    uint32_t b = getReg(cpu, rt);

    bool wasDelaySlot = cpu->branchPending;
    uint32_t oldBranchTarget = cpu->branchTarget;

    cpu->branchPending = false;

    uint32_t nextPC = cpu->pc + 4;
    uint32_t address = a + (uint32_t)simm;

    switch (opcode) {

        case 0x00:
            switch (funct) {

                case 0x00:
                    setReg(cpu, rd, b << shamt);
                    break;

                case 0x02:
                    setReg(cpu, rd, b >> shamt);
                    break;

                case 0x03:
                    setReg(cpu, rd,
                        (uint32_t)((int32_t)b >> shamt));
                    break;

                case 0x04:
                    setReg(cpu, rd, b << (a & 31));
                    break;

                case 0x06:
                    setReg(cpu, rd, b >> (a & 31));
                    break;

                case 0x07:
                    setReg(cpu, rd,
                        (uint32_t)((int32_t)b >> (a & 31)));
                    break;

                case 0x08:
                    cpu->branchTarget = a;
                    cpu->branchPending = true;
                    break;

                case 0x09:
                    setReg(cpu, rd, cpu->pc + 8);
                    cpu->branchTarget = a;
                    cpu->branchPending = true;
                    break;

                case 0x0A:
                    if (b == 0)
                        setReg(cpu, rd, a);
                    break;

                case 0x0B:
                    if (b != 0)
                        setReg(cpu, rd, a);
                    break;

                case 0x0C:
                    cpu->halted = true;
                    cpu->exception = CPU_EXCEPTION_SYSCALL;
                    break;

                case 0x0D:
                    cpu->halted = true;
                    cpu->exception = CPU_EXCEPTION_BREAK;
                    break;

                case 0x10:
                    setReg(cpu, rd, cpu->hi);
                    break;

                case 0x11:
                    cpu->hi = a;
                    break;

                case 0x12:
                    setReg(cpu, rd, cpu->lo);
                    break;

                case 0x13:
                    cpu->lo = a;
                    break;

                case 0x18: {
                    int64_t result =
                        (int64_t)(int32_t)a * (int64_t)(int32_t)b;

                    cpu->lo = (uint32_t)result;
                    cpu->hi = (uint32_t)((uint64_t)result >> 32);
                    break;
                }

                case 0x19: {
                    uint64_t result = (uint64_t)a * b;

                    cpu->lo = (uint32_t)result;
                    cpu->hi = (uint32_t)(result >> 32);
                    break;
                }

                case 0x1A:
                    if (b != 0) {
                        if (a == 0x80000000 &&
                            b == 0xFFFFFFFF) {
                            cpu->lo = 0x80000000;
                            cpu->hi = 0;
                        } else {
                            cpu->lo =
                                (uint32_t)((int32_t)a / (int32_t)b);
                            cpu->hi =
                                (uint32_t)((int32_t)a % (int32_t)b);
                        }
                    }
                    break;

                case 0x1B:
                    if (b != 0) {
                        cpu->lo = a / b;
                        cpu->hi = a % b;
                    }
                    break;

                case 0x20: {
                    int32_t result;

                    if (addOverflow((int32_t)a, (int32_t)b, &result))
                        return cpuError(cpu, CPU_EXCEPTION_OVERFLOW);

                    setReg(cpu, rd, (uint32_t)result);
                    break;
                }

                case 0x21:
                    setReg(cpu, rd, a + b);
                    break;

                case 0x22: {
                    int32_t result;

                    if (subOverflow((int32_t)a, (int32_t)b, &result))
                        return cpuError(cpu, CPU_EXCEPTION_OVERFLOW);

                    setReg(cpu, rd, (uint32_t)result);
                    break;
                }

                case 0x23:
                    setReg(cpu, rd, a - b);
                    break;

                case 0x24:
                    setReg(cpu, rd, a & b);
                    break;

                case 0x25:
                    setReg(cpu, rd, a | b);
                    break;

                case 0x26:
                    setReg(cpu, rd, a ^ b);
                    break;

                case 0x27:
                    setReg(cpu, rd, ~(a | b));
                    break;

                case 0x2A:
                    setReg(cpu, rd, (int32_t)a < (int32_t)b);
                    break;

                case 0x2B:
                    setReg(cpu, rd, a < b);
                    break;

                default:
                    return cpuError(cpu, CPU_EXCEPTION_OPCODE);
            }
            break;

        case 0x01:
            if (rt == 0 || rt == 16) {
                if ((int32_t)a < 0) {
                    cpu->branchTarget =
                        nextPC + (uint32_t)(simm * 4);
                    cpu->branchPending = true;

                    if (rt == 16)
                        setReg(cpu, 31, cpu->pc + 8);
                }
            } else if (rt == 1 || rt == 17) {
                if ((int32_t)a >= 0) {
                    cpu->branchTarget =
                        nextPC + (uint32_t)(simm * 4);
                    cpu->branchPending = true;

                    if (rt == 17)
                        setReg(cpu, 31, cpu->pc + 8);
                }
            } else {
                return cpuError(cpu, CPU_EXCEPTION_OPCODE);
            }
            break;

        case 0x02:
        case 0x03:
            if (opcode == 0x03)
                setReg(cpu, 31, cpu->pc + 8);

            cpu->branchTarget =
                (nextPC & 0xF0000000) |
                ((instruction & 0x03FFFFFF) << 2);

            cpu->branchPending = true;
            break;

        case 0x04:
            if (a == b) {
                cpu->branchTarget =
                    nextPC + (uint32_t)(simm * 4);
                cpu->branchPending = true;
            }
            break;

        case 0x05:
            if (a != b) {
                cpu->branchTarget =
                    nextPC + (uint32_t)(simm * 4);
                cpu->branchPending = true;
            }
            break;

        case 0x06:
            if ((int32_t)a <= 0) {
                cpu->branchTarget =
                    nextPC + (uint32_t)(simm * 4);
                cpu->branchPending = true;
            }
            break;

        case 0x07:
            if ((int32_t)a > 0) {
                cpu->branchTarget =
                    nextPC + (uint32_t)(simm * 4);
                cpu->branchPending = true;
            }
            break;

        case 0x08: {
            int32_t result;

            if (addOverflow((int32_t)a, simm, &result))
                return cpuError(cpu, CPU_EXCEPTION_OVERFLOW);

            setReg(cpu, rt, (uint32_t)result);
            break;
        }

        case 0x09:
            setReg(cpu, rt, a + (uint32_t)simm);
            break;

        case 0x0A:
            setReg(cpu, rt, (int32_t)a < simm);
            break;

        case 0x0B:
            setReg(cpu, rt, a < (uint32_t)simm);
            break;

        case 0x0C:
            setReg(cpu, rt, a & uimm);
            break;

        case 0x0D:
            setReg(cpu, rt, a | uimm);
            break;

        case 0x0E:
            setReg(cpu, rt, a ^ uimm);
            break;

        case 0x0F:
            setReg(cpu, rt, uimm << 16);
            break;

        case 0x20: {
            uint8_t value;

            if (!pspMemoryRead8(memory, address, &value))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);

            setReg(cpu, rt, (uint32_t)(int32_t)(int8_t)value);
            break;
        }

        case 0x21: {
            uint16_t value;

            if ((address & 1) ||
                !pspMemoryRead16(memory, address, &value))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);

            setReg(cpu, rt, (uint32_t)(int32_t)(int16_t)value);
            break;
        }

        case 0x23: {
            uint32_t value;

            if ((address & 3) ||
                !pspMemoryRead32(memory, address, &value))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);

            setReg(cpu, rt, value);
            break;
        }

        case 0x24: {
            uint8_t value;

            if (!pspMemoryRead8(memory, address, &value))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);

            setReg(cpu, rt, value);
            break;
        }

        case 0x25: {
            uint16_t value;

            if ((address & 1) ||
                !pspMemoryRead16(memory, address, &value))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);

            setReg(cpu, rt, value);
            break;
        }

        case 0x28:
            if (!pspMemoryWrite8(memory, address, (uint8_t)b))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);
            break;

        case 0x29:
            if ((address & 1) ||
                !pspMemoryWrite16(memory, address, (uint16_t)b))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);
            break;

        case 0x2B:
            if ((address & 3) ||
                !pspMemoryWrite32(memory, address, b))
                return cpuError(cpu, CPU_EXCEPTION_MEMORY);
            break;

        default:
            return cpuError(cpu, CPU_EXCEPTION_OPCODE);
    }

    if (wasDelaySlot) {
        if (cpu->branchPending)
            return cpuError(cpu, CPU_EXCEPTION_DELAY_SLOT);

        nextPC = oldBranchTarget;
    }

    cpu->pc = nextPC;
    cpu->r[0] = 0;
    cpu->instructions++;

    return 0;
}

int pspCpuRun(
    PSPCPU *cpu,
    PSPMemory *memory,
    uint32_t maxInstructions
) {
    if (!cpu || !memory)
        return -1;

    for (uint32_t i = 0; i < maxInstructions; i++) {
        if (cpu->halted)
            return 0;

        int result = pspCpuStep(cpu, memory);

        if (result != 0)
            return result;
    }

    return 0;
}
