#pragma once

#include "starfox/render/palette.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Backend-agnostic description of one presented frame.
//
// Projection, face visibility, clipping and draw order all run in the
// source's fixed-point arithmetic before anything reaches here. What arrives
// is already clipped and already ordered, in source raster units with the
// fraction kept, so a backend only rasterises and composites.
//
// Submission order is the contract: the source orders faces through its own
// BSP walk, so there is no depth buffer and no sorting.
namespace starfox::gfx {

// Screen position in source raster units. The fraction is what lets a backend
// resolve an edge more finely than the grid the cartridge worked on.
struct Vertex2D {
    float x{};
    float y{};
};

struct TexCoord {
    float u{};
    float v{};
};

// One palette entry, or two alternating in a checkerboard where the source
// dithers between colours.
struct Colour {
    std::uint8_t even{};
    std::uint8_t odd{};
    bool dither{};
};

struct TextureHandle {
    std::uint32_t value{};
    [[nodiscard]] bool valid() const noexcept { return value != 0U; }
};

// What the fill step records besides the colour it writes. The port's optional
// surface effects want the polygon's real normal and camera depth rather than
// a screen-space filter's guess at them, so filling a face means recording
// them too. Carried per primitive because that is where the value is constant:
// the source shades a face flat.
//
// The normal is unit length in camera space and the depth is the face's mean
// camera z, both produced before projection, so neither depends on the render
// scale.
struct SurfaceAttributes {
    float normal_x{};
    float normal_y{};
    float normal_z{1.0F};
    float depth{};
    // Cleared on a primitive whose pixels should leave the record untouched,
    // which is how lines and cartridge art behave in the built-in fill.
    bool recorded{};
};

// One rasterised pixel's surface record, at the backend's target resolution.
struct SurfacePixel {
    SurfaceAttributes surface{};
    std::uint8_t palette_index{};
};

enum class PrimitiveKind : std::uint8_t {
    triangle,
    line,
    textured_triangle,
};

// Triangles use three positions, lines the first two.
struct Primitive {
    PrimitiveKind kind{PrimitiveKind::triangle};
    Colour colour{};
    Vertex2D position[3]{};
    TexCoord texture_coordinate[3]{};
    TextureHandle texture{};
    SurfaceAttributes surface{};
};

struct ScenePass {
    std::span<const Primitive> primitives;
    std::int32_t offset_x{};
    std::int32_t offset_y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

// Cartridge-authored 8-bit art. It is composited unscaled so it keeps the
// look it was drawn with however finely the scene is rasterised.
struct ImagePass {
    std::span<const std::uint8_t> pixels;
    std::uint32_t width{};
    std::uint32_t height{};
    std::int32_t offset_x{};
    std::int32_t offset_y{};
    bool transparent_index_zero{true};
};

enum class PassKind : std::uint8_t {
    scene,
    image,
};

struct Pass {
    PassKind kind{PassKind::image};
    ScenePass scene{};
    ImagePass image{};
};

struct Frame {
    std::uint32_t width{256U};
    std::uint32_t height{224U};
    std::uint8_t clear_index{};
    std::span<const Pass> passes;
    std::span<const render::Rgba8> palette;
};

// Owns its storage so a frame can be assembled across subsystems and
// submitted once. The spans in `build` borrow it until the next `reset`.
class FrameBuilder {
public:
    void reset(std::uint32_t width, std::uint32_t height, std::uint8_t clear_index) {
        width_ = width;
        height_ = height;
        clear_index_ = clear_index;
        primitives_.clear();
        passes_.clear();
        scene_ranges_.clear();
    }

    void add_image(const ImagePass& image) {
        Pass pass;
        pass.kind = PassKind::image;
        pass.image = image;
        passes_.push_back(pass);
        scene_ranges_.push_back({0U, 0U});
    }

    void begin_scene(std::int32_t offset_x, std::int32_t offset_y,
        std::uint32_t width, std::uint32_t height) {
        Pass pass;
        pass.kind = PassKind::scene;
        pass.scene.offset_x = offset_x;
        pass.scene.offset_y = offset_y;
        pass.scene.width = width;
        pass.scene.height = height;
        passes_.push_back(pass);
        scene_ranges_.push_back(
            {static_cast<std::uint32_t>(primitives_.size()), 0U});
    }

    void add_primitive(const Primitive& primitive) {
        if (scene_ranges_.empty()) return;
        primitives_.push_back(primitive);
        ++scene_ranges_.back().count;
    }

    [[nodiscard]] std::size_t primitive_count() const noexcept {
        return primitives_.size();
    }

    [[nodiscard]] Frame build(std::span<const render::Rgba8> palette) {
        for (std::size_t index = 0; index < passes_.size(); ++index) {
            if (passes_[index].kind != PassKind::scene) continue;
            const auto& range = scene_ranges_[index];
            passes_[index].scene.primitives = std::span<const Primitive>{
                primitives_.data() + range.first, range.count};
        }
        Frame frame;
        frame.width = width_;
        frame.height = height_;
        frame.clear_index = clear_index_;
        frame.passes = passes_;
        frame.palette = palette;
        return frame;
    }

private:
    struct Range {
        std::uint32_t first{};
        std::uint32_t count{};
    };

    std::uint32_t width_{256U};
    std::uint32_t height_{224U};
    std::uint8_t clear_index_{};
    std::vector<Primitive> primitives_;
    std::vector<Pass> passes_;
    std::vector<Range> scene_ranges_;
};

} // namespace starfox::gfx
