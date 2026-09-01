#pragma once

#include "starfox/gfx/draw_list.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The whole graphics-API surface of the port. Nothing here names a graphics
// API, a windowing library or a platform, so adding a technology is one
// translation unit and one registry entry.
namespace starfox::gfx {

struct Capabilities {
    bool textured_primitives{};
    bool multisampling{};
    bool frame_capture{};
    // The port composites its layers and post-processes them as palette
    // indices, so a backend that cannot hand indices back can produce a
    // picture but cannot take part in that pipeline.
    bool indexed_readback{};
    // Records the polygon normal and camera depth the surface effects read.
    // A backend without it can still draw every frame; what it cannot do is
    // drive ENHANCED TEXTURES, SMOOTH POLYS or RTX LIGHTING.
    bool surface_attributes{};
    std::uint32_t max_target_size{};
};

enum class FilterMode : std::uint8_t {
    nearest,
    linear,
};

// A request, not a promise: a backend clamps what it cannot honour and
// reports the result through applied_options().
struct RenderOptions {
    // Rasterisation scale for the scene, in multiples of the source raster.
    // Image passes are never scaled.
    std::uint32_t render_scale{1U};
    std::uint32_t sample_count{1U};
    FilterMode present_filter{FilterMode::nearest};
    std::uint32_t output_width{};
    std::uint32_t output_height{};
    // Ask for the per-pixel surface record. It is the largest thing a frame
    // holds at a high render scale, so it is only produced when one of the
    // effects that reads it is switched on. A backend that cannot record
    // clears this in applied_options() rather than pretending it did.
    bool record_surfaces{};
};

// Host handles a backend may need. Unused members stay null, which is how one
// struct serves an OpenGL context, a Vulkan surface and a console display.
struct BackendInit {
    void* (*load_symbol)(const char* name){};
    void* native_display{};
    void* native_window{};
    void* native_instance{};
    void* native_surface{};
};

struct TextureDescription {
    std::span<const std::uint8_t> indices;
    std::uint32_t width{};
    std::uint32_t height{};
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;

    // Returns false with last_error() set when the device is unavailable, so
    // a caller can fall back. Backends never throw: drivers and consoles
    // report failure by status.
    [[nodiscard]] virtual bool initialise(const BackendInit& init) = 0;
    virtual void shutdown() noexcept = 0;

    [[nodiscard]] virtual Capabilities capabilities() const noexcept = 0;

    [[nodiscard]] virtual TextureHandle create_texture(
        const TextureDescription& description) = 0;
    virtual void destroy_texture(TextureHandle handle) noexcept = 0;

    // Draws one frame. Passes composite in the order given.
    [[nodiscard]] virtual bool render(
        const Frame& frame, const RenderOptions& options) = 0;

    [[nodiscard]] virtual RenderOptions applied_options() const noexcept = 0;

    // Tightly packed top-down RGBA. Backends without capture return false.
    [[nodiscard]] virtual bool capture(std::uint32_t& width,
        std::uint32_t& height, std::vector<std::uint8_t>& rgba) {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(rgba);
        return false;
    }

    // The composed scene as palette indices, tightly packed and top-down at
    // the applied render scale. This is the form the port composites and
    // post-processes in, so it is what a backend has to return to stand in
    // for the built-in fill. Guarded by Capabilities::indexed_readback.
    [[nodiscard]] virtual bool read_indexed(std::uint32_t& width,
        std::uint32_t& height, std::vector<std::uint8_t>& indices) {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(indices);
        return false;
    }

    // The surface record for the same pixels, in the same layout. Returns
    // false unless the last render honoured RenderOptions::record_surfaces.
    [[nodiscard]] virtual bool read_surfaces(std::uint32_t& width,
        std::uint32_t& height, std::vector<SurfacePixel>& samples) {
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(samples);
        return false;
    }

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }

protected:
    RenderBackend() = default;
    void set_error(std::string message) { last_error_ = std::move(message); }

private:
    std::string last_error_;
};

// Every backend compiled into this build, most capable first.
[[nodiscard]] std::vector<std::string_view> available_backends();

// An empty or unknown name selects the first available backend, so a missing
// one never prevents start-up.
[[nodiscard]] std::unique_ptr<RenderBackend> make_render_backend(
    std::string_view name = {});

} // namespace starfox::gfx
