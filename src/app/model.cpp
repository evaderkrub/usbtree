#include "app/model.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace app {
namespace {
std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
void report_tree(const Node& node, std::ostringstream& out, int depth) {
    out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << node.name << " [" << node.status << "]\n";
    for (const Node& child : node.children) report_tree(child, out, depth + 1);
}
void report_details(const Node& node, std::ostringstream& out) {
    out << "\n" << node_report(node);
    for (const Node& child : node.children) report_details(child, out);
}
}
const char* kind_name(Kind kind) {
    switch (kind) {
    case Kind::Computer: return "Computer";
    case Kind::Controller: return "Host controller";
    case Kind::Hub: return "USB hub";
    case Kind::Device: return "USB device";
    case Kind::EmptyPort: return "Empty port";
    }
    return "Unknown";
}
std::string hex(std::uint32_t value, int width) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return out.str();
}
Counts count(const Node& root) {
    Counts c;
    c.controllers = root.kind == Kind::Controller;
    c.hubs = root.kind == Kind::Hub;
    c.devices = root.kind == Kind::Device;
    c.empty_ports = root.kind == Kind::EmptyPort;
    c.problems = root.problem_code != 0;
    for (const auto& child : root.children) {
        const auto a = count(child);
        c.controllers += a.controllers; c.hubs += a.hubs; c.devices += a.devices;
        c.empty_ports += a.empty_ports; c.problems += a.problems;
    }
    return c;
}
const Node* find(const Node& root, const std::string& id) {
    if (root.id == id) return &root;
    for (const auto& child : root.children) if (const auto* n = find(child, id)) return n;
    return nullptr;
}
bool matches(const Node& n, const std::string& query) {
    const auto haystack = lower(n.name + " " + n.manufacturer + " " + n.instance_id + " " + n.serial + " " + n.service + " " +
        hex(n.vendor_id) + ":" + hex(n.product_id) + " " + n.speed + " " + n.status);
    std::istringstream tokens(lower(query));
    std::string token;
    while (tokens >> token) if (haystack.find(token) == std::string::npos) return false;
    return true;
}
bool visible(const Node& n, const std::string& query, bool show_empty) {
    if (n.kind == Kind::EmptyPort && !show_empty) return false;
    if (matches(n, query)) return true;
    for (const auto& child : n.children) if (visible(child, query, show_empty)) return true;
    return false;
}
void accept_snapshot(State& s, Snapshot snapshot) {
    s.snapshot = std::move(snapshot);
    if (!find(s.snapshot.root, s.selected_id)) s.selected_id = s.snapshot.root.id;
    s.report = full_report(s.snapshot);
    s.scanning = false;
    ++s.generation;
}
std::string hex_dump(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream out;
    for (std::size_t i = 0; i < bytes.size(); i += 16) {
        out << hex(static_cast<std::uint32_t>(i)) << "  ";
        for (std::size_t j = 0; j < 16; ++j) out << (i + j < bytes.size() ? hex(bytes[i + j], 2) : "  ") << ' ';
        out << " |";
        for (std::size_t j = i; j < std::min(i + 16, bytes.size()); ++j) out << (bytes[j] >= 32 && bytes[j] < 127 ? static_cast<char>(bytes[j]) : '.');
        out << "|\n";
    }
    return out.str();
}
bool decode_configuration(const std::vector<std::uint8_t>& b, std::vector<Property>& rows, std::string& error) {
    rows.clear(); error.clear();
    if (b.empty()) return true;
    if (b.size() < 9 || b[0] < 9 || b[1] != 2) { error = "Invalid configuration header"; return false; }
    const std::size_t total = b[2] | (static_cast<std::size_t>(b[3]) << 8);
    if (total < 9 || total > b.size()) { error = "Truncated configuration descriptor"; return false; }
    for (std::size_t offset = 0; offset < total;) {
        if (offset + 2 > total || b[offset] < 2 || offset + b[offset] > total) {
            error = "Invalid descriptor length at byte " + std::to_string(offset); return false;
        }
        const auto* d = b.data() + offset;
        const int type = d[1];
        if ((type == 2 && d[0] < 9) || (type == 4 && d[0] < 9) || (type == 5 && d[0] < 7)) {
            error = "Descriptor is shorter than its required fields"; return false;
        }
        if (type == 2) {
            rows.push_back({"Configuration", std::to_string(d[5]) + " / " + std::to_string(d[4]) + " interfaces"});
            rows.push_back({"Power attributes", (d[7] & 0x40) ? "Self powered" : "Bus powered"});
            rows.push_back({"bMaxPower", std::to_string(d[8]) + " units (2 mA at USB 2; 8 mA at SuperSpeed)"});
        } else if (type == 4) {
            rows.push_back({"Interface " + std::to_string(d[2]) + " / alternate " + std::to_string(d[3]),
                "Class 0x" + hex(d[5], 2) + " / subclass 0x" + hex(d[6], 2) + " / " + std::to_string(d[4]) + " endpoints"});
        } else if (type == 5) {
            constexpr const char* transfers[] = {"Control", "Isochronous", "Bulk", "Interrupt"};
            rows.push_back({"Endpoint 0x" + hex(d[2], 2), std::string((d[2] & 0x80) ? "IN / " : "OUT / ") + transfers[d[3] & 3] +
                " / max packet " + std::to_string(d[4] | (d[5] << 8)) + " / interval " + std::to_string(d[6])});
        } else rows.push_back({"Descriptor 0x" + hex(d[1], 2), std::to_string(d[0]) + " bytes at offset " + std::to_string(offset)});
        offset += d[0];
    }
    return true;
}
std::string node_report(const Node& n) {
    std::ostringstream o;
    o << n.name << "\n" << std::string(64, '-') << "\nType: " << kind_name(n.kind) << "\nStatus: " << n.status
      << "\nInstance ID: " << n.instance_id << "\nManufacturer: " << n.manufacturer << "\nService: " << n.service
      << "\nLocation: " << n.location << "\nPort: " << n.port << "\nSpeed: " << n.speed << "\nVID:PID: "
      << hex(n.vendor_id) << ':' << hex(n.product_id) << "\nSerial: " << n.serial << '\n';
    for (const auto& p : n.properties) o << p.name << ": " << p.value << '\n';
    if (!n.device_descriptor.empty()) o << "\nDevice descriptor\n" << hex_dump(n.device_descriptor);
    std::vector<Property> rows; std::string error;
    decode_configuration(n.configuration, rows, error);
    for (const auto& p : rows) o << p.name << ": " << p.value << '\n';
    if (!error.empty()) o << "Descriptor warning: " << error << '\n';
    if (!n.configuration.empty()) o << "\nConfiguration bytes\n" << hex_dump(n.configuration);
    return o.str();
}
std::string full_report(const Snapshot& s) {
    std::ostringstream o;
    o << "USB TREE / topology report\nCaptured: " << s.captured_at << "\n\n";
    report_tree(s.root, o, 0);
    for (const auto& warning : s.warnings) o << "Warning: " << warning << '\n';
    report_details(s.root, o);
    return o.str();
}
float clamp_scale(float value) { return std::isfinite(value) ? std::clamp(value, 0.75f, 2.0f) : 1.0f; }
Snapshot demo_snapshot() {
    Snapshot s; s.captured_at = "Test fixture";
    s.root.id = "computer"; s.root.name = "Test workstation"; s.root.kind = Kind::Computer;
    Node c; c.id = "controller"; c.name = "USB xHCI host controller"; c.kind = Kind::Controller;
    Node h; h.id = "hub"; h.name = "USB Root Hub (USB 3.0)"; h.kind = Kind::Hub; h.port_count = 3;
    Node d; d.id = "keyboard"; d.name = "Studio Keyboard"; d.manufacturer = "Test Devices";
    d.vendor_id = 0x1234; d.product_id = 0xabcd; d.port = 1; d.speed = "Full-Speed (12 Mbit/s)";
    d.serial = "TEST-001"; d.instance_id = "USB\\VID_1234&PID_ABCD\\TEST-001"; d.service = "HidUsb";
    d.device_descriptor = {18,1,0,2,0,0,0,64,0x34,0x12,0xcd,0xab,0,1,1,2,3,1};
    d.configuration = {9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,7,5,0x81,3,8,0,10};
    h.children.push_back(d);
    Node empty; empty.id = "empty"; empty.name = "Port 2 - available"; empty.kind = Kind::EmptyPort; empty.port = 2; empty.connected = false; empty.status = "No device connected";
    h.children.push_back(empty);
    Node disk; disk.id = "disk"; disk.name = "Portable SSD"; disk.port = 3; disk.speed = "SuperSpeed (5 Gbit/s)";
    disk.vendor_id = 0x0781; disk.product_id = 0x5588; disk.service = "UASPStor";
    h.children.push_back(disk); c.children.push_back(h); s.root.children.push_back(c);
    return s;
}
}
