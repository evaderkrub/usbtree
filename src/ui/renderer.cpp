#include "ui/renderer.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
namespace ui {
bool initialize(SDL_Window* window, SDL_Renderer* renderer, const std::filesystem::path& base, Fonts& fonts, std::string& error) {
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) { error = "SDL ImGui initialization failed"; return false; }
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) { error = "ImGui renderer initialization failed"; return false; }
    return load_fonts(base / "assets", fonts, error);
}
void process_event(const SDL_Event& event) { ImGui_ImplSDL3_ProcessEvent(&event); }
void load_layout(const std::string& text) { if (!text.empty()) ImGui::LoadIniSettingsFromMemory(text.data(), text.size()); }
std::string save_layout() { return ImGui::SaveIniSettingsToMemory(); }
void frame(SDL_Renderer* renderer, app::State& state, const Fonts& fonts, float dpi) {
    apply_theme(state.scale * dpi);
    ImGui_ImplSDLRenderer3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
    draw_workspace(state, fonts);
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer,22,23,27,255); SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}
void shutdown() {
    if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplSDLRenderer3_Shutdown();
    if (ImGui::GetIO().BackendPlatformUserData) ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
}
