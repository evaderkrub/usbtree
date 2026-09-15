#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
namespace platform {
bool executable_directory(std::filesystem::path& path, std::string& error) noexcept;
bool write_text(const std::filesystem::path& path, const std::string& text, std::string& error) noexcept;
bool read_text(const std::filesystem::path& path, std::string& text, std::string& error) noexcept;
bool read_binary(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, std::string& error) noexcept;
std::string timestamp();
}
