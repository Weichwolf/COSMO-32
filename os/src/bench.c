// COSMO-32 Benchmark Suite
// Measures CPU, memory, and display performance using host clock

#include "const.h"
#include <stdint.h>

// External I/O functions from shell.S
extern void putchar(int c);
extern void print_str(const char* s);
extern void print_dec(uint32_t n);

// Linker symbols
extern char _bss_end[];

// Host clock access
static volatile uint32_t* const HOSTCLOCK = (volatile uint32_t*)HOSTCLOCK_BASE;

static inline void clock_latch(void) {
    HOSTCLOCK[2] = 0;  // Write to LATCH register
}

static inline uint64_t clock_read(void) {
    uint32_t lo = HOSTCLOCK[0];
    uint32_t hi = HOSTCLOCK[1];
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t clock_us(void) {
    clock_latch();
    return clock_read();
}

// Print results helper
static void print_result(const char* name, uint32_t ops, uint64_t us) {
    print_str("  ");
    print_str(name);
    print_str(": ");
    if (us > 0) {
        uint64_t rate64 = ((uint64_t)ops * 1000000ULL) / us;
        if (rate64 > 999999999ULL) {
            print_str(">1G");
        } else {
            print_dec((uint32_t)rate64);
        }
        print_str("/s");
    } else {
        print_str("N/A");
    }
    print_str("\n");
}

static void print_bw(const char* name, uint32_t bytes, uint64_t us) {
    print_str("  ");
    print_str(name);
    print_str(": ");
    if (us > 0) {
        uint32_t mbps = (uint32_t)((uint64_t)bytes / us);  // bytes/us = MB/s
        print_dec(mbps);
        print_str(" MB/s");
    } else {
        print_str("N/A");
    }
    print_str("\n");
}

// ============================================================================
// CPU Benchmarks
// ============================================================================

// Integer ALU: ADD/XOR in tight loop
static uint32_t bench_int_alu(void) {
    volatile uint32_t a = 0x12345678;
    volatile uint32_t b = 0x9ABCDEF0;
    uint32_t iterations = 1000000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a + b;
        b = b ^ a;
        a = a + b;
        b = b ^ a;
    }
    uint64_t end = clock_us();

    // 4 ops per iteration
    print_result("Integer ALU", iterations * 4, end - start);
    return a + b;  // Prevent optimization
}

// Integer MUL
static uint32_t bench_int_mul(void) {
    volatile uint32_t a = 12345;
    volatile uint32_t b = 67890;
    uint32_t iterations = 500000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a * b;
        b = b * a;
    }
    uint64_t end = clock_us();

    print_result("Integer MUL", iterations * 2, end - start);
    return a + b;
}

// Integer DIV
static uint32_t bench_int_div(void) {
    volatile uint32_t a = 0xFFFFFFFF;
    volatile uint32_t b = 7;
    uint32_t iterations = 100000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a / b;
        a = a | 0x80000000;  // Reset high bit to keep numbers large
    }
    uint64_t end = clock_us();

    print_result("Integer DIV", iterations, end - start);
    return a;
}

// Branch prediction test
static uint32_t bench_branch(void) {
    volatile uint32_t count = 0;
    uint32_t iterations = 500000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        if (i & 1) count++;
        if (i & 2) count++;
        if (i & 4) count++;
        if (i & 8) count++;
    }
    uint64_t end = clock_us();

    // 4 branches per iteration
    print_result("Branches", iterations * 4, end - start);
    return count;
}

// ============================================================================
// Memory Benchmarks
// ============================================================================

// Sequential read from SRAM
static void bench_mem_read_sram(void) {
    volatile uint32_t* mem = (volatile uint32_t*)SRAM_BASE;
    uint32_t words = 8192;  // 32KB
    volatile uint32_t sum = 0;

    uint64_t start = clock_us();
    for (int rep = 0; rep < 10; rep++) {
        for (uint32_t i = 0; i < words; i++) {
            sum += mem[i];
        }
    }
    uint64_t end = clock_us();

    print_bw("SRAM Read", words * 4 * 10, end - start);
    (void)sum;
}

// Sequential write to SRAM (after .bss, before stack)
static void bench_mem_write_sram(void) {
    // Align _bss_end to 4 bytes, use area between .bss and stack
    uintptr_t start_addr = ((uintptr_t)_bss_end + 3) & ~3;
    uintptr_t end_addr = SRAM_BASE + SRAM_SIZE - 1024;  // Leave 1KB for stack
    if (end_addr <= start_addr) {
        print_str("  SRAM Write: skip (no space)\n");
        return;
    }
    uint32_t words = (end_addr - start_addr) / 4;
    volatile uint32_t* mem = (volatile uint32_t*)start_addr;

    uint64_t start = clock_us();
    for (int rep = 0; rep < 10; rep++) {
        for (uint32_t i = 0; i < words; i++) {
            mem[i] = i;
        }
    }
    uint64_t end = clock_us();

    print_bw("SRAM Write", words * 4 * 10, end - start);
}

// Sequential read from FSMC (external SRAM)
static void bench_mem_read_fsmc(void) {
    volatile uint32_t* mem = (volatile uint32_t*)FSMC_BASE;
    uint32_t words = 32768;  // 128KB
    volatile uint32_t sum = 0;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < words; i++) {
        sum += mem[i];
    }
    uint64_t end = clock_us();

    print_bw("FSMC Read", words * 4, end - start);
    (void)sum;
}

// Sequential write to FSMC
static void bench_mem_write_fsmc(void) {
    volatile uint32_t* mem = (volatile uint32_t*)FSMC_BASE;
    uint32_t words = 32768;  // 128KB

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < words; i++) {
        mem[i] = i;
    }
    uint64_t end = clock_us();

    print_bw("FSMC Write", words * 4, end - start);
}

// ============================================================================
// Floating Point (Soft-Float)
// ============================================================================

static volatile float bench_float_dummy;

static void bench_float_add(void) {
    volatile float a = 1.5f;
    volatile float b = 2.5f;
    uint32_t iterations = 100000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a + b;
        b = b + a;
    }
    uint64_t end = clock_us();

    bench_float_dummy = a + b;
    print_result("Float ADD", iterations * 2, end - start);
}

static void bench_float_mul(void) {
    volatile float a = 1.00001f;
    volatile float b = 0.99999f;
    uint32_t iterations = 100000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a * b;
        b = b * a;
    }
    uint64_t end = clock_us();

    bench_float_dummy = a + b;
    print_result("Float MUL", iterations * 2, end - start);
}

static void bench_float_div(void) {
    volatile float a = 123456.789f;
    volatile float b = 1.0001f;
    uint32_t iterations = 50000;

    uint64_t start = clock_us();
    for (uint32_t i = 0; i < iterations; i++) {
        a = a / b;
    }
    uint64_t end = clock_us();

    bench_float_dummy = a;
    print_result("Float DIV", iterations, end - start);
}

// ============================================================================
// Math Functions
// ============================================================================

// Simple integer sqrt approximation
static uint32_t isqrt(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

static void bench_sqrt(void) {
    volatile uint32_t result = 0;
    uint32_t iterations = 50000;

    uint64_t start = clock_us();
    for (uint32_t i = 1; i <= iterations; i++) {
        result += isqrt(i * 1000);
    }
    uint64_t end = clock_us();

    (void)result;
    print_result("Int SQRT", iterations, end - start);
}

// 32x32 matrix multiply (integer)
#define MAT_N 32
static int32_t mat_a[MAT_N][MAT_N];
static int32_t mat_b[MAT_N][MAT_N];
static int32_t mat_c[MAT_N][MAT_N];

static void bench_matrix(void) {
    // Init matrices
    for (int i = 0; i < MAT_N; i++) {
        for (int j = 0; j < MAT_N; j++) {
            mat_a[i][j] = i + j;
            mat_b[i][j] = i - j;
        }
    }

    uint64_t start = clock_us();
    // 200 iterations of matrix multiply
    for (int rep = 0; rep < 200; rep++) {
        for (int i = 0; i < MAT_N; i++) {
            for (int j = 0; j < MAT_N; j++) {
                int32_t sum = 0;
                for (int k = 0; k < MAT_N; k++) {
                    sum += mat_a[i][k] * mat_b[k][j];
                }
                mat_c[i][j] = sum;
            }
        }
    }
    uint64_t end = clock_us();

    // 2*N^3 ops per multiply (N^3 muls + N^3 adds), 200 reps
    uint32_t ops = 2 * MAT_N * MAT_N * MAT_N * 200;

    // Prevent dead code elimination
    volatile int32_t anchor = mat_c[0][0] + mat_c[MAT_N-1][MAT_N-1];
    (void)anchor;

    print_result("Matrix 32x32", ops, end - start);
}

// Sieve of Eratosthenes - unpredictable branches
#define SIEVE_N 10000
static uint8_t sieve[SIEVE_N];

static void bench_sieve(void) {
    uint64_t start = clock_us();

    // Run sieve 10 times
    volatile uint32_t prime_count = 0;
    for (int rep = 0; rep < 10; rep++) {
        // Init
        for (int i = 0; i < SIEVE_N; i++) sieve[i] = 1;
        sieve[0] = sieve[1] = 0;

        // Sieve
        for (int i = 2; i * i < SIEVE_N; i++) {
            if (sieve[i]) {
                for (int j = i * i; j < SIEVE_N; j += i) {
                    sieve[j] = 0;
                }
            }
        }

        // Count primes
        uint32_t count = 0;
        for (int i = 0; i < SIEVE_N; i++) {
            if (sieve[i]) count++;
        }
        prime_count = count;
    }
    uint64_t end = clock_us();

    (void)prime_count;
    print_result("Sieve 10K", 10, end - start);
}

// ============================================================================
// Main Entry Point
// ============================================================================

void bench_main(void) {
    print_str("=== COSMO-32 Benchmark ===\n\n");

    print_str("CPU:\n");
    bench_int_alu();
    bench_int_mul();
    bench_int_div();
    bench_branch();

    print_str("\nMemory:\n");
    bench_mem_read_sram();
    bench_mem_write_sram();
    bench_mem_read_fsmc();
    bench_mem_write_fsmc();

    print_str("\nFloat (soft):\n");
    bench_float_add();
    bench_float_mul();
    bench_float_div();

    print_str("\nMath:\n");
    bench_sqrt();
    bench_matrix();
    bench_sieve();

    print_str("\n=== Done ===\n");
}
