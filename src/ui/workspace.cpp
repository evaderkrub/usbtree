#include "ui/workspace.h"
#include "ui/views.h"
#include "ui/icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
namespace ui {
namespace {
void about(app::State& s, const Fonts& fonts) {
    if (s.about_open) { ImGui::OpenPopup("About USB Tree"); s.about_open = false; }
    const auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, {0.5f,0.5f});
    ImGui::SetNextWindowSize({std::min(510.0f * ImGui::GetStyle().FontScaleMain, vp->Size.x - 40),0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({0,0},{vp->Size.x - 40,vp->Size.y - 40});
    if (ImGui::BeginPopupModal("About USB Tree", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(fonts.bold,26); ImGui::TextUnformatted(ICON_MD_USB "  USB Tree"); ImGui::PopFont();
        ImGui::TextDisabled("Version 0.1.0  /  Windows preview"); ImGui::Separator();
        ImGui::TextWrapped("Explore the devices, hubs and connections behind your USB ports.");
        ImGui::Spacing();
        ImGui::TextWrapped("Inspired by Uwe Sieber's USB Device Tree Viewer. An independent C++20 application using SDL3 and Dear ImGui docking.");
        ImGui::Spacing();
        ImGui::TextWrapped("Wili Dark styling, Open Sans, Fira Code and Material Icons from fwcom. Third-party notices are included in the portable folder.");
        ImGui::Spacing(); ImGui::TextDisabled("USB enumeration: Windows\nmacOS and Linux backends: planned"); ImGui::Spacing();
        if (ImGui::Button("Close", {-1,0}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
}
void draw_workspace(app::State& s, const Fonts& fonts) {
    const auto* vp = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleMain;
    const bool compact = vp->Size.x / scale < 1120;
    const bool tiny = vp->Size.x / scale < 560;
    const float header = (compact ? 96.0f : 70.0f) * scale, footer = 30 * scale;
    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) s.refresh_requested = true;
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) s.focus_search = true;
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Equal)) s.scale = app::clamp_scale(s.scale + 0.25f);
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Minus)) s.scale = app::clamp_scale(s.scale - 0.25f);
    }
    constexpr ImGuiWindowFlags fixed = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(vp->Pos); ImGui::SetNextWindowSize({vp->Size.x,header});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::Begin("Toolbar",nullptr,fixed);
    ImGui::PushFont(fonts.bold,24); ImGui::TextColored({0.94f,0.27f,0.27f,1},ICON_MD_USB);
    ImGui::SameLine(); ImGui::TextUnformatted("USB TREE"); ImGui::PopFont();
    if (!tiny) { ImGui::SameLine(205 * scale); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled(s.demo ? "DEMO DATA" : "Device explorer"); }
    if (compact) ImGui::Spacing();
    else { ImGui::SameLine(); ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), vp->Size.x - 565 * scale)); }
    ImGui::BeginDisabled(s.scanning);
    ImGui::PushStyleColor(ImGuiCol_Button,{0.68f,0.16f,0.16f,1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.82f,0.20f,0.20f,1});
    if (ImGui::Button(tiny ? ICON_MD_REFRESH "###Refresh" : ICON_MD_REFRESH "  Refresh###Refresh")) s.refresh_requested = true;
    ImGui::PopStyleColor(2); ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh connected devices (F5)");
    ImGui::SameLine(); ImGui::BeginDisabled(s.generation == 0);
    if (ImGui::Button(tiny ? ICON_MD_FILE_DOWNLOAD "###Export" : ICON_MD_FILE_DOWNLOAD "  Export###Export")) s.export_requested = true;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save a complete text report in the portable reports folder");
    ImGui::SameLine(); ImGui::SetNextItemWidth((tiny ? 80 : 92) * scale);
    const auto percent = std::to_string(static_cast<int>(s.scale * 100)) + "%";
    if (ImGui::BeginCombo("###Scale",percent.c_str())) {
        for (int p : {75,100,125,150,175,200}) {
            const auto label = std::to_string(p) + "%";
            if (ImGui::Selectable(label.c_str(),static_cast<int>(s.scale * 100) == p)) s.scale = p / 100.0f;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(); if (ImGui::Button(ICON_MD_VIEW_QUILT "###Layout")) s.reset_layout = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset dock layout");
    ImGui::SameLine(); if (ImGui::Button(ICON_MD_INFO_OUTLINE "###About")) s.about_open = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("About USB Tree");
    ImGui::End();
    ImGui::SetNextWindowPos({vp->Pos.x,vp->Pos.y + header});
    ImGui::SetNextWindowSize({vp->Size.x,std::max(100.0f,vp->Size.y - header - footer)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("Workspace",nullptr,fixed);
    const ImGuiID dock = ImGui::GetID("UsbTreeDock");
    if (!ImGui::DockBuilderGetNode(dock) || s.reset_layout) {
        s.reset_layout = false;
        ImGui::DockBuilderRemoveNode(dock); ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock,ImGui::GetContentRegionAvail());
        ImGuiID left = 0, right = 0;
        ImGui::DockBuilderSplitNode(dock,ImGuiDir_Left,compact ? 0.43f : 0.36f,&left,&right);
        ImGui::DockBuilderDockWindow("Connections",left); ImGui::DockBuilderDockWindow("Device details",right);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpace(dock); ImGui::End(); ImGui::PopStyleVar();
    draw_tree(s,fonts); draw_details(s,fonts);
    ImGui::SetNextWindowPos({vp->Pos.x,vp->Pos.y + vp->Size.y - footer}); ImGui::SetNextWindowSize({vp->Size.x,footer});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12 * scale,4 * scale});
    ImGui::Begin("Status",nullptr,fixed);
    const auto counts = app::count(s.snapshot.root);
    ImGui::TextColored(s.error.empty() ? ImVec4{0.4f,0.73f,0.48f,1} : ImVec4{0.9f,0.65f,0.3f,1},ICON_MD_FIBER_MANUAL_RECORD);
    ImGui::SameLine();
    if (s.scanning) ImGui::TextUnformatted("Scanning USB connections...");
    else if (!s.error.empty()) ImGui::TextUnformatted("Scan failed - see device details");
    else if (!s.notice.empty()) ImGui::TextUnformatted(s.notice.c_str());
    else ImGui::Text("%d devices  /  %d hubs  /  %d available ports",counts.devices,counts.hubs,counts.empty_ports);
    if (!compact && !s.snapshot.captured_at.empty() && s.notice.empty()) {
        ImGui::SameLine(vp->Size.x - 310 * scale); ImGui::TextDisabled("Updated %s",s.snapshot.captured_at.c_str());
    }
    ImGui::End(); ImGui::PopStyleVar(2);
    about(s,fonts);
}
}
