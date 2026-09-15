#pragma once
#include <filesystem>
#include <string>
struct ImFont;
namespace ui {
struct Fonts { ImFont* regular = nullptr; ImFont* bold = nullptr; ImFont* mono = nullptr; };
bool load_fonts(const std::filesystem::path& assets, Fonts& fonts, std::string& error);
void apply_theme(float scale);
}
