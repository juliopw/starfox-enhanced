#pragma once

#include <filesystem>
#include <string_view>

namespace starfox::platform::switch_runtime {

struct WindowConfig {
    int width;
    int height;
};

WindowConfig window_config() noexcept;
std::filesystem::path executable_path(const char* argv0);
void clear_startup_error(const char* executable) noexcept;
void report_startup_error(
    const char* executable, std::string_view message) noexcept;

} // namespace starfox::platform::switch_runtime
