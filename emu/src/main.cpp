// COSMO-32 Emulator
// CH32V307 (RV32IMAC) Emulator

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "cpu.hpp"
#include "bus.hpp"
#include "device/memory.hpp"
#include "device/usart.hpp"
#include "device/timer.hpp"
#include "device/pfic.hpp"
#include "device/dma.hpp"
#include "device/fsmc.hpp"
#include "device/display.hpp"
#include "device/i2s.hpp"
#include "device/eth.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Memory map constants
constexpr uint32_t FLASH_BASE = 0x00000000;
constexpr uint32_t FLASH_SIZE = 0x40000;      // 256KB
constexpr uint32_t SRAM_BASE  = 0x20000000;
constexpr uint32_t SRAM_SIZE  = 0x10000;      // 64KB

// Peripheral addresses
constexpr uint32_t USART1_BASE  = 0x40000000;
constexpr uint32_t USART1_SIZE  = 0x100;
constexpr uint32_t SYSTICK_BASE = 0xE0000000;
constexpr uint32_t SYSTICK_SIZE = 0x100;
constexpr uint32_t PFIC_BASE    = 0xE000E000;
constexpr uint32_t PFIC_SIZE    = 0x1000;
constexpr uint32_t DMA1_BASE    = 0x40020000;
constexpr uint32_t DMA1_SIZE    = 0x1000;
constexpr uint32_t DISPLAY_BASE = 0x40018000;
constexpr uint32_t DISPLAY_SIZE = 0x100;
constexpr uint32_t FSMC_BASE    = 0x60000000;
constexpr uint32_t FSMC_SIZE    = 0x100000;  // 1MB
constexpr uint32_t I2S_BASE     = 0x40013000;
constexpr uint32_t I2S_SIZE     = 0x100;
constexpr uint32_t ETH_BASE     = 0x40023000;
constexpr uint32_t ETH_SIZE     = 0x1000;

// Emulator context - centralizes device setup
struct EmulatorContext {
    cosmo::ROM flash{FLASH_SIZE};
    cosmo::RAM sram{SRAM_SIZE};
    cosmo::USART usart1;
    cosmo::SysTickTimer systick;
    cosmo::PFIC pfic;
    cosmo::DMA dma1;
    cosmo::FSMC fsmc;
    cosmo::DisplayControl display;
    cosmo::I2S i2s;
    cosmo::ETH eth;
    cosmo::Bus bus;
    cosmo::CPU cpu{&bus};

    EmulatorContext() {
        // Map all devices
        bus.map(FLASH_BASE, FLASH_SIZE, &flash);
        bus.map(SRAM_BASE, SRAM_SIZE, &sram);
        bus.map(USART1_BASE, USART1_SIZE, &usart1);
        bus.map(SYSTICK_BASE, SYSTICK_SIZE, &systick);
        bus.map(PFIC_BASE, PFIC_SIZE, &pfic);
        bus.map(DMA1_BASE, DMA1_SIZE, &dma1);
        bus.map(DISPLAY_BASE, DISPLAY_SIZE, &display);
        bus.map(FSMC_BASE, FSMC_SIZE, &fsmc);
        bus.map(I2S_BASE, I2S_SIZE, &i2s);
        bus.map(ETH_BASE, ETH_SIZE, &eth);

        // DMA needs bus access for transfers
        dma1.set_bus_callbacks(
            [this](uint32_t addr, cosmo::Width w) { return bus.read(addr, w); },
            [this](uint32_t addr, cosmo::Width w, uint32_t val) { bus.write(addr, w, val); }
        );

        // ETH needs bus access for DMA descriptors
        eth.set_bus_callbacks(
            [this](uint32_t addr, cosmo::Width w) { return bus.read(addr, w); },
            [this](uint32_t addr, cosmo::Width w, uint32_t val) { bus.write(addr, w, val); }
        );
        eth.set_tftp_root("fs");

        // Connect CPU to PFIC
        cpu.set_pfic(&pfic);
    }

    bool load_firmware(const char* path) {
        if (!flash.load_file(path)) {
            std::fprintf(stderr, "Failed to load: %s\n", path);
            return false;
        }
        cpu.reset(FLASH_BASE);
        return true;
    }

    // Tick all peripherals and handle interrupts
    void tick_peripherals() {
        if (auto irq = systick.tick(cpu.cycles)) {
            pfic.set_pending(irq->cause);
            cpu.mip |= cosmo::MIE_MEIE;
        }
        if (auto irq = dma1.tick(cpu.cycles)) {
            pfic.set_pending(irq->cause);
            cpu.mip |= cosmo::MIE_MEIE;
        }
        if (auto irq = eth.tick(cpu.cycles)) {
            pfic.set_pending(irq->cause);
            cpu.mip |= cosmo::MIE_MEIE;
        }
    }
};

// Test result detection
enum class TestResult { Running, Pass, Fail };

TestResult check_test_result(cosmo::CPU& cpu) {
    if (cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
        uint32_t gp = cpu.reg(3);   // x3 = gp
        uint32_t a0 = cpu.reg(10);  // x10 = a0
        return (gp == 1 && a0 == 0) ? TestResult::Pass : TestResult::Fail;
    }
    return TestResult::Running;
}

// Test runner
std::string usart_output;

bool run_test(const char* path, bool verbose) {
    EmulatorContext emu;

    // Capture USART output
    usart_output.clear();
    emu.usart1.set_output_callback([](char c) { usart_output += c; });

    if (!emu.load_firmware(path)) return false;

    constexpr uint64_t MAX_CYCLES = 100'000'000;

    while (emu.cpu.cycles < MAX_CYCLES && !emu.cpu.halted) {
        emu.tick_peripherals();
        emu.cpu.step();

        TestResult result = check_test_result(emu.cpu);
        if (result == TestResult::Pass) {
            if (verbose) std::printf("PASS: %s\n", path);
            return true;
        } else if (result == TestResult::Fail) {
            uint32_t test_num = emu.cpu.reg(3) >> 1;
            std::printf("FAIL: %s (test #%u)\n", path, test_num);
            if (!usart_output.empty()) {
                std::printf("USART output: %s\n", usart_output.c_str());
            }
            return false;
        }

        if (emu.cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
            break;
        }
    }

    if (emu.cpu.cycles >= MAX_CYCLES) {
        std::printf("TIMEOUT: %s\n", path);
        return false;
    }

    std::printf("UNKNOWN: %s (halted without ECALL)\n", path);
    return false;
}

bool run_tests(const char* dir_path) {
    std::vector<fs::path> test_files;

    for (auto& entry : fs::recursive_directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bin") {
            std::string name = entry.path().filename().string();
            if (name.find(".dump") == std::string::npos) {
                test_files.push_back(entry.path());
            }
        }
    }

    if (test_files.empty()) {
        std::fprintf(stderr, "No test files found in: %s\n", dir_path);
        return false;
    }

    std::sort(test_files.begin(), test_files.end());

    int passed = 0, failed = 0;
    for (auto& path : test_files) {
        if (run_test(path.string().c_str(), true)) {
            passed++;
        } else {
            failed++;
        }
    }

    std::printf("\n=== Results ===\n");
    std::printf("Passed: %d\n", passed);
    std::printf("Failed: %d\n", failed);
    std::printf("Total:  %d\n", passed + failed);

    return failed == 0;
}

// Interactive emulator with SDL
void run_emulator(const char* firmware_path) {
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    EmulatorContext emu;

    emu.usart1.set_output_callback([](char c) {
        std::putchar(c);
        std::fflush(stdout);
    });

    if (!emu.load_firmware(firmware_path)) {
        SDL_Quit();
        return;
    }

    std::printf("COSMO-32 Emulator\n");
    std::printf("Firmware: %s\n", firmware_path);
    std::printf("Running...\n");

    constexpr uint64_t CYCLES_PER_FRAME = 144'000'000 / 60;

    bool running = true;
    while (running && !emu.cpu.halted) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        uint64_t target = emu.cpu.cycles + CYCLES_PER_FRAME;
        while (emu.cpu.cycles < target && !emu.cpu.halted) {
            emu.tick_peripherals();
            emu.cpu.step();

            if (emu.cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
                std::printf("\nECALL at PC=0x%08X, a0=%u\n", emu.cpu.mepc, emu.cpu.reg(10));
                emu.cpu.halted = true;
                break;
            }
        }

        SDL_Delay(16);
    }

    std::printf("Emulator stopped after %lu cycles\n", static_cast<unsigned long>(emu.cpu.cycles));
    SDL_Quit();
}

// Headless mode
void run_headless(const char* firmware_path, const char* input_str = nullptr,
                   uint64_t timeout_ms = 0, bool batch_mode = false) {
    EmulatorContext emu;

    emu.usart1.set_output_callback([](char c) {
        std::putchar(c);
        std::fflush(stdout);
    });

    if (input_str) {
        emu.usart1.queue_input(input_str);
        emu.usart1.queue_input("\nexit\n");
    } else if (batch_mode) {
        std::string all_input;
        std::string line;
        while (std::getline(std::cin, line)) {
            all_input += line + "\n";
        }
        if (!all_input.empty()) {
            emu.usart1.queue_input(all_input.c_str());
            emu.usart1.queue_input("exit\n");
        }
    }

    if (!emu.load_firmware(firmware_path)) return;

    constexpr uint64_t CYCLES_PER_MS = 144'000;
    uint64_t max_cycles = (timeout_ms > 0) ? timeout_ms * CYCLES_PER_MS : 100'000'000;

    while (emu.cpu.cycles < max_cycles && !emu.cpu.halted) {
        emu.tick_peripherals();
        emu.cpu.step();

        if (emu.cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
            break;
        }
    }

    if (emu.cpu.cycles >= max_cycles && timeout_ms > 0) {
        std::fprintf(stderr, "\nTimeout after %lu ms\n", static_cast<unsigned long>(timeout_ms));
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "COSMO-32 Emulator\n");
        std::fprintf(stderr, "Usage: cosmo32 <firmware.bin>\n");
        std::fprintf(stderr, "       cosmo32 --headless <firmware.bin> [options]\n");
        std::fprintf(stderr, "       cosmo32 --run-tests <test-dir>\n");
        std::fprintf(stderr, "       cosmo32 --test <test-file.bin>\n");
        std::fprintf(stderr, "\nHeadless options:\n");
        std::fprintf(stderr, "  --cmd <command>     Execute single command, then exit\n");
        std::fprintf(stderr, "  --timeout <ms>      Exit after timeout (milliseconds)\n");
        std::fprintf(stderr, "  --batch             Read commands from stdin\n");
        return 1;
    }

    if (std::strcmp(argv[1], "--run-tests") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Error: --run-tests requires a directory path\n");
            return 1;
        }
        return run_tests(argv[2]) ? 0 : 1;
    }

    if (std::strcmp(argv[1], "--test") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Error: --test requires a file path\n");
            return 1;
        }
        return run_test(argv[2], true) ? 0 : 1;
    }

    if (std::strcmp(argv[1], "--headless") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "Error: --headless requires a firmware path\n");
            return 1;
        }
        const char* firmware = argv[2];
        const char* cmd = nullptr;
        uint64_t timeout_ms = 0;
        bool batch_mode = false;

        for (int i = 3; i < argc; i++) {
            if (std::strcmp(argv[i], "--cmd") == 0 && i + 1 < argc) {
                cmd = argv[++i];
            } else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
                timeout_ms = std::strtoull(argv[++i], nullptr, 10);
            } else if (std::strcmp(argv[i], "--batch") == 0) {
                batch_mode = true;
            }
        }

        run_headless(firmware, cmd, timeout_ms, batch_mode);
        return 0;
    }

    run_emulator(argv[1]);
    return 0;
}
