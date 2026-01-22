#include "cpu.hpp"
#include "decode.hpp"
#include "device/pfic.hpp"
#include <cstdio>

namespace cosmo {

void CPU::reset(uint32_t start_pc) {
    x.fill(0);
    pc = start_pc;
    cycles = 0;
    mstatus = 0;
    mie = 0;
    mtvec = 0;
    mepc = 0;
    mcause = 0;
    mtval = 0;
    mip = 0;
    reservation_valid = false;
    halted = false;
    wfi = false;
}

uint32_t CPU::csr_read(uint32_t addr) {
    switch (static_cast<CSR>(addr)) {
        case CSR::mstatus: return mstatus;
        case CSR::mie:     return mie;
        case CSR::mtvec:   return mtvec;
        case CSR::mepc:    return mepc;
        case CSR::mcause:  return mcause;
        case CSR::mtval:   return mtval;
        case CSR::mip:     return mip;
        default:
            std::fprintf(stderr, "[CPU] Unknown CSR read: 0x%03X at PC=0x%08X\n", addr, pc);
            return 0;
    }
}

void CPU::csr_write(uint32_t addr, uint32_t val) {
    switch (static_cast<CSR>(addr)) {
        case CSR::mstatus: mstatus = val & 0x88; break; // MIE(3), MPIE(7)
        case CSR::mie:     mie = val; break;
        case CSR::mtvec:   mtvec = val & ~0x2; break; // Mode 0 or 1 only
        case CSR::mepc:    mepc = val & ~0x1; break;  // Aligned
        case CSR::mcause:  mcause = val; break;
        case CSR::mtval:   mtval = val; break;
        case CSR::mip:     mip = val; break;
        default:
            std::fprintf(stderr, "[CPU] Unknown CSR write: 0x%03X = 0x%08X at PC=0x%08X\n", addr, val, pc);
            break;
    }
}

void CPU::take_trap(TrapCause cause, uint32_t tval) {
    mepc = pc;
    mcause = static_cast<uint32_t>(cause);
    mtval = tval;

    // MPIE = MIE, MIE = 0
    uint32_t mie_bit = (mstatus >> 3) & 1;
    mstatus = (mstatus & ~0x88) | (mie_bit << 7);

    // Jump to trap vector
    if ((mtvec & 0x1) == 0) {
        // Direct mode
        pc = mtvec & ~0x3;
    } else {
        // Vectored mode
        pc = (mtvec & ~0x3) + 4 * static_cast<uint32_t>(cause);
    }
}

void CPU::take_interrupt(InterruptCause cause) {
    mepc = pc;
    // Interrupt bit (bit 31) + cause
    mcause = 0x80000000 | static_cast<uint32_t>(cause);
    mtval = 0;

    // MPIE = MIE, MIE = 0
    uint32_t mie_bit = (mstatus >> 3) & 1;
    mstatus = (mstatus & ~0x88) | (mie_bit << 7);

    // Jump to trap vector
    if ((mtvec & 0x1) == 0) {
        // Direct mode - all interrupts go to same address
        pc = mtvec & ~0x3;
    } else {
        // Vectored mode - different address per interrupt
        pc = (mtvec & ~0x3) + 4 * static_cast<uint32_t>(cause);
    }
}

void CPU::mret() {
    // MIE = MPIE, MPIE = 1
    uint32_t mpie = (mstatus >> 7) & 1;
    mstatus = (mstatus & ~0x88) | (mpie << 3) | 0x80;
    pc = mepc;
}

bool CPU::check_interrupts() {
    // Check if interrupts are globally enabled
    if (!interrupts_enabled()) {
        // But still wake from WFI if any interrupt pending
        if (wfi && (mip & mie)) {
            wfi = false;
        }
        return false;
    }

    // Check for pending and enabled interrupts (priority: external > timer > software)
    // Machine external interrupt
    if ((mip & MIE_MEIE) && (mie & MIE_MEIE)) {
        // Check PFIC for actual pending interrupt
        if (pfic) {
            int irq = pfic->get_pending_irq();
            if (irq >= 0) {
                pfic->set_active(irq);
                take_interrupt(InterruptCause::MExternal);
                wfi = false;
                return true;
            }
        } else {
            take_interrupt(InterruptCause::MExternal);
            mip &= ~MIE_MEIE;  // Clear pending
            wfi = false;
            return true;
        }
    }

    // Machine timer interrupt
    if ((mip & MIE_MTIE) && (mie & MIE_MTIE)) {
        take_interrupt(InterruptCause::MTimer);
        // Timer pending is typically cleared by writing to mtimecmp
        wfi = false;
        return true;
    }

    // Machine software interrupt
    if ((mip & MIE_MSIE) && (mie & MIE_MSIE)) {
        take_interrupt(InterruptCause::MSoftware);
        mip &= ~MIE_MSIE;  // Clear pending
        wfi = false;
        return true;
    }

    return false;
}

void CPU::step() {
    if (halted) return;

    // Check for pending interrupts
    if (check_interrupts()) {
        cycles++;
        return;  // Interrupt taken, will execute handler next step
    }

    if (wfi) return;  // Still waiting for interrupt

    // Fetch instruction
    uint32_t inst = bus->read32(pc);
    inst_len_ = 4;

    // Check for compressed instruction
    if (is_compressed(inst)) {
        uint16_t cinst = inst & 0xFFFF;
        inst = expand_compressed(cinst);
        inst_len_ = 2;
        if (inst == 0) {
            illegal_instruction(cinst);
            return;
        }
    }

    // Decode opcode
    uint32_t op = opcode(inst);

    // Execute based on opcode
    switch (static_cast<OpType>(op)) {
        case OpType::OP:       exec_op(inst); break;
        case OpType::OP_IMM:   exec_op_imm(inst); break;
        case OpType::LOAD:     exec_load(inst); break;
        case OpType::STORE:    exec_store(inst); break;
        case OpType::BRANCH:   exec_branch(inst); return; // PC set by branch
        case OpType::JAL:      exec_jal(inst); return;
        case OpType::JALR:     exec_jalr(inst); return;
        case OpType::LUI:      exec_lui(inst); break;
        case OpType::AUIPC:    exec_auipc(inst); break;
        case OpType::SYSTEM:   exec_system(inst); if (op == 0x73 && funct3(inst) == 0) return; break;
        case OpType::AMO:      exec_amo(inst); break;
        case OpType::MISC_MEM: exec_misc_mem(inst); break;
        default:
            illegal_instruction(inst);
            return;
    }

    pc += inst_len_;
    cycles++;
}

void CPU::exec_op(uint32_t inst) {
    uint32_t d = rd(inst);
    uint32_t s1 = reg(rs1(inst));
    uint32_t s2 = reg(rs2(inst));
    uint32_t f3 = funct3(inst);
    uint32_t f7 = funct7(inst);


    uint32_t result = 0;

    if (f7 == 0x01) {
        // RV32M Extension
        int32_t ss1 = static_cast<int32_t>(s1);
        int32_t ss2 = static_cast<int32_t>(s2);
        switch (f3) {
            case 0b000: // MUL
                result = static_cast<uint32_t>(ss1 * ss2);
                break;
            case 0b001: // MULH
                result = static_cast<uint32_t>((static_cast<int64_t>(ss1) * static_cast<int64_t>(ss2)) >> 32);
                break;
            case 0b010: // MULHSU
                result = static_cast<uint32_t>((static_cast<int64_t>(ss1) * static_cast<uint64_t>(s2)) >> 32);
                break;
            case 0b011: // MULHU
                result = static_cast<uint32_t>((static_cast<uint64_t>(s1) * static_cast<uint64_t>(s2)) >> 32);
                break;
            case 0b100: // DIV
                if (s2 == 0) result = 0xFFFFFFFF;
                else if (s1 == 0x80000000 && s2 == 0xFFFFFFFF) result = 0x80000000;
                else result = static_cast<uint32_t>(ss1 / ss2);
                break;
            case 0b101: // DIVU
                result = s2 ? s1 / s2 : 0xFFFFFFFF;
                break;
            case 0b110: // REM
                if (s2 == 0) result = s1;
                else if (s1 == 0x80000000 && s2 == 0xFFFFFFFF) result = 0;
                else result = static_cast<uint32_t>(ss1 % ss2);
                break;
            case 0b111: // REMU
                result = s2 ? s1 % s2 : s1;
                break;
        }
    } else {
        // RV32I base
        switch (f3) {
            case 0b000: // ADD/SUB
                result = (f7 == 0x20) ? s1 - s2 : s1 + s2;
                break;
            case 0b001: // SLL
                result = s1 << (s2 & 0x1F);
                break;
            case 0b010: // SLT
                result = (static_cast<int32_t>(s1) < static_cast<int32_t>(s2)) ? 1 : 0;
                break;
            case 0b011: // SLTU
                result = (s1 < s2) ? 1 : 0;
                break;
            case 0b100: // XOR
                result = s1 ^ s2;
                break;
            case 0b101: // SRL/SRA
                if (f7 == 0x20)
                    result = static_cast<uint32_t>(static_cast<int32_t>(s1) >> (s2 & 0x1F));
                else
                    result = s1 >> (s2 & 0x1F);
                break;
            case 0b110: // OR
                result = s1 | s2;
                break;
            case 0b111: // AND
                result = s1 & s2;
                break;
        }
    }

    set_reg(d, result);
}

void CPU::exec_op_imm(uint32_t inst) {
    uint32_t d = rd(inst);
    uint32_t s1 = reg(rs1(inst));
    int32_t imm = imm_i(inst);
    uint32_t f3 = funct3(inst);
    uint32_t shamt = imm & 0x1F;


    uint32_t result = 0;

    switch (f3) {
        case 0b000: // ADDI
            result = s1 + imm;
            break;
        case 0b001: // SLLI
            result = s1 << shamt;
            break;
        case 0b010: // SLTI
            result = (static_cast<int32_t>(s1) < imm) ? 1 : 0;
            break;
        case 0b011: // SLTIU
            result = (s1 < static_cast<uint32_t>(imm)) ? 1 : 0;
            break;
        case 0b100: // XORI
            result = s1 ^ static_cast<uint32_t>(imm);
            break;
        case 0b101: // SRLI/SRAI
            if (inst & (1 << 30))
                result = static_cast<uint32_t>(static_cast<int32_t>(s1) >> shamt);
            else
                result = s1 >> shamt;
            break;
        case 0b110: // ORI
            result = s1 | static_cast<uint32_t>(imm);
            break;
        case 0b111: // ANDI
            result = s1 & static_cast<uint32_t>(imm);
            break;
    }

    set_reg(d, result);
}

void CPU::exec_load(uint32_t inst) {
    uint32_t d = rd(inst);
    uint32_t base = reg(rs1(inst));
    int32_t offset = imm_i(inst);
    uint32_t addr = base + offset;
    uint32_t f3 = funct3(inst);

    uint32_t result = 0;

    switch (f3) {
        case 0b000: // LB
            result = static_cast<uint32_t>(static_cast<int8_t>(bus->read8(addr)));
            break;
        case 0b001: // LH
            if (addr & 1) {
                // Misaligned halfword load - assemble from bytes
                uint32_t lo = bus->read8(addr);
                uint32_t hi = bus->read8(addr + 1);
                result = static_cast<uint32_t>(static_cast<int16_t>(lo | (hi << 8)));
            } else {
                result = static_cast<uint32_t>(static_cast<int16_t>(bus->read16(addr)));
            }
            break;
        case 0b010: // LW
            if (addr & 3) {
                // Misaligned word load - assemble from bytes
                result = bus->read8(addr) |
                         (bus->read8(addr + 1) << 8) |
                         (bus->read8(addr + 2) << 16) |
                         (bus->read8(addr + 3) << 24);
            } else {
                result = bus->read32(addr);
            }
            break;
        case 0b100: // LBU
            result = bus->read8(addr);
            break;
        case 0b101: // LHU
            if (addr & 1) {
                // Misaligned unsigned halfword load - assemble from bytes
                uint32_t lo = bus->read8(addr);
                uint32_t hi = bus->read8(addr + 1);
                result = lo | (hi << 8);
            } else {
                result = bus->read16(addr);
            }
            break;
        default:
            illegal_instruction(inst);
            return;
    }

    // LR.W invalidates on load to different address
    if (reservation_valid && reservation_addr != addr) {
        reservation_valid = false;
    }

    set_reg(d, result);
}

void CPU::exec_store(uint32_t inst) {
    uint32_t base = reg(rs1(inst));
    uint32_t src = reg(rs2(inst));
    int32_t offset = imm_s(inst);
    uint32_t addr = base + offset;
    uint32_t f3 = funct3(inst);

    switch (f3) {
        case 0b000: // SB
            bus->write8(addr, src);
            break;
        case 0b001: // SH
            if (addr & 1) {
                // Misaligned halfword store - split into bytes
                bus->write8(addr, src & 0xFF);
                bus->write8(addr + 1, (src >> 8) & 0xFF);
            } else {
                bus->write16(addr, src);
            }
            break;
        case 0b010: // SW
            if (addr & 3) {
                // Misaligned word store - split into bytes
                bus->write8(addr, src & 0xFF);
                bus->write8(addr + 1, (src >> 8) & 0xFF);
                bus->write8(addr + 2, (src >> 16) & 0xFF);
                bus->write8(addr + 3, (src >> 24) & 0xFF);
            } else {
                bus->write32(addr, src);
            }
            break;
        default:
            illegal_instruction(inst);
            return;
    }

    // Store invalidates reservation
    if (reservation_valid && reservation_addr == addr) {
        reservation_valid = false;
    }
}

void CPU::exec_branch(uint32_t inst) {
    uint32_t s1 = reg(rs1(inst));
    uint32_t s2 = reg(rs2(inst));
    int32_t offset = imm_b(inst);
    uint32_t f3 = funct3(inst);

    bool taken = false;
    int32_t ss1 = static_cast<int32_t>(s1);
    int32_t ss2 = static_cast<int32_t>(s2);

    switch (f3) {
        case 0b000: taken = (s1 == s2); break;  // BEQ
        case 0b001: taken = (s1 != s2); break;  // BNE
        case 0b100: taken = (ss1 < ss2); break; // BLT
        case 0b101: taken = (ss1 >= ss2); break;// BGE
        case 0b110: taken = (s1 < s2); break;   // BLTU
        case 0b111: taken = (s1 >= s2); break;  // BGEU
        default:
            illegal_instruction(inst);
            return;
    }

    if (taken) {
        uint32_t target = pc + offset;
        if (target & 0x1) {
            take_trap(TrapCause::InstructionAddressMisaligned);
            return;
        }
        pc = target;
    } else {
        pc += inst_len_;
    }
    cycles++;
}

void CPU::exec_jal(uint32_t inst) {
    uint32_t d = rd(inst);
    int32_t offset = imm_j(inst);
    uint32_t target = pc + offset;

    if (target & 0x1) {
        take_trap(TrapCause::InstructionAddressMisaligned);
        return;
    }

    set_reg(d, pc + inst_len_);
    pc = target;
    cycles++;
}

void CPU::exec_jalr(uint32_t inst) {
    uint32_t d = rd(inst);
    uint32_t base = reg(rs1(inst));
    int32_t offset = imm_i(inst);
    uint32_t target = (base + offset) & ~1;

    set_reg(d, pc + inst_len_);
    pc = target;
    cycles++;
}

void CPU::exec_lui(uint32_t inst) {
    uint32_t d = rd(inst);
    int32_t imm = imm_u(inst);
    set_reg(d, imm);
}

void CPU::exec_auipc(uint32_t inst) {
    uint32_t d = rd(inst);
    int32_t imm = imm_u(inst);
    set_reg(d, pc + imm);
}

void CPU::exec_system(uint32_t inst) {
    uint32_t f3 = funct3(inst);
    uint32_t d = rd(inst);
    uint32_t csr = csr_addr(inst);

    if (f3 == 0) {
        // ECALL, EBREAK, MRET, WFI
        uint32_t funct12 = (inst >> 20) & 0xFFF;
        switch (funct12) {
            case 0x000: // ECALL
                take_trap(TrapCause::ECallFromMMode);
                return;
            case 0x001: // EBREAK
                take_trap(TrapCause::Breakpoint);
                return;
            case 0x302: // MRET
                mret();
                return;
            case 0x105: // WFI
                wfi = true;
                return;
            default:
                illegal_instruction(inst);
                return;
        }
    }

    // CSR instructions
    uint32_t src = (f3 & 0x4) ? rs1(inst) : reg(rs1(inst)); // Immediate vs register
    uint32_t old_val = csr_read(csr);
    uint32_t new_val = old_val;

    switch (f3 & 0x3) {
        case 0b01: // CSRRW / CSRRWI
            new_val = src;
            break;
        case 0b10: // CSRRS / CSRRSI
            new_val = old_val | src;
            break;
        case 0b11: // CSRRC / CSRRCI
            new_val = old_val & ~src;
            break;
    }

    // Write only if rs1 != 0 for set/clear, always for swap
    if ((f3 & 0x3) == 0b01 || rs1(inst) != 0) {
        csr_write(csr, new_val);
    }

    set_reg(d, old_val);
}

void CPU::exec_amo(uint32_t inst) {
    uint32_t d = rd(inst);
    uint32_t addr = reg(rs1(inst));
    uint32_t src = reg(rs2(inst));
    uint32_t f5 = funct5(inst);

    if (addr & 3) {
        take_trap(TrapCause::StoreAddressMisaligned);
        return;
    }

    uint32_t loaded = bus->read32(addr);
    uint32_t result = loaded;

    switch (f5) {
        case 0b00010: // LR.W
            reservation_addr = addr;
            reservation_valid = true;
            set_reg(d, loaded);
            return;

        case 0b00011: // SC.W
            if (reservation_valid && reservation_addr == addr) {
                bus->write32(addr, src);
                set_reg(d, 0); // Success
            } else {
                set_reg(d, 1); // Failure
            }
            reservation_valid = false;
            return;

        case 0b00001: result = src; break;                                      // AMOSWAP
        case 0b00000: result = loaded + src; break;                             // AMOADD
        case 0b00100: result = loaded ^ src; break;                             // AMOXOR
        case 0b01100: result = loaded & src; break;                             // AMOAND
        case 0b01000: result = loaded | src; break;                             // AMOOR
        case 0b10000: result = (static_cast<int32_t>(loaded) < static_cast<int32_t>(src)) ? loaded : src; break; // AMOMIN
        case 0b10100: result = (static_cast<int32_t>(loaded) > static_cast<int32_t>(src)) ? loaded : src; break; // AMOMAX
        case 0b11000: result = (loaded < src) ? loaded : src; break;            // AMOMINU
        case 0b11100: result = (loaded > src) ? loaded : src; break;            // AMOMAXU
        default:
            illegal_instruction(inst);
            return;
    }

    bus->write32(addr, result);
    set_reg(d, loaded);
}

void CPU::exec_misc_mem(uint32_t) {
    // FENCE - NOP for single-hart
}

void CPU::illegal_instruction(uint32_t inst) {
    std::fprintf(stderr, "[CPU] Illegal instruction at PC=0x%08X: 0x%08X\n", pc, inst);
    take_trap(TrapCause::IllegalInstruction, inst);
}

} // namespace cosmo
