#include "platform/usb.h"
#include "platform/files.h"
#include <iostream>
#include <set>
namespace {
bool validate(const app::Node& n, std::set<std::string>& ids) {
    if (n.id.empty() || !ids.insert(n.id).second) return false;
    for (const auto& child : n.children) if (!validate(child, ids)) return false;
    return true;
}
}
int main() {
    app::Snapshot snapshot; std::string error;
    if (!platform::enumerate_usb(snapshot, error)) { std::cerr << error << '\n'; return 1; }
    std::set<std::string> ids;
    if (!validate(snapshot.root, ids)) { std::cerr << "Duplicate or missing node IDs\n"; return 1; }
    const auto counts = app::count(snapshot.root);
    std::cout << "Live USB topology: " << counts.controllers << " controllers, " << counts.hubs << " hubs, " << counts.devices << " devices, " << counts.empty_ports << " empty ports\n";
    for (const auto& warning : snapshot.warnings) std::cout << "Warning: " << warning << '\n';
    std::filesystem::path base;
    if (!platform::executable_directory(base, error) || !platform::write_text(base / "test-results" / "live-topology.txt", app::full_report(snapshot), error)) {
        std::cerr << error << '\n'; return 1;
    }
    return 0;
}
