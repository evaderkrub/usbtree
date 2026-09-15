#include "ui/views.h"
#include "ui/icons.h"
#include "imgui.h"
#include <algorithm>

namespace ui {
namespace {
const char* icon(const app::Node& n) {
    if (n.problem_code) return ICON_MD_WARNING;
    switch (n.kind) {
    case app::Kind::Computer: return ICON_MD_COMPUTER;
    case app::Kind::Controller: return ICON_MD_MEMORY;
    case app::Kind::Hub: return ICON_MD_ACCOUNT_TREE;
    case app::Kind::EmptyPort: return ICON_MD_RADIO_BUTTON_UNCHECKED;
    case app::Kind::Device:
        if (n.service == "HidUsb") return ICON_MD_KEYBOARD;
        if (n.service == "USBSTOR" || n.service == "UASPStor") return ICON_MD_STORAGE;
        if (n.service == "usbvideo") return ICON_MD_VIDEOCAM;
        if (n.service == "BTHUSB") return ICON_MD_BLUETOOTH;
        return ICON_MD_USB;
    }
    return ICON_MD_USB;
}
void properties(const char* id, const std::vector<app::Property>& rows) {
    if (ImGui::BeginTable(id,2,ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,160 * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
        for (const auto& p : rows) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextDisabled("%s",p.name.c_str());
            ImGui::TableNextColumn(); ImGui::TextWrapped("%s",p.value.empty() ? "Not reported" : p.value.c_str());
        }
        ImGui::EndTable();
    }
}
void tree_node(app::State& s, const app::Node& n) {
    if (!app::visible(n,s.search.data(),s.show_empty)) return;
    auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (n.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (s.selected_id == n.id) flags |= ImGuiTreeNodeFlags_Selected;
    if (s.expand_tree || s.search[0]) ImGui::SetNextItemOpen(s.expand_tree >= 0 || s.search[0] != 0,ImGuiCond_Always);
    else ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    if (n.kind == app::Kind::EmptyPort) ImGui::PushStyleColor(ImGuiCol_Text,{0.43f,0.46f,0.51f,1});
    else if (n.problem_code) ImGui::PushStyleColor(ImGuiCol_Text,{0.9f,0.68f,0.34f,1});
    const std::string label = std::string(icon(n)) + "  " + (n.port && n.kind != app::Kind::EmptyPort ? std::to_string(n.port) + "   " : "") + n.name + "###" + n.id;
    const bool open = ImGui::TreeNodeEx(label.c_str(),flags);
    if (n.kind == app::Kind::EmptyPort || n.problem_code) ImGui::PopStyleColor();
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) s.selected_id = n.id;
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip(); ImGui::TextUnformatted(n.name.c_str()); ImGui::TextDisabled("%s",n.status.c_str());
        if (!n.speed.empty()) ImGui::TextUnformatted(n.speed.c_str());
        if (n.vendor_id) ImGui::Text("VID %s  /  PID %s",app::hex(n.vendor_id).c_str(),app::hex(n.product_id).c_str());
        ImGui::EndTooltip();
    }
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Copy device report")) { s.selected_id = n.id; s.copy_requested = true; }
        ImGui::EndPopup();
    }
    if (open && !n.children.empty()) { for (const auto& c : n.children) tree_node(s,c); ImGui::TreePop(); }
}
void metric(const char* title, const std::string& value, const Fonts& fonts) {
    ImGui::TableNextColumn(); ImGui::TextDisabled("%s",title);
    ImGui::PushFont(fonts.bold,20); ImGui::TextWrapped("%s",value.c_str()); ImGui::PopFont();
}
void inventory(app::State& s, const app::Node& n) {
    if (n.kind == app::Kind::Device) {
        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::PushID(n.id.c_str());
        if (ImGui::Selectable(n.name.c_str(),s.selected_id == n.id,ImGuiSelectableFlags_SpanAllColumns)) s.selected_id = n.id;
        ImGui::PopID(); ImGui::TableNextColumn(); ImGui::TextWrapped("%s",n.speed.c_str());
        ImGui::TableNextColumn(); ImGui::Text("%s:%s",app::hex(n.vendor_id).c_str(),app::hex(n.product_id).c_str());
    }
    for (const auto& child : n.children) inventory(s,child);
}
void overview(app::State& s, const app::Node& n, const Fonts& fonts) {
    if (n.kind == app::Kind::Computer || n.kind == app::Kind::Controller || n.kind == app::Kind::Hub) {
        const auto c = app::count(n);
        if (ImGui::BeginTable("Summary",3,ImGuiTableFlags_SizingStretchSame)) {
            metric("DEVICES",std::to_string(c.devices),fonts); metric("HUBS",std::to_string(c.hubs),fonts);
            metric("AVAILABLE PORTS",std::to_string(c.empty_ports),fonts); ImGui::EndTable();
        }
        ImGui::Spacing(); ImGui::SeparatorText("Connected devices");
        if (!c.devices) ImGui::TextDisabled("No connected devices in this branch.");
        else if (ImGui::BeginTable("Inventory",3,ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("DEVICE",ImGuiTableColumnFlags_WidthStretch,1.4f);
            ImGui::TableSetupColumn("CONNECTION",ImGuiTableColumnFlags_WidthStretch,1);
            ImGui::TableSetupColumn("VID : PID",ImGuiTableColumnFlags_WidthFixed,105 * ImGui::GetStyle().FontScaleMain);
            ImGui::TableHeadersRow(); inventory(s,n); ImGui::EndTable();
        }
        ImGui::Spacing();
    } else if (n.kind == app::Kind::EmptyPort) {
        ImGui::TextWrapped("This port is available. Connect a USB device to see its identity, connection speed and descriptors here."); ImGui::Spacing();
    } else {
        if (ImGui::BeginTable("Identity",3,ImGuiTableFlags_SizingStretchSame)) {
            metric("VENDOR ID","0x" + app::hex(n.vendor_id),fonts); metric("PRODUCT ID","0x" + app::hex(n.product_id),fonts);
            metric("USB VERSION",n.usb_version ? app::hex(n.usb_version >> 8,1) + "." + app::hex(n.usb_version & 255,2) : "--",fonts);
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }
    ImGui::SeparatorText("Connection & identity");
    std::vector<app::Property> rows = {{"Status",n.status},{"Connection speed",n.speed},{"Manufacturer",n.manufacturer},
        {"Serial number",n.serial},{"Instance ID",n.instance_id},{"Location",n.location},{"Driver service",n.service}};
    if (n.port) rows.insert(rows.begin()+1,{"Upstream port",std::to_string(n.port)});
    if (n.kind == app::Kind::Computer) rows = {{"Computer",n.name},{"Host controllers",std::to_string(app::count(n).controllers)},{"Captured",s.snapshot.captured_at}};
    if (n.kind == app::Kind::EmptyPort) {
        rows = {{"Status",n.status},{"Hub port",std::to_string(n.port)}};
        rows.insert(rows.end(),n.properties.begin(),n.properties.end());
    }
    properties("IdentityProperties",rows);
}
}
void draw_tree(app::State& s, const Fonts&) {
    ImGui::Begin("Connections");
    const float scale = ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextItemWidth(std::max(40.0f,ImGui::GetContentRegionAvail().x - 44 * scale));
    if (s.focus_search) { ImGui::SetKeyboardFocusHere(); s.focus_search = false; }
    ImGui::InputTextWithHint("###Search",ICON_MD_SEARCH "  Search name, VID:PID, serial...",s.search.data(),s.search.size());
    ImGui::SameLine(); if (ImGui::Button(ICON_MD_CLOSE "###ClearSearch")) s.search[0] = '\0';
    ImGui::Checkbox("Empty ports",&s.show_empty); ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_UNFOLD_MORE "###Expand")) s.expand_tree = 1;
    ImGui::SameLine(); if (ImGui::SmallButton(ICON_MD_UNFOLD_LESS "###Collapse")) s.expand_tree = -1;
    ImGui::Separator();
    ImGui::BeginChild("Tree",{0,-ImGui::GetFrameHeightWithSpacing()},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4 * scale,4 * scale});
    if (!app::visible(s.snapshot.root,s.search.data(),s.show_empty)) {
        ImGui::Spacing(); ImGui::TextUnformatted("No matching devices"); ImGui::TextWrapped("Try a device name, vendor ID or serial number.");
    } else tree_node(s,s.snapshot.root);
    ImGui::PopStyleVar(); ImGui::EndChild(); s.expand_tree = 0;
    ImGui::Checkbox("Watch device changes",&s.auto_refresh);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh after Windows reports a device connection or removal");
    ImGui::End();
}
void draw_details(app::State& s, const Fonts& fonts) {
    ImGui::Begin("Device details");
    if (!s.error.empty()) {
        ImGui::TextColored({0.95f,0.58f,0.36f,1},ICON_MD_WARNING "  Scan could not complete");
        ImGui::TextWrapped("%s",s.error.c_str()); ImGui::Separator();
    }
    if (!s.snapshot.warnings.empty() && ImGui::CollapsingHeader("Scan warnings"))
        for (const auto& warning : s.snapshot.warnings) ImGui::TextWrapped("%s",warning.c_str());
    const auto* node = app::find(s.snapshot.root,s.selected_id); if (!node) node = &s.snapshot.root;
    const auto& n = *node;
    ImGui::TextDisabled("%s",app::kind_name(n.kind));
    ImGui::PushFont(fonts.bold,25); ImGui::TextWrapped("%s  %s",icon(n),n.name.c_str()); ImGui::PopFont();
    ImGui::TextColored(n.problem_code ? ImVec4{0.9f,0.68f,0.34f,1} : ImVec4{0.4f,0.73f,0.48f,1},"%s",n.status.c_str());
    if (!n.speed.empty()) { ImGui::SameLine(); ImGui::TextDisabled(" /  %s",n.speed.c_str()); }
    ImGui::Spacing(); if (ImGui::Button(ICON_MD_CONTENT_COPY "  Copy details###CopyDetails")) s.copy_requested = true;
    ImGui::Spacing();
    if (ImGui::BeginTabBar("DetailTabs")) {
        if (ImGui::BeginTabItem("Overview")) { overview(s,n,fonts); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Properties")) { properties("Properties",n.properties); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Descriptors")) {
            if (n.device_descriptor.size() < 18) ImGui::TextWrapped("No device descriptor is available for this node. Select a connected USB device.");
            else {
                ImGui::SeparatorText("Device descriptor"); const auto& d = n.device_descriptor;
                properties("DeviceDescriptor",{{"bLength",std::to_string(d[0])},{"bcdUSB","0x" + app::hex(n.usb_version)},
                    {"idVendor","0x" + app::hex(n.vendor_id)},{"idProduct","0x" + app::hex(n.product_id)},
                    {"bDeviceClass","0x" + app::hex(d[4],2)},{"bDeviceSubClass","0x" + app::hex(d[5],2)},
                    {"bDeviceProtocol","0x" + app::hex(d[6],2)},{"bMaxPacketSize0",std::to_string(d[7])},{"bNumConfigurations",std::to_string(d[17])}});
                ImGui::SeparatorText("Configuration, interfaces & endpoints");
                std::vector<app::Property> rows; std::string error;
                app::decode_configuration(n.configuration,rows,error); properties("Configuration",rows);
                if (!error.empty()) ImGui::TextWrapped("%s",error.c_str());
                if (n.configuration.empty()) ImGui::TextDisabled("Configuration descriptor unavailable.");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Raw bytes")) {
            ImGui::PushFont(fonts.mono,15);
            ImGui::BeginChild("Hex",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
            if (n.device_descriptor.empty()) ImGui::TextUnformatted("No descriptor bytes for this node.");
            else { ImGui::TextUnformatted("DEVICE"); ImGui::TextUnformatted(app::hex_dump(n.device_descriptor).c_str()); }
            if (!n.configuration.empty()) { ImGui::Separator(); ImGui::TextUnformatted("CONFIGURATION"); ImGui::TextUnformatted(app::hex_dump(n.configuration).c_str()); }
            ImGui::EndChild(); ImGui::PopFont(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
}
