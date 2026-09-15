#pragma once
#include "app/model.h"
#include <filesystem>
namespace e2e {
void start(app::State& state, const std::filesystem::path& base);
int tick();
void stop();
void destroy();
}

