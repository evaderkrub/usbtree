#include "app/model.h"
#include <algorithm>
#include <map>

namespace app {
namespace {
std::string join(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) { if (!result.empty()) result += ", "; result += value; }
    return result;
}
void unique(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
void index_nodes(Node& node, std::map<std::string, Node*>& nodes) {
    node.serial_ports.clear(); node.volumes.clear();
    if (!node.instance_id.empty() && node.connected && (node.kind == Kind::Device || node.kind == Kind::Hub))
        nodes.emplace(node.instance_id, &node);
    for (auto& child : node.children) index_nodes(child, nodes);
}
}
void apply_device_access(Snapshot& snapshot, const std::vector<DeviceAccess>& mappings) {
    std::map<std::string, Node*> nodes;
    index_nodes(snapshot.root, nodes);
    for (const auto& mapping : mappings) {
        const auto found = nodes.find(mapping.instance_id);
        // A device may disappear between the topology scan and the access scan. Never attach its ports to a sibling or hub.
        if (found == nodes.end()) continue;
        auto& n = *found->second;
        n.serial_ports.insert(n.serial_ports.end(), mapping.serial_ports.begin(), mapping.serial_ports.end());
        for (const auto& volume : mapping.volumes) {
            auto existing = std::find_if(n.volumes.begin(), n.volumes.end(), [&](const Volume& v) { return v.id == volume.id; });
            if (existing == n.volumes.end()) n.volumes.push_back(volume);
            else {
                existing->mount_paths.insert(existing->mount_paths.end(), volume.mount_paths.begin(), volume.mount_paths.end());
                if (existing->label.empty()) existing->label = volume.label;
                if (existing->filesystem.empty()) existing->filesystem = volume.filesystem;
            }
        }
    }
    for (const auto& [id, node] : nodes) {
        (void)id;
        unique(node->serial_ports);
        std::sort(node->serial_ports.begin(), node->serial_ports.end(), [](const std::string& a, const std::string& b) {
            return a.size() == b.size() ? a < b : a.size() < b.size();
        });
        for (auto& volume : node->volumes) unique(volume.mount_paths);
        std::sort(node->volumes.begin(), node->volumes.end(), [](const Volume& a, const Volume& b) { return a.mount_paths < b.mount_paths; });
    }
}
std::string access_summary(const Node& node) {
    std::vector<std::string> drives;
    for (const auto& volume : node.volumes)
        for (const auto& path : volume.mount_paths)
            if (path.size() == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) drives.push_back(path.substr(0, 2));
    unique(drives);
    auto result = join(node.serial_ports);
    if (!drives.empty()) { if (!result.empty()) result += " | "; result += join(drives); }
    return result;
}
std::string display_name(const Node& node) {
    const auto summary = access_summary(node);
    return summary.empty() ? node.name : "[" + summary + "] " + node.name;
}
std::vector<Property> access_properties(const Node& node) {
    std::vector<Property> rows;
    if (!node.serial_ports.empty()) rows.push_back({"COM ports", join(node.serial_ports)});
    for (const auto& volume : node.volumes) {
        const auto location = volume.mount_paths.empty() ? "No mount point" : join(volume.mount_paths);
        rows.push_back({"Drive / mount paths", location});
        rows.push_back({"Volume label", volume.label.empty() ? "Not reported" : volume.label});
        rows.push_back({"Filesystem", volume.filesystem.empty() ? "Not reported (media may be unavailable)" : volume.filesystem});
        rows.push_back({"Volume ID", volume.id});
    }
    return rows;
}
}
