#include "platform/usb.h"
#include "platform/files.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <algorithm>
#include <stdexcept>

namespace platform {
namespace {
// Registry inspection never opens, claims or resets a USB device.
struct Object {
    io_object_t value = IO_OBJECT_NULL;
    ~Object() { if (value) IOObjectRelease(value); }
    Object() = default;
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
};
struct Properties {
    CFMutableDictionaryRef value = nullptr;
    ~Properties() { if (value) CFRelease(value); }
    CFTypeRef get(CFStringRef key) const { return value ? CFDictionaryGetValue(value, key) : nullptr; }
    std::int64_t number(CFStringRef key, std::int64_t fallback = 0) const {
        auto v = get(key); std::int64_t n = fallback;
        if (v && CFGetTypeID(v) == CFNumberGetTypeID())
            CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberSInt64Type, &n);
        return n;
    }
    std::string string(CFStringRef key) const {
        auto v = get(key);
        if (!v || CFGetTypeID(v) != CFStringGetTypeID()) return {};
        auto s = static_cast<CFStringRef>(v);
        std::string text(static_cast<std::size_t>(CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8)) + 1, '\0');
        if (!CFStringGetCString(s, text.data(), static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8)) return {};
        text.resize(text.find('\0')); return text;
    }
};
void children(io_registry_entry_t entry, app::Node& parent, unsigned depth) {
    if (depth > 32) throw std::runtime_error("USB registry exceeds the supported tree depth");
    Object iterator;
    if (IORegistryEntryGetChildIterator(entry, "IOUSB", &iterator.value) != KERN_SUCCESS)
        throw std::runtime_error("Cannot read USB registry children; refresh to retry");
    while (true) {
        Object child; child.value = IOIteratorNext(iterator.value);
        if (!child.value) break;
        Properties p;
        if (IORegistryEntryCreateCFProperties(child.value, &p.value, kCFAllocatorDefault, 0) != KERN_SUCCESS)
            throw std::runtime_error("USB registry changed during scanning; refresh to retry");
        app::Node node;
        io_name_t name{}; io_string_t path{}; io_name_t class_name{};
        std::uint64_t registry_id = 0;
        if (IORegistryEntryGetRegistryEntryID(child.value, &registry_id) != KERN_SUCCESS)
            throw std::runtime_error("Cannot identify USB registry entry");
        IORegistryEntryGetName(child.value, name);
        IOObjectGetClass(child.value, class_name);
        // Some Apple Silicon entries cannot produce an IOUSB-plane path.
        // Use the service path for stable selection, and never accept a partial failed path.
        const bool has_path = IORegistryEntryGetPath(child.value, kIOServicePlane, path) == KERN_SUCCESS;
        node.id = has_path ? path : "registry:" + std::to_string(registry_id);
        node.instance_id = node.id;
        node.name = p.string(CFSTR("USB Product Name"));
        if (node.name.empty()) node.name = name;
        node.kind = parent.kind == app::Kind::Computer ? app::Kind::Controller :
            (p.number(CFSTR("bDeviceClass")) == 9 ? app::Kind::Hub : app::Kind::Device);
        node.vendor_id = static_cast<std::uint16_t>(p.number(CFSTR("idVendor")));
        node.product_id = static_cast<std::uint16_t>(p.number(CFSTR("idProduct")));
        node.usb_version = static_cast<std::uint16_t>(p.number(CFSTR("bcdUSB")));
        node.manufacturer = p.string(CFSTR("USB Vendor Name"));
        node.serial = p.string(CFSTR("USB Serial Number"));
        node.service = class_name;
        if (p.get(CFSTR("locationID"))) node.location = "0x" + app::hex(static_cast<std::uint32_t>(p.number(CFSTR("locationID"))), 8);
        node.port = static_cast<unsigned>(p.number(CFSTR("port")));
        node.port_count = static_cast<unsigned>(std::clamp<std::int64_t>(p.number(CFSTR("bNumPorts")), 0, 255));
        // Device Speed is the legacy USB.h enum, not the xHCI speed encoding.
        switch (p.number(CFSTR("Device Speed"), -1)) {
        case 0: node.speed = "Low-speed (1.5 Mbit/s)"; break;
        case 1: node.speed = "Full-speed (12 Mbit/s)"; break;
        case 2: node.speed = "High-speed (480 Mbit/s)"; break;
        case 3: node.speed = "SuperSpeed (5 Gbit/s)"; break;
        case 4: node.speed = "SuperSpeedPlus (10 Gbit/s)"; break;
        case 5: node.speed = "SuperSpeedPlus (20 Gbit/s)"; break;
        default: break;
        }
        node.properties.push_back({"Registry class", class_name});
        node.properties.push_back({"Registry entry ID", std::to_string(registry_id)});
        const auto driver = p.string(CFSTR("CFBundleIdentifier"));
        if (!driver.empty()) node.properties.push_back({"Driver bundle", driver});
        if (node.kind != app::Kind::Controller)
            node.properties.push_back({"Descriptors", "Raw descriptors are not collected by the macOS registry backend"});
        children(child.value, node, depth + 1);
        parent.children.push_back(std::move(node));
    }
    if (!IOIteratorIsValid(iterator.value)) throw std::runtime_error("USB registry changed during scanning; refresh to retry");
}
}
bool enumerate_usb(app::Snapshot& snapshot, std::string& error) noexcept {
    try {
        app::Snapshot result;
        result.root.id = "computer"; result.root.name = "This Mac"; result.root.kind = app::Kind::Computer;
        result.captured_at = timestamp();
        Object root; root.value = IORegistryGetRootEntry(kIOMainPortDefault);
        if (!root.value) throw std::runtime_error("Cannot open the macOS I/O Registry");
        children(root.value, result.root, 0);
        snapshot = std::move(result); error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
}
