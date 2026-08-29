#pragma once

#include "starfox/gfx/render_backend.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace starfox::gfx {

// Reference backend. It needs no device, so it runs in tests and on a build
// machine, and it is the yardstick a device backend is measured against.
class SoftwareBackend final : public RenderBackend {
public:
    [[nodiscard]] bool initialise(const BackendInit& init) override;
    void shutdown() noexcept override;
    [[nodiscard]] Capabilities capabilities() const noexcept override;
    [[nodiscard]] TextureHandle create_texture(
        const TextureDescription& description) override;
    void destroy_texture(TextureHandle handle) noexcept override;
    [[nodiscard]] bool render(
        const Frame& frame, const RenderOptions& options) override;
    [[nodiscard]] RenderOptions applied_options() const noexcept override {
        return applied_;
    }
    [[nodiscard]] bool capture(std::uint32_t& width, std::uint32_t& height,
        std::vector<std::uint8_t>& rgba) override;
    [[nodiscard]] bool read_indexed(std::uint32_t& width, std::uint32_t& height,
        std::vector<std::uint8_t>& indices) override;
    [[nodiscard]] bool read_surfaces(std::uint32_t& width,
        std::uint32_t& height,
        std::vector<SurfacePixel>& samples) override;
    [[nodiscard]] std::string_view name() const noexcept override {
        return "software";
    }

    // Composed indexed target, without a palette round trip or a copy.
    [[nodiscard]] std::span<const std::uint8_t> indexed_target() const noexcept {
        return target_;
    }

    // Empty unless the last render recorded surfaces.
    [[nodiscard]] std::span<const SurfacePixel> surface_target() const noexcept {
        return surfaces_;
    }

private:
    struct Texture {
        std::uint32_t width{};
        std::uint32_t height{};
        std::vector<std::uint8_t> indices;
    };

    void resize_target(std::uint32_t width, std::uint32_t height);
    // A null record leaves the surface target alone, which is how lines and
    // cartridge art behave in the built-in fill.
    void plot(std::int32_t x, std::int32_t y, std::uint8_t index,
        const SurfaceAttributes* record) noexcept;
    static std::uint8_t resolve(
        const Colour& colour, std::int32_t x, std::int32_t y) noexcept;
    void draw_line(const Primitive& primitive, float scale);
    void draw_triangle(const Primitive& primitive, float scale);
    void composite_image(const ImagePass& image, std::uint32_t scale);
    [[nodiscard]] const Texture* find_texture(
        TextureHandle handle) const noexcept;

    std::uint32_t target_width_{};
    std::uint32_t target_height_{};
    std::vector<std::uint8_t> target_;
    // Sized only while a render records, so an unused record costs nothing.
    std::vector<SurfacePixel> surfaces_;
    bool recording_surfaces_{};
    std::vector<render::Rgba8> palette_;
    std::unordered_map<std::uint32_t, Texture> textures_;
    std::uint32_t next_handle_{1U};
    RenderOptions applied_{};
};

} // namespace starfox::gfx
