#pragma once
#include "ui/workspace.h"
struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
namespace ui {
bool initialize(SDL_Window* window, SDL_Renderer* renderer, const std::filesystem::path& base, Fonts& fonts, std::string& error);
void process_event(const SDL_Event& event);
void frame(SDL_Renderer* renderer, app::State& state, const Fonts& fonts, float dpi);
void shutdown();
void load_layout(const std::string& text);
std::string save_layout();
}
