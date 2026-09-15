#include "platform/desktop.h"
#include "platform/files.h"
#include "platform/usb.h"
#include "platform/notifications.h"
#include "ui/renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <future>
#include <iostream>
#include <string_view>
#ifdef USBTREE_E2E
#include "e2e.h"
#endif
namespace platform {
namespace {
struct ScanResult { app::Snapshot snapshot; std::string error; bool ok = false; };
bool set_window_icon(SDL_Window* window, const std::filesystem::path& base, std::string& error) {
    const auto filename = (base / "assets" / "icons" / "usbtree.bmp").u8string();
    SDL_Surface* icon = SDL_LoadBMP(reinterpret_cast<const char*>(filename.c_str()));
    if (!icon) { error = SDL_GetError(); return false; }
    // BMP preserves alpha while keeping the portable application independent of an image-codec library.
    const bool ok = SDL_SetWindowIcon(window, icon);
    if (!ok) error = SDL_GetError();
    SDL_DestroySurface(icon);
    return ok;
}
bool capture(SDL_Renderer* renderer, const std::filesystem::path& path, std::string& error) {
    std::error_code ec; std::filesystem::create_directories(path.parent_path(),ec);
    if (ec) { error = ec.message(); return false; }
    auto* surface = SDL_RenderReadPixels(renderer,nullptr);
    if (!surface) { error = SDL_GetError(); return false; }
    const auto utf8 = path.u8string();
    const bool ok = SDL_SaveBMP(surface,reinterpret_cast<const char*>(utf8.c_str()));
    SDL_DestroySurface(surface);
    if (!ok) error = SDL_GetError();
    return ok;
}
int run_app(int argc, char** argv) {
    bool demo = false, smoke = false;
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        demo |= arg == "--demo"; smoke |= arg == "--smoke";
        if (arg == "--capture" && i + 1 < argc) screenshot = argv[++i];
    }
#ifdef USBTREE_E2E
    demo = true;
#endif
    const bool persist = !demo && !smoke;
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << SDL_GetError(); return 1; }
    std::filesystem::path base; std::string error;
    if (!executable_directory(base,error)) { SDL_Quit(); return 1; }
    app::State state; state.demo = demo;
    state.snapshot.root.id = "computer"; state.snapshot.root.name = "This computer"; state.snapshot.root.kind = app::Kind::Computer;
    int width = 1440, height = 900; std::string preferences;
    if (persist && read_text(base / "settings" / "preferences.txt",preferences,error))
        app::load_preferences(preferences,state,width,height);
    auto* window = SDL_CreateWindow("USB Tree",width,height,SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { SDL_Quit(); return 1; }
    // Some Linux window managers control their own icons; that must not prevent device inspection.
    if (!set_window_icon(window,base,error)) std::cerr << "Window icon: " << error << '\n';
    SDL_SetWindowMinimumSize(window,800,560);
    auto* renderer = SDL_CreateRenderer(window,nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_SetRenderVSync(renderer,1);
    ui::Fonts fonts;
    if (!ui::initialize(window,renderer,base,fonts,error)) {
        if (!smoke) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"USB Tree",error.c_str(),window);
        std::cerr << error << '\n';
        ui::shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    std::string layout;
    if (persist && read_text(base / "settings" / "layout.ini",layout,error)) ui::load_layout(layout);
    DeviceNotifications notifications;
    std::future<ScanResult> scan;
    bool running = true; unsigned frames = 0; int exit_code = 0;
    Uint64 change_at = 0, notice_at = 0;
#ifdef USBTREE_E2E
    e2e::start(state,base);
    bool tests_completed = false;
#endif
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui::process_event(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }
        if (notifications.consume() && !demo && state.auto_refresh) change_at = SDL_GetTicks();
        if (change_at && SDL_GetTicks() - change_at > 700) {
            if (state.auto_refresh) state.refresh_requested = true;
            change_at = 0;
        }
        if (state.refresh_requested && !state.scanning) {
            state.refresh_requested = false; state.scanning = true; state.notice.clear();
            scan = std::async(std::launch::async,[demo] {
                ScanResult r;
                if (demo) { r.snapshot = app::demo_snapshot(); r.ok = true; }
                else r.ok = enumerate_usb(r.snapshot,r.error);
                return r;
            });
        }
        if (scan.valid() && scan.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = scan.get(); state.error = result.error; state.scanning = false;
            if (result.ok) {
                if (!demo && !notifications.available()) result.snapshot.warnings.push_back("Device notifications unavailable. Use Refresh to update the tree.");
                app::accept_snapshot(state,std::move(result.snapshot));
            }
        }
        if (state.export_requested) {
            state.export_requested = false;
            auto stamp = timestamp(); std::replace(stamp.begin(),stamp.end(),':','-'); std::replace(stamp.begin(),stamp.end(),' ','_');
            const auto name = "usb-tree_" + stamp + "_" + std::to_string(SDL_GetTicks()) + ".txt";
            if (write_text(base / "reports" / name,state.report,state.error)) { state.notice = "Saved reports/" + name; notice_at = SDL_GetTicks(); }
        }
        if (state.copy_requested) {
            state.copy_requested = false;
            auto* n = app::find(state.snapshot.root,state.selected_id); if (!n) n = &state.snapshot.root;
            if (SDL_SetClipboardText(app::node_report(*n).c_str())) { state.notice = "Device report copied to clipboard"; notice_at = SDL_GetTicks(); }
            else state.error = SDL_GetError();
        }
        if (notice_at && SDL_GetTicks() - notice_at > 7000) { state.notice.clear(); notice_at = 0; }
        if (state.requested_width && state.requested_height) {
            SDL_SetWindowSize(window,state.requested_width,state.requested_height); state.requested_width = state.requested_height = 0;
        }
        // ImGui uses window coordinates; its SDL renderer already applies pixel density.
        const float density = SDL_GetWindowPixelDensity(window);
        const float display_scale = SDL_GetWindowDisplayScale(window);
        ui::frame(renderer,state,fonts,density > 0 && display_scale > 0 ? display_scale / density : 1.0f);
        if (!state.capture_name.empty()) {
            if (!capture(renderer,base / "captures" / std::filesystem::path(state.capture_name).filename(),state.error)) exit_code = 1;
            state.capture_name.clear();
        }
        if (!screenshot.empty() && frames > 60 && !state.scanning) {
            if (!capture(renderer,base / "captures" / std::filesystem::path(screenshot).filename(),state.error)) exit_code = 1;
            screenshot.clear();
        }
        SDL_RenderPresent(renderer);
#ifdef USBTREE_E2E
        const int test_result = e2e::tick();
        if (test_result >= 0) { exit_code = std::max(exit_code,test_result); tests_completed = true; running = false; }
#endif
        ++frames;
        if (smoke && frames > 65 && !state.scanning) { running = false; if (!state.error.empty()) exit_code = 1; }
        SDL_Delay(8);
    }
    if (scan.valid()) scan.wait();
    if (persist) {
        SDL_GetWindowSize(window,&width,&height);
        if (!write_text(base / "settings" / "preferences.txt",app::save_preferences(state,width,height),error) ||
            !write_text(base / "settings" / "layout.ini",ui::save_layout(),error))
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,"USB Tree - settings",error.c_str(),window);
    }
#ifdef USBTREE_E2E
    if (!tests_completed) exit_code = 1;
    e2e::stop();
#endif
    ui::shutdown();
#ifdef USBTREE_E2E
    e2e::destroy();
#endif
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return exit_code;
}
}
int run(int argc, char** argv) noexcept {
    try { return run_app(argc,argv); }
    catch (const std::exception& e) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"USB Tree",e.what(),nullptr); return 1; }
}
}
