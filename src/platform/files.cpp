#include "platform/files.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace platform {
bool read_binary(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, std::string& error) noexcept {
    try {
        bytes.clear(); error.clear();
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in || in.tellg() <= 0 || in.tellg() > 16000000) { error = "Cannot read asset file"; return false; }
        bytes.resize(static_cast<std::size_t>(in.tellg()));
        in.seekg(0); in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!in) { error = "Cannot read asset file"; return false; }
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool read_text(const std::filesystem::path& path, std::string& text, std::string& error) noexcept {
    try {
        text.clear(); error.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) && !ec) return true;
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in || in.tellg() < 0 || in.tellg() > 1048576) { error = "Cannot read settings file"; return false; }
        text.resize(static_cast<std::size_t>(in.tellg()));
        in.seekg(0); in.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!in) { error = "Cannot read settings file"; return false; }
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool executable_directory(std::filesystem::path& path, std::string& error) noexcept {
    try {
        const char* base = SDL_GetBasePath();
        if (!base) { error = SDL_GetError(); return false; }
        path = std::filesystem::path(reinterpret_cast<const char8_t*>(base)); error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool write_text(const std::filesystem::path& path, const std::string& text, std::string& error) noexcept {
    try {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { error = "Cannot create output directory: " + ec.message(); return false; }
        std::ofstream out(path, std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size())); out.close();
        if (!out) { error = "Cannot write " + path.string(); return false; }
        error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
std::string timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out; out << std::put_time(&local, "%Y-%m-%d %H:%M:%S"); return out.str();
}
}
