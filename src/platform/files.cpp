#include "platform/files.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace platform {
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
