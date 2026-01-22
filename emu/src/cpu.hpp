#pragma once
#include "bus.hpp"
#include <cstdint>
#include <array>

namespace cosmo {

// Forward declaration
class PFIC;

// Trap causes (exceptions)
enum class TrapCause : uint32_t {
    InstructionAddressMisaligned = 0,
    InstructionAccessFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAddressMisaligned = 6,
    StoreAccessFault = 7,
    ECallFromUMode = 8,
    ECallFromSMode = 9,
    ECallFromMMode = 11,
};

// Interrupt causes (bit 31 set in mcause)
enum class InterruptCause : uint32_t {
    MSoftware = 3,   // Machine software interrupt
    MTimer = 7,      // Machine timer interrupt
    MExternal = 11,  // Machine external interrupt
};

// CSR addresses
enum class CSR : uint32_t {
    mstatus = 0x300,
    mie     = 0x304,
    mtvec   = 0x305,
    mepc    = 0x341,
    mcause  = 0x342,
    mtval   = 0x343,
    mip     = 0x344,
};

// MIE/MIP bit positions
constexpr uint32_t MIE_MSIE = 1 << 3;   // Machine software interrupt enable
constexpr uint32_t MIE_MTIE = 1 << 7;   // Machine timer interrupt enable
constexpr uint32_t MIE_MEIE = 1 << 11;  // Machine external interrupt enable

class CPU {
public:
    // State
    std::array<uint32_t, 32> x{};  // General purpose registers
    uint32_t pc = 0;
    uint64_t cycles = 0;

    // CSRs
    uint32_t mstatus = 0;
    uint32_t mie = 0;
    uint32_t mtvec = 0;
    uint32_t mepc = 0;
    uint32_t mcause = 0;
    uint32_t mtval = 0;
    uint32_t mip = 0;

    // Atomics reservation
    uint32_t reservation_addr = 0xFFFFFFFF;
    bool reservation_valid = false;

    // Bus reference
    Bus* bus = nullptr;

    // PFIC reference (optional, for external interrupts)
    PFIC* pfic = nullptr;

    // Halted state
    bool halted = false;
    bool wfi = false;  // Wait for interrupt

    // Current instruction length (set by step(), used by branch/jal/jalr)
    uint32_t inst_len_ = 4;

    explicit CPU(Bus* b) : bus(b) {}

    void set_pfic(PFIC* p) { pfic = p; }

    // Register access (x0 always 0)
    uint32_t reg(uint32_t r) const { return r ? x[r] : 0; }
    void set_reg(uint32_t r, uint32_t v) { if (r) x[r] = v; }

    // CSR access
    uint32_t csr_read(uint32_t addr);
    void csr_write(uint32_t addr, uint32_t val);

    // Trap handling
    void take_trap(TrapCause cause, uint32_t tval = 0);
    void take_interrupt(InterruptCause cause);
    void mret();

    // Execution
    void step();
    void step_fast();  // No interrupt check
    void run(uint64_t cycles);  // Batch execution
    void reset(uint32_t start_pc = 0);

    // Interrupt check - returns true if interrupt was taken
    bool check_interrupts();

    // Check if interrupts are globally enabled
    bool interrupts_enabled() const { return mstatus & 0x8; }

private:
    void exec_op(uint32_t inst);
    void exec_op_imm(uint32_t inst);
    void exec_load(uint32_t inst);
    void exec_store(uint32_t inst);
    void exec_branch(uint32_t inst);
    void exec_jal(uint32_t inst);
    void exec_jalr(uint32_t inst);
    void exec_lui(uint32_t inst);
    void exec_auipc(uint32_t inst);
    void exec_system(uint32_t inst);
    void exec_amo(uint32_t inst);
    void exec_misc_mem(uint32_t inst);

    void illegal_instruction(uint32_t inst);
};

} // namespace cosmo
