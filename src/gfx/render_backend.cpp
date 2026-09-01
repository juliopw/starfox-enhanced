#include "starfox/gfx/render_backend.hpp"

#include "starfox/gfx/software_backend.hpp"

#if defined(STARFOX_ENABLE_OPENGL)
#include "starfox/gfx/gl_backend.hpp"
#endif

#include <algorithm>
#include <array>
#include <functional>

namespace starfox::gfx {
namespace {

struct Entry {
    std::string_view name;
    std::unique_ptr<RenderBackend> (*create)();
};

// Most capable first; make_render_backend walks this order when given no
// name. Adding a technology is one entry plus one translation unit.
const std::array<Entry, 0
#if defined(STARFOX_ENABLE_OPENGL)
    + 1
#endif
    + 1> registry{{
#if defined(STARFOX_ENABLE_OPENGL)
    Entry{"opengl", []() -> std::unique_ptr<RenderBackend> {
        return std::make_unique<GlBackend>();
    }},
#endif
    Entry{"software", []() -> std::unique_ptr<RenderBackend> {
        return std::make_unique<SoftwareBackend>();
    }},
}};

} // namespace

std::vector<std::string_view> available_backends() {
    std::vector<std::string_view> names;
    names.reserve(registry.size());
    for (const auto& entry : registry) names.push_back(entry.name);
    return names;
}

std::unique_ptr<RenderBackend> make_render_backend(std::string_view name) {
    if (registry.empty()) return nullptr;
    if (name.empty()) return registry.front().create();
    const auto found = std::find_if(registry.begin(), registry.end(),
        [name](const Entry& entry) { return entry.name == name; });
    return found == registry.end() ? registry.front().create() : found->create();
}

} // namespace starfox::gfx
