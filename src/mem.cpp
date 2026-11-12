#include "mem.hpp"

#include <cstring>

std::vector<int> parse_pattern(std::string_view text) {
    std::vector<int> out;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == ' ') { ++i; continue; }
        if (text[i] == '?') {
            out.push_back(-1);
            while (i < text.size() && text[i] == '?') ++i;
            continue;
        }
        auto hex = [](char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
        out.push_back(hex(text[i]) * 16 + hex(text[i + 1]));
        i += 2;
    }
    return out;
}

void* find_unique(const std::uint8_t* begin, std::size_t size, const std::vector<int>& pat) {
    void* found = nullptr;
    if (pat.empty() || size < pat.size()) return nullptr;
    for (std::size_t i = 0; i + pat.size() <= size; ++i) {
        std::size_t k = 0;
        while (k < pat.size() && (pat[k] < 0 || begin[i + k] == pat[k])) ++k;
        if (k != pat.size()) continue;
        if (found) return nullptr;
        found = const_cast<std::uint8_t*>(begin + i);
    }
    return found;
}

bool text_section(const std::uint8_t** begin, std::size_t* size, HMODULE module) {
    auto* base = reinterpret_cast<const std::uint8_t*>(module ? module : GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::string_view(reinterpret_cast<const char*>(sec->Name), 5) == ".text") {
            *begin = base + sec->VirtualAddress;
            *size = sec->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

std::vector<const std::uint8_t*> find_calls_to(const std::uint8_t* begin, std::size_t size, const void* target) {
    std::vector<const std::uint8_t*> out;
    for (std::size_t i = 0; i + 5 <= size; ++i) {
        if (begin[i] != 0xE8) continue;
        std::int32_t rel;
        std::memcpy(&rel, begin + i + 1, 4);
        if (begin + i + 5 + rel == target) out.push_back(begin + i);
    }
    return out;
}

const void* function_start(const void* addr) {
    constexpr std::uint8_t kChainInfo = 0x4;  // UNW_FLAG_CHAININFO
    DWORD64 base = 0;
    const RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(addr), &base, nullptr);
    // A function split into chunks has chained entries; the primary one holds the real start
    for (int depth = 0; rf && depth < 32; ++depth) {
        auto* info = reinterpret_cast<const std::uint8_t*>(base + rf->UnwindData);
        if (!((info[0] >> 3) & kChainInfo)) return reinterpret_cast<const void*>(base + rf->BeginAddress);
        const std::uint8_t codes = info[2];  // UNWIND_CODE slots, padded to an even count, precede the chained entry
        rf = reinterpret_cast<const RUNTIME_FUNCTION*>(info + 4 + ((codes + 1) & ~1) * 2);
    }
    return nullptr;
}
