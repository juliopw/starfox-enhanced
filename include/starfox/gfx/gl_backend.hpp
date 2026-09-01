#pragma once

#include "starfox/gfx/render_backend.hpp"

#include <memory>

namespace starfox::gfx {

// OpenGL 3.3 core backend. It resolves its own entry points through
// BackendInit::load_symbol, so it includes no OpenGL headers and links no
// OpenGL library; GLES and console GL differ only in the shader version.
class GlBackend final : public RenderBackend {
public:
    GlBackend();
    ~GlBackend() override;

    [[nodiscard]] bool initialise(const BackendInit& init) override;
    void shutdown() noexcept override;
    [[nodiscard]] Capabilities capabilities() const noexcept override;
    [[nodiscard]] TextureHandle create_texture(
        const TextureDescription& description) override;
    void destroy_texture(TextureHandle handle) noexcept override;
    [[nodiscard]] bool render(
        const Frame& frame, const RenderOptions& options) override;
    [[nodiscard]] RenderOptions applied_options() const noexcept override;
    [[nodiscard]] bool capture(std::uint32_t& width, std::uint32_t& height,
        std::vector<std::uint8_t>& rgba) override;
    [[nodiscard]] bool read_indexed(std::uint32_t& width, std::uint32_t& height,
        std::vector<std::uint8_t>& indices) override;
    [[nodiscard]] bool read_surfaces(std::uint32_t& width,
        std::uint32_t& height,
        std::vector<SurfacePixel>& samples) override;
    [[nodiscard]] std::string_view name() const noexcept override {
        return "opengl";
    }

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace starfox::gfx
