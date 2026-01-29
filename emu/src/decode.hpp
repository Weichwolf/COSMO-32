#pragma once
#include <cstdint>

namespace cosmo {

// RISC-V RV32IMAC Instruction Decoder
//
// Opcode types from opcode[6:0] field
// Reference: RISC-V Unprivileged ISA Specification
enum class OpType {
    LOAD      = 0b0000011,  // I-type: LB, LH, LW, LBU, LHU
    LOAD_FP   = 0b0000111,  // I-type: FLW, FLD (not implemented)
    MISC_MEM  = 0b0001111,  // I-type: FENCE, FENCE.I
    OP_IMM    = 0b0010011,  // I-type: ADDI, SLTI, ANDI, ORI, XORI, SLLI, SRLI, SRAI
    AUIPC     = 0b0010111,  // U-type: Add upper immediate to PC
    OP_IMM_32 = 0b0011011,  // RV64 only (not implemented)
    STORE     = 0b0100011,  // S-type: SB, SH, SW
    STORE_FP  = 0b0100111,  // S-type: FSW, FSD (not implemented)
    AMO       = 0b0101111,  // R-type: LR.W, SC.W, AMO* (RV32A extension)
    OP        = 0b0110011,  // R-type: ADD, SUB, AND, OR, XOR, SLT, SLL, SRL, SRA + RV32M
    LUI       = 0b0110111,  // U-type: Load upper immediate
    OP_32     = 0b0111011,  // RV64 only (not implemented)
    MADD      = 0b1000011,  // FP multiply-add (not implemented)
    MSUB      = 0b1000111,  // FP multiply-sub (not implemented)
    NMSUB     = 0b1001011,  // FP neg multiply-sub (not implemented)
    NMADD     = 0b1001111,  // FP neg multiply-add (not implemented)
    OP_FP     = 0b1010011,  // FP operations (not implemented)
    BRANCH    = 0b1100011,  // B-type: BEQ, BNE, BLT, BGE, BLTU, BGEU
    JALR      = 0b1100111,  // I-type: Jump and link register
    JAL       = 0b1101111,  // J-type: Jump and link
    SYSTEM    = 0b1110011,  // I-type: ECALL, EBREAK, CSR*, WFI, MRET
    INVALID   = 0xFF
};

// ============================================================================
// 32-bit instruction field extraction
// ============================================================================

// opcode[6:0] - determines instruction format and operation class
inline uint32_t opcode(uint32_t inst) { return inst & 0x7F; }

// rd[11:7] - destination register (R/I/U/J-type)
inline uint32_t rd(uint32_t inst)     { return (inst >> 7) & 0x1F; }

// funct3[14:12] - operation variant selector
inline uint32_t funct3(uint32_t inst) { return (inst >> 12) & 0x7; }

// rs1[19:15] - source register 1 (R/I/S/B-type)
inline uint32_t rs1(uint32_t inst)    { return (inst >> 15) & 0x1F; }

// rs2[24:20] - source register 2 (R/S/B-type)
inline uint32_t rs2(uint32_t inst)    { return (inst >> 20) & 0x1F; }

// funct7[31:25] - extended operation selector (R-type)
inline uint32_t funct7(uint32_t inst) { return (inst >> 25) & 0x7F; }

// funct5[31:27] - AMO operation selector
inline uint32_t funct5(uint32_t inst) { return (inst >> 27) & 0x1F; }

// ============================================================================
// Immediate extraction (sign-extended to 32 bits)
// ============================================================================

// I-type: inst[31:20] -> imm[11:0], sign-extended
// Used by: LOAD, OP_IMM, JALR, SYSTEM
inline int32_t imm_i(uint32_t inst) {
    return static_cast<int32_t>(inst) >> 20;
}

// S-type: inst[31:25|11:7] -> imm[11:5|4:0], sign-extended
// Used by: STORE
inline int32_t imm_s(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0xFE000000) >> 20) | ((inst >> 7) & 0x1F);
}

// B-type: inst[31|7|30:25|11:8] -> imm[12|11|10:5|4:1], sign-extended
// Used by: BRANCH (offset is in multiples of 2)
inline int32_t imm_b(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0x80000000) >> 19) |  // imm[12]
           ((inst & 0x80) << 4) |                              // imm[11]
           ((inst >> 20) & 0x7E0) |                            // imm[10:5]
           ((inst >> 7) & 0x1E);                               // imm[4:1]
}

// U-type: inst[31:12] -> imm[31:12], lower 12 bits zero
// Used by: LUI, AUIPC
inline int32_t imm_u(uint32_t inst) {
    return inst & 0xFFFFF000;
}

// J-type: inst[31|19:12|20|30:21] -> imm[20|10:1|11|19:12], sign-extended
// Used by: JAL (offset is in multiples of 2)
inline int32_t imm_j(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0x80000000) >> 11) |  // imm[20]
           (inst & 0xFF000) |                                  // imm[19:12]
           ((inst >> 9) & 0x800) |                             // imm[11]
           ((inst >> 20) & 0x7FE);                             // imm[10:1]
}

// CSR address from I-type immediate field
inline uint32_t csr_addr(uint32_t inst) { return (inst >> 20) & 0xFFF; }

// ============================================================================
// Compressed instruction handling (RV32C extension)
// ============================================================================

// Check if instruction is compressed (16-bit)
// Compressed instructions have bits [1:0] != 0b11
inline bool is_compressed(uint32_t inst) {
    return (inst & 0x3) != 0x3;
}

// Expand 16-bit compressed instruction to 32-bit equivalent
// Returns 0 on illegal/unimplemented instruction
//
// Compressed instruction format:
//   Quadrant 0 (op=00): C.ADDI4SPN, C.LW, C.SW
//   Quadrant 1 (op=01): C.NOP, C.ADDI, C.JAL, C.LI, C.ADDI16SP, C.LUI,
//                       C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR, C.AND,
//                       C.J, C.BEQZ, C.BNEZ
//   Quadrant 2 (op=10): C.SLLI, C.LWSP, C.JR, C.MV, C.EBREAK, C.JALR, C.ADD, C.SWSP
//
// Reference: RISC-V "C" Standard Extension for Compressed Instructions
inline uint32_t expand_compressed(uint16_t cinst) {
    uint32_t op = cinst & 0x3;           // Quadrant selector
    uint32_t funct3 = (cinst >> 13) & 0x7;

    // Compressed register encoding: 3-bit field maps to x8-x15
    auto creg = [](uint32_t r) { return r + 8; };

    switch (op) {
    // ========================================================================
    // Quadrant 0 (op = 00): Stack-relative loads/stores, ADDI4SPN
    // ========================================================================
    case 0b00: {
        switch (funct3) {
        case 0b000: { // C.ADDI4SPN: addi rd', x2, nzuimm
            // Expands to: addi rd', sp, imm (add scaled immediate to stack pointer)
            uint32_t rd_c = (cinst >> 2) & 0x7;
            uint32_t nzuimm = ((cinst >> 1) & 0x3C0) | ((cinst >> 7) & 0x30) |
                              ((cinst >> 2) & 0x8) | ((cinst >> 4) & 0x4);
            if (nzuimm == 0) return 0; // Reserved encoding
            return 0x13 | (creg(rd_c) << 7) | (2 << 15) | (nzuimm << 20);
        }
        case 0b010: { // C.LW: lw rd', offset(rs1')
            // Load word from memory, base+offset addressing
            uint32_t rd_c = (cinst >> 2) & 0x7;
            uint32_t rs1_c = (cinst >> 7) & 0x7;
            uint32_t uimm = ((cinst >> 7) & 0x38) | ((cinst >> 4) & 0x4) | ((cinst << 1) & 0x40);
            return 0x03 | (creg(rd_c) << 7) | (0b010 << 12) | (creg(rs1_c) << 15) | (uimm << 20);
        }
        case 0b110: { // C.SW: sw rs2', offset(rs1')
            // Store word to memory, base+offset addressing
            uint32_t rs2_c = (cinst >> 2) & 0x7;
            uint32_t rs1_c = (cinst >> 7) & 0x7;
            uint32_t uimm = ((cinst >> 7) & 0x38) | ((cinst >> 4) & 0x4) | ((cinst << 1) & 0x40);
            uint32_t imm_lo = uimm & 0x1F;
            uint32_t imm_hi = (uimm >> 5) & 0x7F;
            return 0x23 | (imm_lo << 7) | (0b010 << 12) | (creg(rs1_c) << 15) | (creg(rs2_c) << 20) | (imm_hi << 25);
        }
        default: return 0;
        }
    }

    // ========================================================================
    // Quadrant 1 (op = 01): Control flow, immediate operations
    // ========================================================================
    case 0b01: {
        switch (funct3) {
        case 0b000: { // C.NOP (rd=0) / C.ADDI (rd!=0): addi rd, rd, imm
            // Add 6-bit signed immediate to register
            uint32_t rd_rs1 = (cinst >> 7) & 0x1F;
            int32_t imm = ((cinst >> 7) & 0x20) | ((cinst >> 2) & 0x1F);
            if (imm & 0x20) imm |= 0xFFFFFFC0; // Sign extend from bit 5
            return 0x13 | (rd_rs1 << 7) | (rd_rs1 << 15) | ((imm & 0xFFF) << 20);
        }
        case 0b001: { // C.JAL (RV32 only): jal x1, offset
            // Jump and link with 12-bit signed offset (saves return address to ra)
            // imm[11|4|9:8|10|6|7|3:1|5] encoded in inst[12:2]
            int32_t offset = ((cinst >> 1) & 0x800) |   // imm[11]
                             ((cinst >> 7) & 0x10) |    // imm[4]
                             ((cinst >> 1) & 0x300) |   // imm[9:8]
                             ((cinst << 2) & 0x400) |   // imm[10]
                             ((cinst >> 1) & 0x40) |    // imm[6]
                             ((cinst << 1) & 0x80) |    // imm[7]
                             ((cinst >> 2) & 0xE) |     // imm[3:1]
                             ((cinst << 3) & 0x20);     // imm[5]
            offset = ((cinst >> 12) & 1) ? (offset | 0xFFFFF000) : offset; // Sign extend
            // Convert to J-type immediate encoding
            uint32_t imm20 = ((offset >> 20) & 1) << 31;
            uint32_t imm10_1 = ((offset >> 1) & 0x3FF) << 21;
            uint32_t imm11 = ((offset >> 11) & 1) << 20;
            uint32_t imm19_12 = ((offset >> 12) & 0xFF) << 12;
            return 0x6F | (1 << 7) | imm19_12 | imm11 | imm10_1 | imm20;
        }
        case 0b010: { // C.LI: addi rd, x0, imm
            // Load 6-bit signed immediate into register
            uint32_t rd = (cinst >> 7) & 0x1F;
            int32_t imm = ((cinst >> 7) & 0x20) | ((cinst >> 2) & 0x1F);
            if (imm & 0x20) imm |= 0xFFFFFFC0;
            return 0x13 | (rd << 7) | ((imm & 0xFFF) << 20);
        }
        case 0b011: { // C.ADDI16SP (rd=2) / C.LUI (rd!=0,2)
            uint32_t rd = (cinst >> 7) & 0x1F;
            if (rd == 2) { // C.ADDI16SP: addi sp, sp, imm*16
                // Add 10-bit signed immediate (scaled by 16) to stack pointer
                int32_t imm = ((cinst >> 3) & 0x200) | ((cinst >> 2) & 0x10) |
                              ((cinst << 1) & 0x40) | ((cinst << 4) & 0x180) |
                              ((cinst << 3) & 0x20);
                if (imm & 0x200) imm |= 0xFFFFFC00;
                if (imm == 0) return 0; // Reserved
                return 0x13 | (2 << 7) | (2 << 15) | ((imm & 0xFFF) << 20);
            } else { // C.LUI: lui rd, imm
                // Load 18-bit upper immediate (bits [17:12] from instruction)
                int32_t imm = ((cinst << 5) & 0x20000) | ((cinst << 10) & 0x1F000);
                if (cinst & 0x1000) imm |= 0xFFFC0000;
                if (imm == 0) return 0; // Reserved
                return 0x37 | (rd << 7) | (imm & 0xFFFFF000);
            }
        }
        case 0b100: { // C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR, C.AND
            uint32_t funct2 = (cinst >> 10) & 0x3;
            uint32_t rd_rs1_c = (cinst >> 7) & 0x7;
            uint32_t rd_rs1 = creg(rd_rs1_c);
            uint32_t shamt = ((cinst >> 7) & 0x20) | ((cinst >> 2) & 0x1F);

            switch (funct2) {
            case 0b00: // C.SRLI: srli rd', rd', shamt
                return 0x13 | (rd_rs1 << 7) | (0b101 << 12) | (rd_rs1 << 15) | (shamt << 20);
            case 0b01: // C.SRAI: srai rd', rd', shamt (arithmetic right shift)
                return 0x13 | (rd_rs1 << 7) | (0b101 << 12) | (rd_rs1 << 15) | (shamt << 20) | (0x20 << 25);
            case 0b10: { // C.ANDI: andi rd', rd', imm
                int32_t imm = shamt;
                if (imm & 0x20) imm |= 0xFFFFFFC0;
                return 0x13 | (rd_rs1 << 7) | (0b111 << 12) | (rd_rs1 << 15) | ((imm & 0xFFF) << 20);
            }
            case 0b11: { // C.SUB, C.XOR, C.OR, C.AND (register-register ops)
                uint32_t rs2_c = (cinst >> 2) & 0x7;
                uint32_t rs2 = creg(rs2_c);
                uint32_t funct1 = (cinst >> 12) & 0x1;
                uint32_t funct2b = (cinst >> 5) & 0x3;
                if (funct1 == 0) {
                    switch (funct2b) {
                    case 0b00: // C.SUB: sub rd', rd', rs2'
                        return 0x33 | (rd_rs1 << 7) | (rd_rs1 << 15) | (rs2 << 20) | (0x20 << 25);
                    case 0b01: // C.XOR: xor rd', rd', rs2'
                        return 0x33 | (rd_rs1 << 7) | (0b100 << 12) | (rd_rs1 << 15) | (rs2 << 20);
                    case 0b10: // C.OR: or rd', rd', rs2'
                        return 0x33 | (rd_rs1 << 7) | (0b110 << 12) | (rd_rs1 << 15) | (rs2 << 20);
                    case 0b11: // C.AND: and rd', rd', rs2'
                        return 0x33 | (rd_rs1 << 7) | (0b111 << 12) | (rd_rs1 << 15) | (rs2 << 20);
                    }
                }
                return 0;
            }
            }
            return 0;
        }
        case 0b101: { // C.J: jal x0, offset (unconditional jump, no link)
            // Same offset encoding as C.JAL but links to x0 (discards return)
            int32_t offset = ((cinst >> 1) & 0x800) |
                             ((cinst >> 7) & 0x10) |
                             ((cinst >> 1) & 0x300) |
                             ((cinst << 2) & 0x400) |
                             ((cinst >> 1) & 0x40) |
                             ((cinst << 1) & 0x80) |
                             ((cinst >> 2) & 0xE) |
                             ((cinst << 3) & 0x20);
            if (cinst & 0x1000) offset |= 0xFFFFF000;
            uint32_t imm20 = ((offset >> 20) & 1) << 31;
            uint32_t imm10_1 = ((offset >> 1) & 0x3FF) << 21;
            uint32_t imm11 = ((offset >> 11) & 1) << 20;
            uint32_t imm19_12 = ((offset >> 12) & 0xFF) << 12;
            return 0x6F | imm19_12 | imm11 | imm10_1 | imm20;
        }
        case 0b110: { // C.BEQZ: beq rs1', x0, offset
            // Branch if register equals zero
            uint32_t rs1_c = (cinst >> 7) & 0x7;
            int32_t offset = ((cinst >> 4) & 0x100) | ((cinst >> 7) & 0x18) |
                             ((cinst << 1) & 0xC0) | ((cinst >> 2) & 0x6) |
                             ((cinst << 3) & 0x20);
            if (cinst & 0x1000) offset |= 0xFFFFFE00;
            uint32_t imm12 = ((offset >> 12) & 1) << 31;
            uint32_t imm10_5 = ((offset >> 5) & 0x3F) << 25;
            uint32_t imm4_1 = ((offset >> 1) & 0xF) << 8;
            uint32_t imm11 = ((offset >> 11) & 1) << 7;
            return 0x63 | imm11 | imm4_1 | (creg(rs1_c) << 15) | imm10_5 | imm12;
        }
        case 0b111: { // C.BNEZ: bne rs1', x0, offset
            // Branch if register not equal to zero
            uint32_t rs1_c = (cinst >> 7) & 0x7;
            int32_t offset = ((cinst >> 4) & 0x100) | ((cinst >> 7) & 0x18) |
                             ((cinst << 1) & 0xC0) | ((cinst >> 2) & 0x6) |
                             ((cinst << 3) & 0x20);
            if (cinst & 0x1000) offset |= 0xFFFFFE00;
            uint32_t imm12 = ((offset >> 12) & 1) << 31;
            uint32_t imm10_5 = ((offset >> 5) & 0x3F) << 25;
            uint32_t imm4_1 = ((offset >> 1) & 0xF) << 8;
            uint32_t imm11 = ((offset >> 11) & 1) << 7;
            return 0x63 | imm11 | imm4_1 | (0b001 << 12) | (creg(rs1_c) << 15) | imm10_5 | imm12;
        }
        }
        return 0;
    }

    // ========================================================================
    // Quadrant 2 (op = 10): Stack-pointer relative operations, misc
    // ========================================================================
    case 0b10: {
        switch (funct3) {
        case 0b000: { // C.SLLI: slli rd, rd, shamt
            // Shift left logical immediate
            uint32_t rd_rs1 = (cinst >> 7) & 0x1F;
            uint32_t shamt = ((cinst >> 7) & 0x20) | ((cinst >> 2) & 0x1F);
            return 0x13 | (rd_rs1 << 7) | (0b001 << 12) | (rd_rs1 << 15) | (shamt << 20);
        }
        case 0b010: { // C.LWSP: lw rd, offset(sp)
            // Load word from stack (sp-relative)
            uint32_t rd = (cinst >> 7) & 0x1F;
            if (rd == 0) return 0; // Reserved
            uint32_t uimm = ((cinst >> 2) & 0x1C) | ((cinst >> 7) & 0x20) | ((cinst << 4) & 0xC0);
            return 0x03 | (rd << 7) | (0b010 << 12) | (2 << 15) | (uimm << 20);
        }
        case 0b100: { // C.JR, C.MV, C.EBREAK, C.JALR, C.ADD
            uint32_t rs1 = (cinst >> 7) & 0x1F;
            uint32_t rs2 = (cinst >> 2) & 0x1F;
            if ((cinst >> 12) & 1) { // bit 12 set
                if (rs2 == 0) {
                    if (rs1 == 0) { // C.EBREAK: ebreak
                        return 0x73 | (1 << 20);
                    } else { // C.JALR: jalr x1, rs1, 0 (call subroutine)
                        return 0x67 | (1 << 7) | (rs1 << 15);
                    }
                } else { // C.ADD: add rd, rd, rs2
                    return 0x33 | (rs1 << 7) | (rs1 << 15) | (rs2 << 20);
                }
            } else { // bit 12 clear
                if (rs2 == 0) { // C.JR: jalr x0, rs1, 0 (return/jump)
                    if (rs1 == 0) return 0; // Reserved
                    return 0x67 | (rs1 << 15);
                } else { // C.MV: add rd, x0, rs2 (move register)
                    return 0x33 | (rs1 << 7) | (rs2 << 20);
                }
            }
        }
        case 0b110: { // C.SWSP: sw rs2, offset(sp)
            // Store word to stack (sp-relative)
            uint32_t rs2 = (cinst >> 2) & 0x1F;
            uint32_t uimm = ((cinst >> 7) & 0x3C) | ((cinst >> 1) & 0xC0);
            uint32_t imm_lo = uimm & 0x1F;
            uint32_t imm_hi = (uimm >> 5) & 0x7F;
            return 0x23 | (imm_lo << 7) | (0b010 << 12) | (2 << 15) | (rs2 << 20) | (imm_hi << 25);
        }
        default: return 0;
        }
    }

    default: return 0;
    }
}

} // namespace cosmo
