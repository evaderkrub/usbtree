#pragma once
#include <memory>
namespace platform {
class DeviceNotifications {
public:
    DeviceNotifications();
    ~DeviceNotifications();
    bool consume();
    bool available() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

