#pragma once
#include "app/model.h"
namespace platform {
bool enumerate_usb(app::Snapshot& snapshot, std::string& error) noexcept;
}
