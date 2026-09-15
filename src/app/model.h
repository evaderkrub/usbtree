#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace app {
enum class Kind { Computer, Controller, Hub, Device, EmptyPort };
struct Property { std::string name; std::string value; };
struct Node {
    std::string id, name, manufacturer, instance_id, service, location;
    Kind kind = Kind::Device;
    std::string status = "Connected", speed, serial;
    std::uint16_t vendor_id = 0, product_id = 0, usb_version = 0;
    unsigned port = 0, port_count = 0, problem_code = 0;
    bool connected = true;
    std::vector<std::uint8_t> device_descriptor, configuration;
    std::vector<Property> properties;
    std::vector<Node> children;
};
struct Snapshot {
    Node root;
    std::string captured_at;
    std::vector<std::string> warnings;
};
struct Counts { int controllers = 0, hubs = 0, devices = 0, empty_ports = 0, problems = 0; };
struct State {
    Snapshot snapshot;
    std::string selected_id, error, notice;
    std::array<char, 256> search{};
    bool show_empty = true, auto_refresh = true, scanning = false;
    bool refresh_requested = true, export_requested = false, copy_requested = false;
    bool about_open = false, reset_layout = false, focus_search = false;
    int expand_tree = 0;
    float scale = 1.0f;
    unsigned generation = 0;
    std::string report;
};
const char* kind_name(Kind kind);
std::string hex(std::uint32_t value, int width = 4);
Counts count(const Node& root);
const Node* find(const Node& root, const std::string& id);
bool matches(const Node& node, const std::string& query);
bool visible(const Node& node, const std::string& query, bool show_empty);
void accept_snapshot(State& state, Snapshot snapshot);
std::string node_report(const Node& node);
std::string full_report(const Snapshot& snapshot);
std::string hex_dump(const std::vector<std::uint8_t>& bytes);
bool decode_configuration(const std::vector<std::uint8_t>& bytes, std::vector<Property>& rows, std::string& error);
float clamp_scale(float value);
Snapshot demo_snapshot();
}
