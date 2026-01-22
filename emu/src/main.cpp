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

        // Fast-path for frequent memory regions (direct data pointers)
        bus.set_fast_path(flash.data(), FLASH_SIZE,
                          sram.data(), SRAM_BASE, SRAM_SIZE);

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

        // Connect USART to PFIC for RX interrupts
        usart1.set_pfic(&pfic);
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

        // Handle WFI - check for pending interrupts to wake
        if (emu.cpu.wfi) {
            emu.cpu.check_interrupts();
            if (emu.cpu.wfi) {
                // Still waiting - increment cycles to avoid infinite loop
                emu.cpu.cycles++;
                continue;
            }
        }

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

// RGB565 to RGB888 conversion
inline void rgb565_to_rgb888(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = ((c >> 11) & 0x1F) << 3;
    g = ((c >> 5) & 0x3F) << 2;
    b = (c & 0x1F) << 3;
}

// Audio callback - called by SDL from audio thread
void audio_callback(void* userdata, Uint8* stream, int len) {
    auto* i2s = static_cast<cosmo::I2S*>(userdata);
    auto* out = reinterpret_cast<int16_t*>(stream);
    size_t samples = len / 4;  // Stereo 16-bit = 4 bytes per sample

    size_t filled = i2s->read_samples(out, samples);

    // Silence for unfilled portion
    if (filled < samples) {
        std::memset(out + filled * 2, 0, (samples - filled) * 4);
    }
}

// Render framebuffer to SDL texture
void render_framebuffer(SDL_Texture* texture, const cosmo::FSMC& fsmc,
                        const cosmo::DisplayControl& display) {
    void* pixels;
    int pitch;

    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) return;

    auto* dst = static_cast<uint32_t*>(pixels);
    const uint8_t* fb = fsmc.framebuffer();
    const auto& palette = display.palette();
    int w = display.width();
    int h = display.height();

    if (display.mode() == cosmo::DisplayMode::Mode0_640x400x4bpp) {
        // 4bpp indexed: 2 pixels per byte
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x += 2) {
                uint8_t byte = fb[(y * w + x) / 2];
                uint8_t idx0 = byte & 0x0F;
                uint8_t idx1 = (byte >> 4) & 0x0F;

                uint8_t r, g, b;
                rgb565_to_rgb888(palette[idx0], r, g, b);
                dst[y * (pitch / 4) + x] = (r << 16) | (g << 8) | b;

                rgb565_to_rgb888(palette[idx1], r, g, b);
                dst[y * (pitch / 4) + x + 1] = (r << 16) | (g << 8) | b;
            }
        }
    } else {
        // 16bpp RGB565 direct
        const auto* fb16 = reinterpret_cast<const uint16_t*>(fb);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint8_t r, g, b;
                rgb565_to_rgb888(fb16[y * w + x], r, g, b);
                dst[y * (pitch / 4) + x] = (r << 16) | (g << 8) | b;
            }
        }
    }

    SDL_UnlockTexture(texture);
}

// Interactive emulator with SDL
void run_emulator(const char* firmware_path) {
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
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

    // Create window (2x scale for visibility)
    constexpr int SCALE = 2;
    int win_w = cosmo::DisplayControl::MODE0_WIDTH * SCALE;
    int win_h = cosmo::DisplayControl::MODE0_HEIGHT * SCALE;

    SDL_Window* window = SDL_CreateWindow(
        "COSMO-32",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Create textures for both modes
    SDL_Texture* tex_mode0 = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
        cosmo::DisplayControl::MODE0_WIDTH, cosmo::DisplayControl::MODE0_HEIGHT);

    SDL_Texture* tex_mode1 = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
        cosmo::DisplayControl::MODE1_WIDTH, cosmo::DisplayControl::MODE1_HEIGHT);

    if (!tex_mode0 || !tex_mode1) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Setup audio
    SDL_AudioSpec want{}, have{};
    want.freq = emu.i2s.sample_rate();
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = &emu.i2s;

    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_dev == 0) {
        std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(audio_dev, 0);  // Start playback
    }

    std::printf("COSMO-32 Emulator\n");
    std::printf("Firmware: %s\n", firmware_path);
    std::printf("Display: %dx%d\n", emu.display.width(), emu.display.height());
    std::printf("Audio: %d Hz\n", have.freq);
    std::printf("Running... (ESC to quit)\n");

    SDL_StartTextInput();

    constexpr uint64_t CYCLES_PER_FRAME = 144'000'000 / 60;
    constexpr uint32_t FRAME_TIME_MS = 1000 / 60;  // ~16ms for 60fps

    bool running = true;
    while (running && !emu.cpu.halted) {
        uint32_t frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.keysym.sym == SDLK_RETURN) {
                        emu.usart1.queue_input("\n");
                    } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        emu.usart1.queue_input("\b");
                    }
                    break;

                case SDL_TEXTINPUT:
                    emu.usart1.queue_input(event.text.text);
                    break;
            }
        }

        // Run CPU for one frame (or until WFI)
        uint64_t target = emu.cpu.cycles + CYCLES_PER_FRAME;
        while (emu.cpu.cycles < target && !emu.cpu.halted && !emu.cpu.wfi) {
            emu.tick_peripherals();
            emu.cpu.step();

            if (emu.cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
                std::printf("\nECALL at PC=0x%08X, a0=%u\n", emu.cpu.mepc, emu.cpu.reg(10));
                emu.cpu.halted = true;
                break;
            }
        }
        // If CPU is waiting for interrupt, tick peripherals and check for wakeup
        if (emu.cpu.wfi) {
            emu.tick_peripherals();
            emu.cpu.check_interrupts();  // May wake from WFI and take interrupt
        }

        // Render display
        SDL_Texture* active_tex = (emu.display.mode() == cosmo::DisplayMode::Mode0_640x400x4bpp)
                                  ? tex_mode0 : tex_mode1;
        render_framebuffer(active_tex, emu.fsmc, emu.display);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, active_tex, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // Frame timing - sleep to maintain 60fps (or longer if idle)
        uint32_t frame_time = SDL_GetTicks() - frame_start;
        uint32_t sleep_time = FRAME_TIME_MS;
        if (emu.cpu.wfi && !emu.usart1.has_input()) {
            sleep_time = 100;  // Sleep longer when idle (100ms)
        }
        if (frame_time < sleep_time) {
            SDL_Delay(sleep_time - frame_time);
        }
    }

    std::printf("Emulator stopped after %lu cycles\n", static_cast<unsigned long>(emu.cpu.cycles));

    // Cleanup
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(tex_mode0);
    SDL_DestroyTexture(tex_mode1);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Save framebuffer as PPM screenshot
void save_screenshot(const char* path, const cosmo::FSMC& fsmc, const cosmo::DisplayControl& display) {
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "Failed to save screenshot: %s\n", path);
        return;
    }

    int w = display.width();
    int h = display.height();
    const uint8_t* fb = fsmc.framebuffer();
    const auto& palette = display.palette();

    std::fprintf(f, "P6\n%d %d\n255\n", w, h);

    if (display.mode() == cosmo::DisplayMode::Mode0_640x400x4bpp) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int byte_offset = (y * w + x) / 2;
                uint8_t byte = fb[byte_offset];
                uint8_t idx = (x & 1) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
                uint16_t c = palette[idx];
                uint8_t r = ((c >> 11) & 0x1F) << 3;
                uint8_t g = ((c >> 5) & 0x3F) << 2;
                uint8_t b = (c & 0x1F) << 3;
                std::fputc(r, f);
                std::fputc(g, f);
                std::fputc(b, f);
            }
        }
    } else {
        const auto* fb16 = reinterpret_cast<const uint16_t*>(fb);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint16_t c = fb16[y * w + x];
                uint8_t r = ((c >> 11) & 0x1F) << 3;
                uint8_t g = ((c >> 5) & 0x3F) << 2;
                uint8_t b = (c & 0x1F) << 3;
                std::fputc(r, f);
                std::fputc(g, f);
                std::fputc(b, f);
            }
        }
    }

    std::fclose(f);
    std::fprintf(stderr, "Screenshot saved: %s\n", path);
}

// Headless mode
void run_headless(const char* firmware_path, const char* input_str = nullptr,
                   uint64_t timeout_ms = 0, bool batch_mode = false, const char* screenshot_path = nullptr) {
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

    auto start = std::chrono::steady_clock::now();
    uint64_t start_cycles = emu.cpu.cycles;

    constexpr uint64_t PERIPHERAL_TICK_INTERVAL = 10000;
    while (emu.cpu.cycles < max_cycles && !emu.cpu.halted) {
        uint64_t batch_target = std::min(emu.cpu.cycles + PERIPHERAL_TICK_INTERVAL, max_cycles);
        emu.cpu.run(batch_target);
        emu.tick_peripherals();

        if (emu.cpu.mcause == static_cast<uint32_t>(cosmo::TrapCause::ECallFromMMode)) {
            break;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    uint64_t total_cycles = emu.cpu.cycles - start_cycles;
    double mips = (total_cycles / 1000000.0) / (duration_ms / 1000.0);
    std::fprintf(stderr, "\n--- Performance ---\n");
    std::fprintf(stderr, "Cycles: %lu, Time: %lu ms, MIPS: %.2f\n",
                 static_cast<unsigned long>(total_cycles),
                 static_cast<unsigned long>(duration_ms), mips);

    if (emu.cpu.cycles >= max_cycles && timeout_ms > 0) {
        std::fprintf(stderr, "Timeout after %lu ms\n", static_cast<unsigned long>(timeout_ms));
    }

    if (screenshot_path) {
        save_screenshot(screenshot_path, emu.fsmc, emu.display);
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
        std::fprintf(stderr, "  --screenshot <path> Save framebuffer as PPM before exit\n");
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
        const char* screenshot = nullptr;

        for (int i = 3; i < argc; i++) {
            if (std::strcmp(argv[i], "--cmd") == 0 && i + 1 < argc) {
                cmd = argv[++i];
            } else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
                timeout_ms = std::strtoull(argv[++i], nullptr, 10);
            } else if (std::strcmp(argv[i], "--batch") == 0) {
                batch_mode = true;
            } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
                screenshot = argv[++i];
            }
        }

        run_headless(firmware, cmd, timeout_ms, batch_mode, screenshot);
        return 0;
    }

    run_emulator(argv[1]);
    return 0;
}
