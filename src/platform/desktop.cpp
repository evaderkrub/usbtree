#include "platform/desktop.h"
#include "platform/files.h"
#include "platform/usb.h"
#include "ui/renderer.h"
#include <SDL3/SDL.h>
#include <future>
#include <iostream>
#include <string_view>

namespace platform {
namespace {
struct ScanResult { app::Snapshot snapshot; std::string error; bool ok = false; };
int run_app(int argc, char** argv) {
    bool demo = false, smoke = false;
    for (int i = 1; i < argc; ++i) { demo |= std::string_view(argv[i]) == "--demo"; smoke |= std::string_view(argv[i]) == "--smoke"; }
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << SDL_GetError(); return 1; }
    std::filesystem::path base; std::string error;
    if (!executable_directory(base, error)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"USB Tree",error.c_str(),nullptr); SDL_Quit(); return 1; }
    auto* window = SDL_CreateWindow("USB Tree", 1440, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(window, 800, 560);
    auto* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_SetRenderVSync(renderer, 1);
    ui::Fonts fonts;
    if (!ui::initialize(window, renderer, base, fonts, error)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "USB Tree", error.c_str(), window);
        ui::shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    app::State state; state.snapshot.root.id = "computer"; state.snapshot.root.name = "This computer"; state.snapshot.root.kind = app::Kind::Computer;
    std::future<ScanResult> scan;
    bool running = true; unsigned frames = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui::process_event(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }
        if (state.refresh_requested && !state.scanning) {
            state.refresh_requested = false; state.scanning = true;
            scan = std::async(std::launch::async, [demo] {
                ScanResult r;
                if (demo) { r.snapshot = app::demo_snapshot(); r.ok = true; }
                else r.ok = enumerate_usb(r.snapshot, r.error);
                return r;
            });
        }
        if (scan.valid() && scan.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = scan.get(); state.error = result.error; state.scanning = false;
            if (result.ok) app::accept_snapshot(state, std::move(result.snapshot));
        }
        ui::frame(renderer, state, fonts, SDL_GetWindowDisplayScale(window));
        SDL_RenderPresent(renderer);
        if (smoke && ++frames > 60 && !state.scanning) running = false;
        SDL_Delay(8);
    }
    if (scan.valid()) scan.wait();
    ui::shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
}
int run(int argc, char** argv) noexcept {
    try { return run_app(argc, argv); }
    catch (const std::exception& e) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "USB Tree", e.what(), nullptr); return 1; }
}
}
