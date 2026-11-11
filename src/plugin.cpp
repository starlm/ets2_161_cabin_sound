// Hooks the following functions to use the three-band EQ instead of multiband for the cabin buses (IDs 2, 3, 4)
//     set_three_eq_enabled(sound_system, bus_id, enabled)
//     set_multiband_enabled(sound_system, bus_id, enabled)

#include "mem.hpp"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

using scs_result_t = std::int32_t;
using scs_log_t = void (*)(std::int32_t type, const char* message);
constexpr scs_result_t SCS_RESULT_ok = 0;
constexpr scs_result_t SCS_RESULT_unsupported = -1;
constexpr std::uint32_t SCS_TELEMETRY_VERSION_1_00 = 0x00010000;
constexpr std::int32_t SCS_LOG_TYPE_message = 0;
constexpr std::int32_t SCS_LOG_TYPE_error = 2;

struct scs_sdk_init_params_v100 {
    const char* game_name;
    const char* game_id;
    std::uint32_t game_version;
    std::uint32_t padding;
    scs_log_t log;
};

// The three bypass wrappers (reverb +0x30, three-EQ +0x38, multiband +0x40) share one body; the last byte is
// the DSP slot in the game's fmod_bus_t. Struct offsets and branch targets are wildcards
constexpr std::string_view kWrapperBody =
    "48 89 5C 24 18 57 48 83 EC 20 41 0F B6 F8 48 8B D9 B8 01 00 00 00 F0 0F C1 81 ? ? ? ? "
    "44 8B 89 ? ? ? ? 41 83 F9 02 75 ? 48 89 74 24 38 E8 ? ? ? ? 48 8B F0 48 85 C0 74 ? 48 8B 48";
constexpr std::uint8_t kSlotThreeEq = 0x38;
constexpr std::uint8_t kSlotMultiband = 0x40;

// FMOD bus IDs: bus:/cabin/window_left, window_right, window_out
constexpr int kBusWindowLeft = 2;
constexpr int kBusWindowRight = 3;
constexpr int kBusWindowOut = 4;

using set_dsp_enabled_fn = void (*)(void* sound_system, int bus_id, bool enabled);

scs_log_t g_log = nullptr;
void* g_three_eq_target = nullptr;
void* g_multiband_target = nullptr;
set_dsp_enabled_fn g_three_eq_original = nullptr;
set_dsp_enabled_fn g_multiband_original = nullptr;
bool g_hooks_created = false;

void log(std::int32_t type, const char* fmt, ...) {
    char msg[512];
    int n = std::snprintf(msg, sizeof(msg), "[ets2_161_cabin_sound] ");
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg + n, sizeof(msg) - n, fmt, ap);
    va_end(ap);
    if (g_log) g_log(type, msg);
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

bool is_cabin_bus(int bus) { return bus == kBusWindowLeft || bus == kBusWindowRight || bus == kBusWindowOut; }

// Last state the game requested via set_multiband_enabled, per cabin bus
constexpr int kNoRequest = -1;
std::atomic<int> g_cabin_eq[3] = {kNoRequest, kNoRequest, kNoRequest};

std::atomic<int>& cabin_eq(int bus) { return g_cabin_eq[bus - kBusWindowLeft]; }

void three_eq_detour(void* sound_system, int bus, bool enabled) {
    if (is_cabin_bus(bus)) {
        const int wanted = cabin_eq(bus).load();
        if (wanted != kNoRequest) enabled = wanted != 0;
    }
    g_three_eq_original(sound_system, bus, enabled);
}

void multiband_detour(void* sound_system, int bus, bool enabled) {
    if (is_cabin_bus(bus)) {
        cabin_eq(bus).store(enabled ? 1 : 0);
        g_three_eq_original(sound_system, bus, enabled);
        enabled = false;
    }
    g_multiband_original(sound_system, bus, enabled);
}

void* find_wrapper(const std::uint8_t* text, std::size_t size, std::uint8_t slot) {
    std::vector<int> pat = parse_pattern(kWrapperBody);
    pat.push_back(slot);
    return find_unique(text, size, pat);
}

void uninstall() {
    if (g_hooks_created) MH_DisableHook(MH_ALL_HOOKS);
    for (auto& state : g_cabin_eq) state.store(kNoRequest);
}

bool create_hooks() {
    const std::uint8_t* text = nullptr;
    std::size_t size = 0;
    if (!text_section(&text, &size)) {
        log(SCS_LOG_TYPE_error, "no .text section in the game exe");
        return false;
    }
    g_three_eq_target = find_wrapper(text, size, kSlotThreeEq);
    g_multiband_target = find_wrapper(text, size, kSlotMultiband);
    if (!g_three_eq_target || !g_multiband_target) {
        log(SCS_LOG_TYPE_error, "sound functions not found (three-EQ %p, multiband %p): unsupported game version, "
            "plugin inactive", g_three_eq_target, g_multiband_target);
        return false;
    }
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK) {
        log(SCS_LOG_TYPE_error, "MinHook init failed (%d)", static_cast<int>(init));
        return false;
    }
    if (MH_CreateHook(g_three_eq_target, reinterpret_cast<void*>(&three_eq_detour),
                      reinterpret_cast<void**>(&g_three_eq_original)) != MH_OK ||
        MH_CreateHook(g_multiband_target, reinterpret_cast<void*>(&multiband_detour),
                      reinterpret_cast<void**>(&g_multiband_original)) != MH_OK) {
        log(SCS_LOG_TYPE_error, "hooking failed");
        MH_Uninitialize();
        g_three_eq_original = g_multiband_original = nullptr;
        return false;
    }
    g_hooks_created = true;
    return true;
}

bool install() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(&install), &self)) {
        log(SCS_LOG_TYPE_error, "pinning the plugin DLL failed (%lu), plugin inactive", GetLastError());
        return false;
    }
    if (!g_hooks_created && !create_hooks()) return false;
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        log(SCS_LOG_TYPE_error, "enabling the hooks failed, plugin inactive");
        MH_DisableHook(MH_ALL_HOOKS);
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    log(SCS_LOG_TYPE_message, "active: 3-band cabin EQ as in 1.61 (hooks at exe+%#llx, exe+%#llx)",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(g_three_eq_target) - base),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(g_multiband_target) - base));
    return true;
}

}  // namespace

extern "C" {

__declspec(dllexport) scs_result_t scs_telemetry_init(std::uint32_t version, const scs_sdk_init_params_v100* params) {
    if (version < SCS_TELEMETRY_VERSION_1_00) return SCS_RESULT_unsupported;
    g_log = params ? params->log : nullptr;

    install();
    return SCS_RESULT_ok;
}

__declspec(dllexport) void scs_telemetry_shutdown() {
    uninstall();
    g_log = nullptr;
}

}  // extern "C"
