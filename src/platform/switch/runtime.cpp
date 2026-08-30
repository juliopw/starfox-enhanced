#include "starfox/platform/switch_runtime.hpp"

#include <fstream>

namespace {

std::filesystem::path startup_error_path(const char* executable) {
    if (executable != nullptr && *executable != '\0') {
        const auto directory = std::filesystem::path{executable}.parent_path();
        if (!directory.empty()) return directory / "starfox-enhanced.log";
    }
    return "starfox-enhanced.log";
}

} // namespace

namespace starfox::platform::switch_runtime {

WindowConfig window_config() noexcept {
    return {1280, 720};
}

std::filesystem::path executable_path(const char* argv0) {
    return argv0 == nullptr ? std::filesystem::path{}
                            : std::filesystem::path{argv0};
}

void clear_startup_error(const char* executable) noexcept {
    try {
        std::error_code error;
        std::filesystem::remove(startup_error_path(executable), error);
    } catch (...) {
    }
}

void report_startup_error(
    const char* executable, std::string_view message) noexcept {
    try {
        std::ofstream stream{startup_error_path(executable),
            std::ios::out | std::ios::trunc};
        stream << message << '\n';
    } catch (...) {
    }
}

} // namespace starfox::platform::switch_runtime
