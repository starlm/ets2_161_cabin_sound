#pragma once

#include <windows.h>
#include <vector>
#include <string_view>
#include <cstdint>

std::vector<int> parse_pattern(std::string_view text);
void* find_unique(const std::uint8_t* begin, std::size_t size, const std::vector<int>& pat);
bool text_section(const std::uint8_t** begin, std::size_t* size, HMODULE module = nullptr);
std::vector<const std::uint8_t*> find_calls_to(const std::uint8_t* begin, std::size_t size, const void* target);
const void* function_start(const void* addr);
