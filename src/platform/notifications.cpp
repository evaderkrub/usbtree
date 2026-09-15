#include "platform/notifications.h"
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#include <cfgmgr32.h>
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
#endif
};
DeviceNotifications::DeviceNotifications() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    CM_NOTIFY_FILTER filter{};
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
    CM_Register_Notification(&filter,impl_.get(),Impl::callback,&impl_->registration);
#endif
}
DeviceNotifications::~DeviceNotifications() {
#ifdef _WIN32
    if (impl_->registration) CM_Unregister_Notification(impl_->registration);
#endif
}
bool DeviceNotifications::consume() { return impl_->changed.exchange(false); }
bool DeviceNotifications::available() const {
#ifdef _WIN32
    return impl_->registration != nullptr;
#else
    return false;
#endif
}
}

