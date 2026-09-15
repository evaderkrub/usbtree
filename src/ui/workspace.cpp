#include "ui/workspace.h"
#include "imgui.h"
namespace ui {
void draw_workspace(app::State& state, const Fonts&) {
    ImGui::DockSpaceOverViewport();
    ImGui::Begin("USB topology");
    ImGui::TextUnformatted("USB Tree");
    ImGui::TextWrapped("%s", state.error.c_str());
    if (ImGui::Button("Refresh")) state.refresh_requested = true;
    ImGui::End();
}
}
