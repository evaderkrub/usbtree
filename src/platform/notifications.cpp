#include "platform/notifications.h"
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#include <cfgmgr32.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif
namespace platform {
struct DeviceNotifications::Impl {
    std::atomic_bool changed{false};
#ifdef _WIN32
    HCMNOTIFICATION registration = nullptr;
    static DWORD CALLBACK callback(HCMNOTIFICATION, PVOID context, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA, DWORD) {
        if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL || action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
            static_cast<Impl*>(context)->changed.store(true);
        return ERROR_SUCCESS;
    }
#elif defined(__APPLE__)
    IONotificationPortRef port = nullptr;
    io_iterator_t added = IO_OBJECT_NULL, removed = IO_OBJECT_NULL;
    bool registered = false;
    static void callback(void* context, io_iterator_t iterator) {
        bool found = false;
        while (auto device = IOIteratorNext(iterator)) { IOObjectRelease(device); found = true; }
        if (found) static_cast<Impl*>(context)->changed.store(true);
    }
#endif
};
DeviceNotifications::DeviceNotifications() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    CM_NOTIFY_FILTER filter{};
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
    CM_Register_Notification(&filter,impl_.get(),Impl::callback,&impl_->registration);
#elif defined(__APPLE__)
    impl_->port = IONotificationPortCreate(kIOMainPortDefault);
    if (!impl_->port) return;
    auto source = IONotificationPortGetRunLoopSource(impl_->port);
    if (!source) return;
    CFRunLoopAddSource(CFRunLoopGetMain(), source, CFSTR("UsbTreeNotifications"));
    const auto added = IOServiceAddMatchingNotification(impl_->port, kIOFirstMatchNotification,
        IOServiceMatching("IOUSBHostDevice"), Impl::callback, impl_.get(), &impl_->added);
    if (added == KERN_SUCCESS) Impl::callback(impl_.get(), impl_->added);
    const auto removed = IOServiceAddMatchingNotification(impl_->port, kIOTerminatedNotification,
        IOServiceMatching("IOUSBHostDevice"), Impl::callback, impl_.get(), &impl_->removed);
    if (removed == KERN_SUCCESS) Impl::callback(impl_.get(), impl_->removed);
    impl_->registered = added == KERN_SUCCESS && removed == KERN_SUCCESS;
    impl_->changed.store(false);
#endif
}
DeviceNotifications::~DeviceNotifications() {
#ifdef _WIN32
    if (impl_->registration) CM_Unregister_Notification(impl_->registration);
#elif defined(__APPLE__)
    if (impl_->port) {
        auto source = IONotificationPortGetRunLoopSource(impl_->port);
        if (source) CFRunLoopRemoveSource(CFRunLoopGetMain(), source, CFSTR("UsbTreeNotifications"));
    }
    if (impl_->added) IOObjectRelease(impl_->added);
    if (impl_->removed) IOObjectRelease(impl_->removed);
    if (impl_->port) IONotificationPortDestroy(impl_->port);
#endif
}
bool DeviceNotifications::consume() {
#ifdef __APPLE__
    if (impl_->registered) CFRunLoopRunInMode(CFSTR("UsbTreeNotifications"), 0, true);
#endif
    return impl_->changed.exchange(false);
}
bool DeviceNotifications::available() const {
#ifdef _WIN32
    return impl_->registration != nullptr;
#elif defined(__APPLE__)
    return impl_->registered;
#else
    return false;
#endif
}
}

