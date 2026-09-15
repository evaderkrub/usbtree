#pragma once
#include <filesystem>
#include <string>
namespace platform {
bool executable_directory(std::filesystem::path& path, std::string& error) noexcept;
bool write_text(const std::filesystem::path& path, const std::string& text, std::string& error) noexcept;
std::string timestamp();
}
