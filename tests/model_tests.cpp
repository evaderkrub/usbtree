#include "app/model.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace { void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); } }
int main() {
    try {
        auto snapshot = app::demo_snapshot();
        auto counts = app::count(snapshot.root);
        check(counts.controllers == 1 && counts.hubs == 1 && counts.devices == 2 && counts.empty_ports == 1, "Topology counts");
        const auto* keyboard = app::find(snapshot.root, "keyboard");
        check(keyboard && app::matches(*keyboard, "1234 ABCD"), "Case insensitive ID search");
        check(app::visible(snapshot.root, "studio keyboard", false), "Search retains ancestors");
        check(!app::visible(snapshot.root, "missing", false), "Search empty result");
        check(!app::visible(*app::find(snapshot.root, "empty"), "", false), "Hide empty ports");
        app::State s; s.selected_id = "keyboard";
        app::accept_snapshot(s, snapshot); check(s.selected_id == "keyboard", "Preserve selection");
        s.selected_id = "removed"; app::accept_snapshot(s, snapshot); check(s.selected_id == "computer", "Removed selection fallback");
        std::vector<app::Property> rows; std::string error;
        check(app::decode_configuration(keyboard->configuration, rows, error) && rows.size() == 5, "Decode interfaces and endpoints");
        auto bad = keyboard->configuration; bad[9] = 0;
        check(!app::decode_configuration(bad, rows, error), "Reject zero length descriptor");
        bad = keyboard->configuration; bad.pop_back();
        check(!app::decode_configuration(bad, rows, error), "Reject truncated descriptors");
        check(app::clamp_scale(std::numeric_limits<float>::quiet_NaN()) == 1 && app::clamp_scale(8) == 2, "Clamp invalid GUI scale");
        check(app::full_report(snapshot).find("Studio Keyboard") != std::string::npos, "Report includes topology");
        std::cout << "All model tests passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
