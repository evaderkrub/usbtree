#include "ui/theme.h"
#include "imgui.h"
#include "ui/icons.h"
#include <fstream>

namespace ui {
namespace {
ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return {r/255.0f, g/255.0f, b/255.0f, a}; }
ImFont* load(const std::filesystem::path& p, float size, const ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {
    // Read with a filesystem path so portable folders with non-ASCII names work on Windows.
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) return nullptr;
    const auto size_in = in.tellg();
    if (size_in <= 0 || size_in > 16000000) return nullptr;
    void* bytes = IM_ALLOC(static_cast<std::size_t>(size_in));
    in.seekg(0); in.read(static_cast<char*>(bytes), static_cast<std::streamsize>(size_in));
    if (!in) { IM_FREE(bytes); return nullptr; }
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(bytes, static_cast<int>(size_in), size, config, ranges);
}
}
bool load_fonts(const std::filesystem::path& assets, Fonts& fonts, std::string& error) {
    const auto dir = assets / "fonts";
    static const ImWchar icons[] = {ICON_MIN_MD, ICON_MAX_16_MD, 0};
    auto merge = [&]() {
        ImFontConfig cfg; cfg.MergeMode = true; cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = 17.0f; cfg.GlyphOffset.y = 3.4f;
        return load(dir / "MaterialIcons-Regular.ttf", 17, &cfg, icons) != nullptr;
    };
    fonts.regular = load(dir / "OpenSans-Regular.ttf", 17);
    if (!fonts.regular || !merge()) { error = "Missing Open Sans or Material Icons in assets/fonts."; return false; }
    fonts.bold = load(dir / "OpenSans-SemiBold.ttf", 17);
    if (!fonts.bold || !merge()) { error = "Missing Open Sans Semibold in assets/fonts."; return false; }
    fonts.mono = load(dir / "FiraCode-Regular.ttf", 15);
    if (!fonts.mono) { error = "Missing Fira Code in assets/fonts."; return false; }
    ImGui::GetIO().FontDefault = fonts.regular;
    return true;
}
void apply_theme(float scale) {
    // fwcom Wili Dark palette and metrics, copied from fwTheme.cpp. Reset before scaling to prevent drift.
    ImGui::GetStyle() = ImGuiStyle();
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowPadding = {12,12}; s.FramePadding = {10,6}; s.CellPadding = {6,4};
    s.ItemSpacing = {8,7}; s.ItemInnerSpacing = {6,5}; s.IndentSpacing = 21;
    s.ScrollbarSize = 12; s.GrabMinSize = 10; s.WindowBorderSize = 0; s.ChildBorderSize = 1;
    s.FrameBorderSize = 0; s.PopupBorderSize = 1; s.TabBorderSize = 0;
    s.WindowRounding = 6; s.ChildRounding = 8; s.FrameRounding = 6; s.PopupRounding = 6;
    s.ScrollbarRounding = 6; s.GrabRounding = 6; s.TabRounding = 6;
    auto* c = s.Colors;
    c[ImGuiCol_Text] = rgb(228,230,235); c[ImGuiCol_TextDisabled] = rgb(150,156,168);
    c[ImGuiCol_WindowBg] = rgb(22,23,27); c[ImGuiCol_ChildBg] = rgb(25,26,31);
    c[ImGuiCol_PopupBg] = rgb(30,32,38); c[ImGuiCol_Border] = rgb(52,56,66);
    c[ImGuiCol_FrameBg] = rgb(39,41,49); c[ImGuiCol_FrameBgHovered] = rgb(48,51,60); c[ImGuiCol_FrameBgActive] = rgb(58,61,70);
    c[ImGuiCol_TitleBg] = rgb(22,23,27); c[ImGuiCol_TitleBgActive] = rgb(25,26,31); c[ImGuiCol_MenuBarBg] = rgb(25,26,31);
    c[ImGuiCol_Button] = rgb(39,41,49); c[ImGuiCol_ButtonHovered] = rgb(57,60,70); c[ImGuiCol_ButtonActive] = rgb(70,73,84);
    c[ImGuiCol_Header] = rgb(200,48,48,0.25f); c[ImGuiCol_HeaderHovered] = rgb(50,53,63); c[ImGuiCol_HeaderActive] = rgb(200,48,48,0.4f);
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_TabSelectedOverline] = c[ImGuiCol_NavCursor] = rgb(200,48,48);
    c[ImGuiCol_Tab] = rgb(22,23,27); c[ImGuiCol_TabSelected] = rgb(39,41,49); c[ImGuiCol_TabHovered] = rgb(57,60,70);
    c[ImGuiCol_TabDimmed] = rgb(22,23,27); c[ImGuiCol_TabDimmedSelected] = rgb(25,26,31);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(200,48,48,0.0f);
    c[ImGuiCol_Separator] = rgb(52,56,66); c[ImGuiCol_DockingPreview] = rgb(200,48,48,0.55f);
    c[ImGuiCol_TableHeaderBg] = rgb(39,41,49); c[ImGuiCol_TableBorderStrong] = rgb(52,56,66);
    c[ImGuiCol_TableBorderLight] = rgb(52,56,66,0.55f); c[ImGuiCol_TableRowBgAlt] = {1,1,1,0.025f};
    c[ImGuiCol_TextSelectedBg] = rgb(200,48,48,0.35f); c[ImGuiCol_TextLink] = rgb(255,120,120);
    c[ImGuiCol_ModalWindowDimBg] = {0.04f,0.04f,0.05f,0.7f};
    s.ScaleAllSizes(scale); s.FontScaleMain = scale;
}
}
