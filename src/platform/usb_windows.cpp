#include "platform/usb.h"
#include "platform/files.h"
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <usbioctl.h>
#include <usbiodef.h>
// The SDK serial header repeats legacy constants from winioctl.h with different casts.
#pragma warning(push)
#pragma warning(disable:4005)
#include <ntddser.h>
#pragma warning(pop)
#include <ntddstor.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <stdexcept>

namespace platform {
namespace {
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return value != INVALID_HANDLE_VALUE && value; }
};
struct DeviceSet {
    HDEVINFO value;
    explicit DeviceSet(HDEVINFO h) : value(h) {}
    ~DeviceSet() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr); return s;
}
std::wstring folded(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); }); return s;
}
std::string win_error(DWORD code) {
    wchar_t buffer[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buffer, 512, nullptr);
    auto s = utf8(buffer);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s + " (" + std::to_string(code) + ")";
}
bool ioctl(HANDLE h, DWORD code, void* buffer, DWORD bytes, DWORD* actual = nullptr, bool output_only = false) {
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return false;
    OVERLAPPED op{}; op.hEvent = event.value;
    DWORD returned = 0;
    bool ok = DeviceIoControl(h, code, output_only ? nullptr : buffer, output_only ? 0 : bytes, buffer, bytes, &returned, &op) != FALSE;
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        // Query cancellation bounds delays from ordinary slow devices; enumeration never runs on the UI thread.
        if (WaitForSingleObject(event.value, 1500) != WAIT_OBJECT_0) {
            CancelIoEx(h, &op); GetOverlappedResult(h, &op, &returned, TRUE);
            SetLastError(ERROR_TIMEOUT); return false;
        }
        ok = GetOverlappedResult(h, &op, &returned, FALSE) != FALSE;
    }
    if (actual) *actual = returned;
    return ok;
}
template<class T> bool query(HANDLE h, DWORD code, T& value) { return ioctl(h, code, &value, sizeof(T)); }
HANDLE open_device(std::wstring path) {
    if (!path.starts_with(L"\\\\")) path = L"\\\\.\\" + path;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    return h;
}
std::wstring registry_property(HDEVINFO set, SP_DEVINFO_DATA& dev, DWORD key) {
    DWORD type = 0, size = 0;
    SetupDiGetDeviceRegistryPropertyW(set, &dev, key, &type, nullptr, 0, &size);
    if (size == 0 || size > 65536) return {};
    std::vector<wchar_t> data(size / sizeof(wchar_t) + 2, 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, key, &type, reinterpret_cast<BYTE*>(data.data()), size, nullptr)) return {};
    if (type == REG_MULTI_SZ) {
        for (std::size_t i = 0; i + 1 < size/sizeof(wchar_t); ++i) if (data[i] == 0 && data[i+1] != 0) data[i] = L';';
    }
    return data.data();
}
std::string device_property(HDEVINFO set, SP_DEVINFO_DATA& dev, const DEVPROPKEY& key) {
    DEVPROPTYPE type = 0; DWORD size = 0;
    SetupDiGetDevicePropertyW(set, &dev, &key, &type, nullptr, 0, &size, 0);
    if (size == 0 || size > 65536) return {};
    std::vector<wchar_t> data(size/sizeof(wchar_t) + 2, 0);
    if (!SetupDiGetDevicePropertyW(set, &dev, &key, &type, reinterpret_cast<BYTE*>(data.data()), size, nullptr, 0)) return {};
    if (type != DEVPROP_TYPE_STRING && type != DEVPROP_TYPE_STRING_LIST) return {};
    if (type == DEVPROP_TYPE_STRING_LIST) {
        for (std::size_t i = 0; i + 1 < size/sizeof(wchar_t); ++i) if (data[i] == 0 && data[i+1] != 0) data[i] = L';';
    }
    return utf8(data.data());
}
struct Metadata { app::Node node; DEVINST devinst = 0; };
Metadata metadata(HDEVINFO set, SP_DEVINFO_DATA& dev) {
    Metadata info; auto& n = info.node; info.devinst = dev.DevInst;
    wchar_t id[MAX_DEVICE_ID_LEN]{}; CM_Get_Device_IDW(dev.DevInst, id, MAX_DEVICE_ID_LEN, 0);
    n.instance_id = utf8(id); n.id = n.instance_id;
    n.name = utf8(registry_property(set, dev, SPDRP_FRIENDLYNAME));
    if (n.name.empty()) n.name = device_property(set, dev, DEVPKEY_Device_BusReportedDeviceDesc);
    if (n.name.empty()) n.name = utf8(registry_property(set, dev, SPDRP_DEVICEDESC));
    n.manufacturer = utf8(registry_property(set, dev, SPDRP_MFG));
    n.service = utf8(registry_property(set, dev, SPDRP_SERVICE));
    n.location = utf8(registry_property(set, dev, SPDRP_LOCATION_INFORMATION));
    n.properties.push_back({"Device class", utf8(registry_property(set, dev, SPDRP_CLASS))});
    n.properties.push_back({"Hardware IDs", utf8(registry_property(set, dev, SPDRP_HARDWAREID))});
    n.properties.push_back({"Driver key", utf8(registry_property(set, dev, SPDRP_DRIVER))});
    n.properties.push_back({"Driver provider", device_property(set, dev, DEVPKEY_Device_DriverProvider)});
    n.properties.push_back({"Driver version", device_property(set, dev, DEVPKEY_Device_DriverVersion)});
    n.properties.push_back({"Location paths", device_property(set, dev, DEVPKEY_Device_LocationPaths)});
    ULONG status = 0, problem = 0;
    if (CM_Get_DevNode_Status(&status, &problem, dev.DevInst, 0) == CR_SUCCESS) {
        n.problem_code = problem;
        n.properties.push_back({"PnP status flags", "0x" + app::hex(status, 8)});
        if (problem) n.properties.push_back({"Windows problem code", std::to_string(problem)});
    }
    return info;
}
using DeviceMap = std::map<std::wstring, Metadata>;
DeviceMap collect_metadata() {
    DeviceMap map;
    DeviceSet set(SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (set.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Device metadata: " + win_error(GetLastError()));
    for (DWORD i = 0;; ++i) {
        SP_DEVINFO_DATA dev{}; dev.cbSize = sizeof(dev);
        if (!SetupDiEnumDeviceInfo(set.value, i, &dev)) break;
        auto driver = registry_property(set.value, dev, SPDRP_DRIVER);
        if (!driver.empty()) map.emplace(folded(driver), metadata(set.value, dev));
    }
    return map;
}
struct ThreadErrorMode {
    DWORD previous = 0;
    bool changed = SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous) != FALSE;
    ~ThreadErrorMode() { if (changed) SetThreadErrorMode(previous, nullptr); }
};
std::string usb_ancestor(DEVINST dev) {
    for (int depth = 0; depth < 64; ++depth) {
        wchar_t text[MAX_DEVICE_ID_LEN]{};
        if (CM_Get_Device_IDW(dev, text, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) return {};
        const auto id = folded(text);
        // Interface children (MI_xx), serial bus children and UASP disks belong to their physical USB parent.
        if (id.starts_with(L"usb\\vid_") && id.substr(0, id.find(L'\\', 4)).find(L"&mi_") == std::wstring::npos)
            return utf8(text);
        DEVINST parent = 0;
        if (CM_Get_Parent(&parent, dev, 0) != CR_SUCCESS || parent == dev) return {};
        dev = parent;
    }
    return {};
}
template<class F> void device_interfaces(const GUID& guid, std::vector<std::string>& warnings, F&& visit) {
    DeviceSet set(SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (set.value == INVALID_HANDLE_VALUE) { warnings.push_back("Device access enumeration: " + win_error(GetLastError())); return; }
    for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set.value, nullptr, &guid, i, &iface)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) warnings.push_back("Device access enumeration: " + win_error(GetLastError()));
            break;
        }
        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(set.value, &iface, nullptr, 0, &bytes, nullptr);
        if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || bytes > 65536) continue;
        std::vector<BYTE> buffer(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data()); detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA dev{}; dev.cbSize = sizeof(dev);
        if (SetupDiGetDeviceInterfaceDetailW(set.value, &iface, detail, bytes, nullptr, &dev))
            visit(set.value, iface, dev, detail->DevicePath);
    }
}
std::string serial_port(HDEVINFO set, SP_DEVICE_INTERFACE_DATA& iface, SP_DEVINFO_DATA& dev) {
    wchar_t name[256]{};
    DEVPROPTYPE type = 0;
    if (!SetupDiGetDeviceInterfacePropertyW(set, &iface, &DEVPKEY_DeviceInterface_Serial_PortName, &type,
        reinterpret_cast<BYTE*>(name), sizeof(name) - sizeof(wchar_t), nullptr, 0) || type != DEVPROP_TYPE_STRING) {
        // Older serial drivers expose PortName only on the device's hardware registry key.
        HKEY key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
        if (key == INVALID_HANDLE_VALUE) return {};
        DWORD bytes = sizeof(name) - sizeof(wchar_t), registry_type = 0;
        const auto result = RegQueryValueExW(key, L"PortName", nullptr, &registry_type, reinterpret_cast<BYTE*>(name), &bytes);
        RegCloseKey(key);
        if (result != ERROR_SUCCESS || registry_type != REG_SZ) return {};
    }
    auto normalized = folded(name);
    if (!normalized.starts_with(L"com") || normalized.size() <= 3 ||
        !std::all_of(normalized.begin() + 3, normalized.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return {};
    return "COM" + utf8(normalized.substr(3));
}
HANDLE open_storage(const std::wstring& path) {
    // Zero desired access queries identity without locking the volume or requiring elevation.
    return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
}
using StorageKey = std::pair<DWORD, DWORD>;
using StorageMap = std::map<StorageKey, std::string>;
std::set<std::string> volume_owners(HANDLE handle, const StorageMap& disks) {
    std::set<std::string> owners;
    std::vector<BYTE> buffer(offsetof(VOLUME_DISK_EXTENTS, Extents) + 4096 * sizeof(DISK_EXTENT), 0);
    DWORD bytes = 0;
    if (ioctl(handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, true)) {
        const auto* extents = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buffer.data());
        if (bytes >= offsetof(VOLUME_DISK_EXTENTS, Extents) &&
            extents->NumberOfDiskExtents <= (bytes - offsetof(VOLUME_DISK_EXTENTS, Extents)) / sizeof(DISK_EXTENT)) {
            for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i) {
                const auto found = disks.find({FILE_DEVICE_DISK, extents->Extents[i].DiskNumber});
                if (found != disks.end()) owners.insert(found->second);
            }
        }
    }
    // Optical drives and removable drives without media may have an assigned letter but no volume extents.
    if (owners.empty()) {
        STORAGE_DEVICE_NUMBER number{};
        if (ioctl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, &number, sizeof(number), nullptr, true)) {
            if (const auto found = disks.find({number.DeviceType, number.DeviceNumber}); found != disks.end()) owners.insert(found->second);
        }
    }
    return owners;
}
std::vector<std::string> mount_paths(const std::wstring& volume) {
    DWORD needed = 0;
    std::vector<wchar_t> paths(512, 0);
    if (!GetVolumePathNamesForVolumeNameW(volume.c_str(), paths.data(), static_cast<DWORD>(paths.size()), &needed)) {
        if (GetLastError() != ERROR_MORE_DATA || needed > 1024 * 1024) return {};
        paths.assign(static_cast<std::size_t>(needed) + 1, 0);
        if (!GetVolumePathNamesForVolumeNameW(volume.c_str(), paths.data(), static_cast<DWORD>(paths.size()), &needed)) return {};
    }
    std::vector<std::string> result;
    for (const wchar_t* p = paths.data(); *p; p += wcslen(p) + 1) result.push_back(utf8(p));
    return result;
}
void map_volume(const std::wstring& path, const std::wstring& id, std::vector<std::string> paths,
    const StorageMap& disks, std::vector<app::DeviceAccess>& mappings) {
    auto device_path = path;
    if (device_path.ends_with(L'\\')) device_path.pop_back();
    Handle handle(open_storage(device_path));
    if (!handle) return;
    const auto owners = volume_owners(handle.value, disks);
    if (owners.empty()) return;
    app::Volume volume; volume.id = utf8(id); volume.mount_paths = std::move(paths);
    wchar_t label[256]{}, filesystem[256]{};
    const auto root = path.starts_with(L"\\\\.\\") ? path.substr(4) : path;
    if (GetVolumeInformationW(root.c_str(), label, 256, nullptr, nullptr, nullptr, filesystem, 256)) {
        volume.label = utf8(label); volume.filesystem = utf8(filesystem);
    }
    for (const auto& owner : owners) mappings.push_back({owner, {}, {volume}});
}
void collect_device_access(app::Snapshot& snapshot) {
    ThreadErrorMode error_mode;
    std::vector<app::DeviceAccess> mappings;
    device_interfaces(GUID_DEVINTERFACE_COMPORT, snapshot.warnings,
        [&](HDEVINFO set, SP_DEVICE_INTERFACE_DATA& iface, SP_DEVINFO_DATA& dev, const wchar_t*) {
            auto owner = usb_ancestor(dev.DevInst);
            if (owner.empty()) return;
            auto port = serial_port(set, iface, dev);
            if (!port.empty()) mappings.push_back({std::move(owner), {std::move(port)}, {}});
        });
    StorageMap disks;
    auto collect_disk = [&](HDEVINFO, SP_DEVICE_INTERFACE_DATA&, SP_DEVINFO_DATA& dev, const wchar_t* path) {
        auto owner = usb_ancestor(dev.DevInst);
        if (owner.empty()) return;
        Handle handle(open_storage(path));
        STORAGE_DEVICE_NUMBER number{};
        if (handle && ioctl(handle.value, IOCTL_STORAGE_GET_DEVICE_NUMBER, &number, sizeof(number), nullptr, true))
            disks.emplace(StorageKey{number.DeviceType, number.DeviceNumber}, std::move(owner));
    };
    device_interfaces(GUID_DEVINTERFACE_DISK, snapshot.warnings, collect_disk);
    device_interfaces(GUID_DEVINTERFACE_CDROM, snapshot.warnings, collect_disk);
    if (!disks.empty()) {
        struct VolumeSearch {
            HANDLE value;
            ~VolumeSearch() { if (value != INVALID_HANDLE_VALUE) FindVolumeClose(value); }
        };
        wchar_t volume[1024]{};
        VolumeSearch search{FindFirstVolumeW(volume, 1024)};
        if (search.value != INVALID_HANDLE_VALUE) {
            do { map_volume(volume, volume, mount_paths(volume), disks, mappings); }
            while (FindNextVolumeW(search.value, volume, 1024));
            if (GetLastError() != ERROR_NO_MORE_FILES) snapshot.warnings.push_back("Volume enumeration: " + win_error(GetLastError()));
        }
        // Empty removable slots are omitted by FindFirstVolume but can still own a drive letter.
        wchar_t roots[512]{};
        const DWORD count = GetLogicalDriveStringsW(512, roots);
        if (count > 0 && count < 512) {
            for (const wchar_t* root = roots; *root; root += wcslen(root) + 1) {
                const UINT type = GetDriveTypeW(root);
                if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED && type != DRIVE_CDROM) continue;
                std::wstring id;
                if (GetVolumeNameForVolumeMountPointW(root, volume, 1024)) id = volume;
                else id = std::wstring(L"Drive ") + root;
                map_volume(std::wstring(L"\\\\.\\") + root, id, {utf8(root)}, disks, mappings);
            }
        }
    }
    app::apply_device_access(snapshot, mappings);
}
std::wstring node_name(HANDLE hub, ULONG port, bool driver) {
    std::vector<BYTE> buffer(8192, 0);
    auto* name = reinterpret_cast<USB_NODE_CONNECTION_NAME*>(buffer.data()); name->ConnectionIndex = port;
    if (!ioctl(hub, driver ? IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME : IOCTL_USB_GET_NODE_CONNECTION_NAME, buffer.data(), static_cast<DWORD>(buffer.size()))) return {};
    if (name->ActualLength > buffer.size() || name->ActualLength < offsetof(USB_NODE_CONNECTION_NAME, NodeName) + sizeof(wchar_t)) return {};
    return std::wstring(name->NodeName, wcsnlen_s(name->NodeName, (name->ActualLength - offsetof(USB_NODE_CONNECTION_NAME, NodeName))/sizeof(wchar_t)));
}
std::vector<std::uint8_t> descriptor(HANDLE hub, ULONG port, BYTE type, BYTE index, USHORT lang, USHORT size) {
    std::vector<BYTE> buffer(sizeof(USB_DESCRIPTOR_REQUEST) + size, 0);
    auto* req = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(buffer.data()); req->ConnectionIndex = port;
    req->SetupPacket.bmRequest = 0x80; req->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    req->SetupPacket.wValue = static_cast<USHORT>((type << 8) | index);
    req->SetupPacket.wIndex = lang; req->SetupPacket.wLength = size;
    DWORD actual = 0;
    if (!ioctl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, buffer.data(), static_cast<DWORD>(buffer.size()), &actual) || actual <= sizeof(USB_DESCRIPTOR_REQUEST)) return {};
    actual = std::min(actual, static_cast<DWORD>(buffer.size()));
    return {buffer.begin() + sizeof(USB_DESCRIPTOR_REQUEST), buffer.begin() + actual};
}
std::string string_descriptor(HANDLE hub, ULONG port, BYTE index, USHORT language) {
    if (!index) return {};
    auto b = descriptor(hub, port, USB_STRING_DESCRIPTOR_TYPE, index, language, 255);
    if (b.size() < 2 || b[1] != USB_STRING_DESCRIPTOR_TYPE || b[0] > b.size() || b[0] < 2 || b[0] % 2) return {};
    std::wstring w;
    for (std::size_t i = 2; i + 1 < b[0]; i += 2) w.push_back(static_cast<wchar_t>(b[i] | (b[i+1] << 8)));
    return utf8(w);
}
void child_functions(app::Node& n, DEVINST dev, int depth = 0) {
    if (depth > 5 || n.properties.size() > 150) return;
    DEVINST child = 0;
    if (CM_Get_Child(&child, dev, 0) != CR_SUCCESS) return;
    do {
        wchar_t text[512]{}; ULONG bytes = sizeof(text), type = 0;
        if (CM_Get_DevNode_Registry_PropertyW(child, CM_DRP_FRIENDLYNAME, &type, text, &bytes, 0) != CR_SUCCESS) {
            bytes = sizeof(text); CM_Get_DevNode_Registry_PropertyW(child, CM_DRP_DEVICEDESC, &type, text, &bytes, 0);
        }
        wchar_t id[MAX_DEVICE_ID_LEN]{}; CM_Get_Device_IDW(child, id, MAX_DEVICE_ID_LEN, 0);
        n.properties.push_back({"Child function", utf8(text) + " / " + utf8(id)});
        child_functions(n, child, depth + 1);
    } while (n.properties.size() <= 150 && CM_Get_Sibling(&child, child, 0) == CR_SUCCESS);
}
const char* connection_status(USB_CONNECTION_STATUS status) {
    switch (status) {
    case NoDeviceConnected: return "No device connected";
    case DeviceConnected: return "Connected";
    case DeviceFailedEnumeration: return "Enumeration failed";
    case DeviceGeneralFailure: return "Device failure";
    case DeviceCausedOvercurrent: return "Overcurrent";
    case DeviceNotEnoughPower: return "Insufficient power";
    case DeviceNotEnoughBandwidth: return "Insufficient bandwidth";
    case DeviceHubNestedTooDeeply: return "Hub nested too deeply";
    case DeviceInLegacyHub: return "Device in legacy hub";
    default: return "Unknown connection status";
    }
}
std::string speed_name(UCHAR speed) {
    switch (speed) {
    case UsbLowSpeed: return "Low-Speed (1.5 Mbit/s)";
    case UsbFullSpeed: return "Full-Speed (12 Mbit/s)";
    case UsbHighSpeed: return "High-Speed (480 Mbit/s)";
    case UsbSuperSpeed: return "SuperSpeed (5 Gbit/s)";
    default: return "Unknown";
    }
}
void enumerate_hub(app::Node& hub, const std::wstring& path, const DeviceMap& devices, app::Snapshot& snapshot, std::set<std::wstring>& visited, int depth) {
    if (depth > 16 || !visited.insert(folded(path)).second) { snapshot.warnings.push_back("Hub recursion stopped at " + hub.name); return; }
    Handle handle(open_device(path));
    if (!handle) { hub.status = "Hub unavailable"; snapshot.warnings.push_back(hub.name + ": " + win_error(GetLastError())); return; }
    hub.properties.push_back({"Hub interface", utf8(path)});
    USB_NODE_INFORMATION info{}; info.NodeType = UsbHub;
    if (!query(handle.value, IOCTL_USB_GET_NODE_INFORMATION, info)) {
        snapshot.warnings.push_back(hub.name + ": cannot read hub information: " + win_error(GetLastError())); return;
    }
    hub.port_count = info.u.HubInformation.HubDescriptor.bNumberOfPorts;
    USB_HUB_INFORMATION_EX extended{};
    if (query(handle.value, IOCTL_USB_GET_HUB_INFORMATION_EX, extended)) hub.port_count = extended.HighestPortNumber;
    hub.port_count = std::min(hub.port_count, 255u);
    hub.properties.push_back({"Downstream ports", std::to_string(hub.port_count)});
    hub.properties.push_back({"Hub power", info.u.HubInformation.HubIsBusPowered ? "Bus powered" : "Self powered"});
    for (ULONG port = 1; port <= hub.port_count; ++port) {
        std::vector<BYTE> buffer(sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + 32 * sizeof(USB_PIPE_INFO), 0);
        auto* c = reinterpret_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(buffer.data()); c->ConnectionIndex = port;
        app::Node n; n.port = port; n.id = hub.id + "/port/" + std::to_string(port);
        if (!ioctl(handle.value, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, buffer.data(), static_cast<DWORD>(buffer.size()))) {
            n.name = "Port " + std::to_string(port) + " - unavailable"; n.status = "Port query failed"; n.problem_code = GetLastError();
            n.connected = false; n.properties.push_back({"Query error", win_error(n.problem_code)});
            hub.children.push_back(std::move(n)); continue;
        }
        const bool connected = c->ConnectionStatus != NoDeviceConnected;
        if (connected) {
            const auto driver = node_name(handle.value, port, true);
            if (const auto it = devices.find(folded(driver)); it != devices.end()) {
                n = it->second.node; child_functions(n, it->second.devinst);
            }
        }
        n.port = port; n.connected = connected;
        if (n.id.empty()) n.id = hub.id + "/port/" + std::to_string(port);
        n.status = connection_status(c->ConnectionStatus);
        n.kind = connected ? (c->DeviceIsHub ? app::Kind::Hub : app::Kind::Device) : app::Kind::EmptyPort;
        if (c->ConnectionStatus > DeviceConnected) n.problem_code = static_cast<unsigned>(c->ConnectionStatus);
        if (n.problem_code && c->ConnectionStatus == DeviceConnected) n.status = "Windows problem " + std::to_string(n.problem_code);
        if (n.name.empty()) n.name = connected ? (c->DeviceIsHub ? "USB Hub" : "USB Device") : "Port " + std::to_string(port) + " - available";
        if (connected) n.speed = speed_name(c->Speed);
        USB_NODE_CONNECTION_INFORMATION_EX_V2 v2{}; v2.ConnectionIndex = port; v2.Length = sizeof(v2); v2.SupportedUsbProtocols.Usb300 = 1;
        if (query(handle.value, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2, v2)) {
            if (connected && v2.Flags.DeviceIsOperatingAtSuperSpeedOrHigher) n.speed = "SuperSpeed (5 Gbit/s)";
            if (connected && v2.Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher) n.speed = "SuperSpeedPlus (10 Gbit/s or higher)";
            n.properties.push_back({"Port protocols", std::string(v2.SupportedUsbProtocols.Usb110 ? "USB 1.x " : "") + (v2.SupportedUsbProtocols.Usb200 ? "USB 2.0 " : "") + (v2.SupportedUsbProtocols.Usb300 ? "USB 3.x" : "")});
            if (connected && v2.Flags.DeviceIsSuperSpeedCapableOrHigher && !v2.Flags.DeviceIsOperatingAtSuperSpeedOrHigher)
                n.properties.push_back({"Speed note", "SuperSpeed capable device is running below SuperSpeed"});
        }
        std::vector<BYTE> connectors(8192, 0);
        auto* connector = reinterpret_cast<USB_PORT_CONNECTOR_PROPERTIES*>(connectors.data()); connector->ConnectionIndex = port;
        if (ioctl(handle.value, IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES, connectors.data(), static_cast<DWORD>(connectors.size()))) {
            n.properties.push_back({"Connector", connector->UsbPortProperties.PortConnectorIsTypeC ? "USB Type-C" : "Not reported as Type-C"});
            n.properties.push_back({"User accessible", connector->UsbPortProperties.PortIsUserConnectable ? "Yes" : "No (internal or firmware-defined)"});
            if (connector->CompanionPortNumber) n.properties.push_back({"Companion port", std::to_string(connector->CompanionPortNumber)});
        }
        if (connected && c->DeviceDescriptor.bLength == sizeof(USB_DEVICE_DESCRIPTOR)) {
            const auto& d = c->DeviceDescriptor;
            n.vendor_id = d.idVendor; n.product_id = d.idProduct; n.usb_version = d.bcdUSB;
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&d); n.device_descriptor.assign(bytes, bytes + sizeof(d));
            n.properties.push_back({"Device address", std::to_string(c->DeviceAddress)});
            n.properties.push_back({"Current configuration", std::to_string(c->CurrentConfigurationValue)});
            n.properties.push_back({"Open pipes", std::to_string(c->NumberOfOpenPipes)});
            n.properties.push_back({"Device revision", "0x" + app::hex(d.bcdDevice)});
            n.properties.push_back({"USB device class", "0x" + app::hex(d.bDeviceClass, 2)});
            const auto languages = descriptor(handle.value, port, USB_STRING_DESCRIPTOR_TYPE, 0, 0, 255);
            const USHORT language = languages.size() >= 4 ? static_cast<USHORT>(languages[2] | (languages[3] << 8)) : 0x0409;
            auto product = string_descriptor(handle.value, port, d.iProduct, language);
            if (!product.empty()) { n.properties.push_back({"Windows name", n.name}); n.name = std::move(product); }
            auto maker = string_descriptor(handle.value, port, d.iManufacturer, language);
            if (!maker.empty()) n.manufacturer = std::move(maker);
            n.serial = string_descriptor(handle.value, port, d.iSerialNumber, language);
            auto header = descriptor(handle.value, port, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, 9);
            if (header.size() >= 9) {
                const auto size = static_cast<USHORT>(header[2] | (header[3] << 8));
                if (size >= 9) n.configuration = descriptor(handle.value, port, USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, size);
            }
            if (n.configuration.empty()) n.properties.push_back({"Descriptor note", "Configuration descriptor unavailable"});
        }
        if (c->DeviceIsHub && connected) {
            const auto child_path = node_name(handle.value, port, false);
            if (!child_path.empty()) enumerate_hub(n, child_path, devices, snapshot, visited, depth + 1);
            else snapshot.warnings.push_back(n.name + ": downstream hub path unavailable");
        }
        hub.children.push_back(std::move(n));
    }
}
}
bool enumerate_usb(app::Snapshot& snapshot, std::string& error) noexcept {
    try {
        app::Snapshot result; result.captured_at = timestamp();
        result.root.id = "computer"; result.root.kind = app::Kind::Computer;
        wchar_t hostname[256]{}; DWORD length = 256;
        result.root.name = GetComputerNameW(hostname, &length) ? utf8(hostname) : "This computer";
        result.root.properties.push_back({"Backend", "Windows SetupAPI / USB hub IOCTL"});
        auto devices = collect_metadata();
        DeviceSet set(SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_HOST_CONTROLLER, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
        if (set.value == INVALID_HANDLE_VALUE) { error = "Cannot enumerate USB controllers: " + win_error(GetLastError()); return false; }
        std::set<std::wstring> visited;
        for (DWORD index = 0;; ++index) {
            SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
            if (!SetupDiEnumDeviceInterfaces(set.value, nullptr, &GUID_DEVINTERFACE_USB_HOST_CONTROLLER, index, &iface)) {
                if (GetLastError() != ERROR_NO_MORE_ITEMS) result.warnings.push_back("Controller enumeration: " + win_error(GetLastError()));
                break;
            }
            DWORD size = 0;
            SetupDiGetDeviceInterfaceDetailW(set.value, &iface, nullptr, 0, &size, nullptr);
            if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 65536) continue;
            std::vector<BYTE> buffer(size, 0);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data()); detail->cbSize = sizeof(*detail);
            SP_DEVINFO_DATA dev{}; dev.cbSize = sizeof(dev);
            if (!SetupDiGetDeviceInterfaceDetailW(set.value, &iface, detail, size, nullptr, &dev)) continue;
            auto controller = metadata(set.value, dev).node; controller.kind = app::Kind::Controller;
            Handle handle(open_device(detail->DevicePath));
            if (handle) {
                std::vector<BYTE> root_buffer(8192, 0);
                auto* root_name = reinterpret_cast<USB_ROOT_HUB_NAME*>(root_buffer.data());
                if (ioctl(handle.value, IOCTL_USB_GET_ROOT_HUB_NAME, root_buffer.data(), static_cast<DWORD>(root_buffer.size())) &&
                    root_name->ActualLength <= root_buffer.size() && root_name->ActualLength > offsetof(USB_ROOT_HUB_NAME, RootHubName)) {
                    const std::wstring root_path(root_name->RootHubName, wcsnlen_s(root_name->RootHubName, (root_name->ActualLength - offsetof(USB_ROOT_HUB_NAME, RootHubName))/sizeof(wchar_t)));
                    app::Node hub; hub.kind = app::Kind::Hub; hub.id = controller.id + "/root"; hub.name = "Root hub";
                    enumerate_hub(hub, root_path, devices, result, visited, 0); controller.children.push_back(std::move(hub));
                } else result.warnings.push_back(controller.name + ": root hub unavailable: " + win_error(GetLastError()));
            } else result.warnings.push_back(controller.name + ": " + win_error(GetLastError()));
            result.root.children.push_back(std::move(controller));
        }
        collect_device_access(result);
        snapshot = std::move(result); error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
}
