#include "e2e.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_exporters.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <string>
namespace e2e {
namespace {
ImGuiTestEngine* engine = nullptr;
app::State* state = nullptr;
std::filesystem::path output;
std::string xml_path;
std::string original_clipboard;
int frames = 0;
void reset(ImGuiTestContext* ctx) {
    for (int i = 0; i < 200 && (state->scanning || state->refresh_requested); ++i) ctx->Yield();
    state->search[0] = '\0'; state->scale = 1;
    state->show_empty = false; state->auto_refresh = false; state->notice.clear();
    state->reset_layout = true; state->selected_id = "computer";
    state->requested_width = 1440; state->requested_height = 900;
    app::accept_snapshot(*state,app::demo_snapshot()); ctx->Yield(8);
}
void screenshot(ImGuiTestContext* ctx, const char* name) {
    ctx->MouseMoveToPos({0,0}); ctx->Yield(3);
    state->capture_name = name; ctx->Yield(3);
}
void register_tests() {
    ImGuiTest* t = IM_REGISTER_TEST(engine,"usbtree","search_and_selection");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Connections");
        ctx->ItemInputValue("###Search","1234 ABCD");
        IM_CHECK_EQ(std::string(state->search.data()),"1234 ABCD");
        ctx->ItemClick("**/###keyboard");
        IM_CHECK_EQ(state->selected_id,"keyboard");
        IM_CHECK(!ctx->ItemExists("**/###disk"));
        screenshot(ctx,"selection.bmp");
        ctx->ItemClick("###ClearSearch");
        IM_CHECK_EQ(state->search[0],'\0');
        IM_CHECK(ctx->ItemExists("**/###disk"));
    };
    t = IM_REGISTER_TEST(engine,"usbtree","empty_ports_and_no_matches");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Connections");
        IM_CHECK(!ctx->ItemExists("**/###empty"));
        ctx->ItemCheck("Empty ports");
        IM_CHECK(state->show_empty);
        ctx->ItemClick("**/###empty"); IM_CHECK_EQ(state->selected_id,"empty");
        screenshot(ctx,"empty-port.bmp");
        ctx->ItemInputValue("###Search","no-such-device-zz");
        IM_CHECK(!ctx->ItemExists("**/###keyboard"));
        IM_CHECK(!ctx->ItemExists("**/###computer"));
        screenshot(ctx,"no-matches.bmp");
        ctx->ItemClick("###ClearSearch");
    };
    t = IM_REGISTER_TEST(engine,"usbtree","descriptor_tabs_and_clipboard");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Connections"); ctx->ItemClick("**/###keyboard");
        ctx->SetRef("Device details");
        ctx->ItemClick("DetailTabs/Descriptors"); ctx->Yield(3);
        screenshot(ctx,"descriptors.bmp");
        ctx->ItemClick("DetailTabs/Raw bytes"); ctx->Yield(3);
        screenshot(ctx,"raw-bytes.bmp");
        ctx->ItemClick("###CopyDetails"); ctx->Yield(3);
        char* text = SDL_GetClipboardText(); const std::string report = text ? text : ""; SDL_free(text);
        IM_CHECK(report.find("Studio Keyboard") != std::string::npos);
        IM_CHECK(report.find("Endpoint 0x81") != std::string::npos);
        ctx->ItemClick("DetailTabs/Overview");
    };
    t = IM_REGISTER_TEST(engine,"usbtree","modal_about_blocks_shortcuts");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Toolbar"); ctx->ItemClick("###About");
        ctx->Yield(3);
        auto* modal = ImGui::GetTopMostPopupModal(); IM_CHECK(modal != nullptr);
        IM_CHECK((modal->Flags & ImGuiWindowFlags_Modal) != 0);
        const auto generation = state->generation;
        ctx->KeyPress(ImGuiKey_F5); ctx->Yield(10);
        IM_CHECK_EQ(state->generation,generation);
        screenshot(ctx,"about.bmp");
        ctx->SetRef("About USB Tree"); ctx->ItemClick("Close"); ctx->Yield(3);
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
    };
    t = IM_REGISTER_TEST(engine,"usbtree","refresh_preserves_selection_and_export");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Connections"); ctx->ItemClick("**/###keyboard");
        const auto generation = state->generation;
        ctx->SetRef("Toolbar"); ctx->ItemClick("###Refresh");
        for (int i = 0; i < 200 && state->generation == generation; ++i) ctx->Yield();
        IM_CHECK_GT(state->generation,generation); IM_CHECK_EQ(state->selected_id,"keyboard");
        ctx->ItemClick("###Export"); ctx->Yield(5);
        IM_CHECK(state->error.empty()); IM_CHECK(state->notice.starts_with("Saved reports/"));
        const auto filename = state->notice.substr(6);
        std::ifstream in(output / filename,std::ios::binary);
        const std::string report((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        IM_CHECK(report.find("Studio Keyboard") != std::string::npos);
        IM_CHECK(report.find("Portable SSD") != std::string::npos);
    };
    t = IM_REGISTER_TEST(engine,"usbtree","scaling_and_narrow_window");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        reset(ctx); ctx->SetRef("Toolbar"); ctx->ItemClick("###Scale");
        ctx->ItemClick("//$FOCUSED/150%"); ctx->Yield(5);
        IM_CHECK_EQ(state->scale,1.5f);
        const float padding = ImGui::GetStyle().FramePadding.x; ctx->Yield(5);
        IM_CHECK_EQ(ImGui::GetStyle().FramePadding.x,padding);
        screenshot(ctx,"scale-150.bmp");
        ctx->ItemClick("###Scale"); ctx->ItemClick("//$FOCUSED/100%"); ctx->Yield(5);
        IM_CHECK_EQ(state->scale,1.0f);
        state->requested_width = 1000; state->requested_height = 720; ctx->Yield(8);
        ctx->ItemClick("###Layout"); ctx->Yield(5);
        IM_CHECK(ctx->ItemExists("###Refresh")); IM_CHECK(ctx->ItemExists("###About"));
        ctx->SetRef("Connections"); ctx->ItemInputValue("###Search","keyboard");
        ctx->ItemClick("**/###keyboard"); IM_CHECK_EQ(state->selected_id,"keyboard");
        screenshot(ctx,"narrow.bmp");
        ctx->SetRef("Toolbar"); ctx->ItemClick("###Scale"); ctx->ItemClick("//$FOCUSED/200%");
        state->requested_width = 800; state->requested_height = 560; ctx->Yield(8);
        const auto about = ctx->ItemInfo("###About");
        IM_CHECK_LE(about.RectFull.Max.x, ImGui::GetMainViewport()->Size.x);
        ctx->ItemClick("###About"); ctx->Yield(5);
        const auto* modal = ImGui::GetTopMostPopupModal(); IM_CHECK(modal != nullptr);
        IM_CHECK_LE(modal->Size.y, ImGui::GetMainViewport()->Size.y - 39);
        screenshot(ctx,"maximum-zoom.bmp");
        ctx->KeyPress(ImGuiKey_Escape); ctx->Yield(3);
        ctx->ItemClick("###Scale"); ctx->ItemClick("//$FOCUSED/100%"); ctx->Yield(5);
    };
}
}
void start(app::State& s, const std::filesystem::path& base) {
    state = &s; output = base;
    char* clipboard = SDL_GetClipboardText(); original_clipboard = clipboard ? clipboard : ""; SDL_free(clipboard);
    std::filesystem::create_directories(base / "test-results");
    xml_path = (base / "test-results" / "imgui-e2e.xml").string();
    engine = ImGuiTestEngine_CreateContext();
    auto& io = ImGuiTestEngine_GetIO(engine);
    io.ConfigSavedSettings = false; io.ConfigLogToTTY = true; io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info; io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    io.ConfigMouseDrawCursor = false;
    io.ExportResultsFilename = xml_path.c_str(); io.ExportResultsFormat = ImGuiTestEngineExportFormat_JUnitXml;
    ImGuiTestEngine_Start(engine,ImGui::GetCurrentContext());
    register_tests(); ImGuiTestEngine_QueueTests(engine,ImGuiTestGroup_Tests,"usbtree");
}
int tick() {
    ImGuiTestEngine_PostSwap(engine);
    if (++frames > 6000) { std::cerr << "E2E frame timeout\n"; return 1; }
    if (frames < 10 || !ImGuiTestEngine_IsTestQueueEmpty(engine)) return -1;
    ImGuiTestEngineResultSummary summary; ImGuiTestEngine_GetResultSummary(engine,&summary);
    std::cout << summary.CountSuccess << "/" << summary.CountTested << " ImGui end-to-end tests passed\n";
    return summary.CountTested == 6 && summary.CountSuccess == 6 ? 0 : 1;
}
void stop() { ImGuiTestEngine_Stop(engine); }
void destroy() { ImGuiTestEngine_DestroyContext(engine); engine = nullptr; SDL_SetClipboardText(original_clipboard.c_str()); }
}
