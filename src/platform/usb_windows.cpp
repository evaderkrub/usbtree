#include "platform/usb.h"
namespace platform {
bool enumerate_usb(app::Snapshot& snapshot, std::string& error) noexcept {
    snapshot.root.id = "computer"; snapshot.root.name = "This computer"; snapshot.root.kind = app::Kind::Computer;
    error = "Windows USB enumeration is being initialized.";
    return false;
}
}
