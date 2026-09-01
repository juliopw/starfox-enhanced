#include "starfox/gfx/software_backend.hpp"

#include <algorithm>
#include <cmath>

namespace starfox::gfx {
namespace {

// One edge convention across all three edges keeps adjacent faces from
// double-filling or leaving a seam between them.
[[nodiscard]] float edge(const Vertex2D& a, const Vertex2D& b, float x, float y) {
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

} // namespace

bool SoftwareBackend::initialise(const BackendInit&) {
    return true;
}

void SoftwareBackend::shutdown() noexcept {
    textures_.clear();
    target_.clear();
    surfaces_.clear();
    recording_surfaces_ = false;
}

Capabilities SoftwareBackend::capabilities() const noexcept {
    Capabilities capabilities;
    capabilities.textured_primitives = true;
    capabilities.multisampling = false;
    capabilities.frame_capture = true;
    capabilities.indexed_readback = true;
    capabilities.surface_attributes = true;
    capabilities.max_target_size = 16384U;
    return capabilities;
}

TextureHandle SoftwareBackend::create_texture(const TextureDescription& description) {
    if (description.width == 0U || description.height == 0U
        || description.indices.size()
            < static_cast<std::size_t>(description.width) * description.height) {
        set_error("texture description does not cover its own extent");
        return {};
    }
    Texture texture;
    texture.width = description.width;
    texture.height = description.height;
    texture.indices.assign(description.indices.begin(), description.indices.end());
    const auto handle = next_handle_++;
    textures_.emplace(handle, std::move(texture));
    return TextureHandle{handle};
}

void SoftwareBackend::destroy_texture(TextureHandle handle) noexcept {
    textures_.erase(handle.value);
}

void SoftwareBackend::resize_target(std::uint32_t width, std::uint32_t height) {
    if (width == target_width_ && height == target_height_) return;
    target_width_ = width;
    target_height_ = height;
    target_.assign(static_cast<std::size_t>(width) * height, 0U);
    surfaces_.clear();
}

void SoftwareBackend::plot(std::int32_t x, std::int32_t y, std::uint8_t index,
    const SurfaceAttributes* record) noexcept {
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(target_width_)
        || y >= static_cast<std::int32_t>(target_height_)) {
        return;
    }
    const auto offset = static_cast<std::size_t>(y) * target_width_
        + static_cast<std::size_t>(x);
    target_[offset] = index;
    if (record == nullptr || !recording_surfaces_) return;
    surfaces_[offset].surface = *record;
    surfaces_[offset].palette_index = index;
}

std::uint8_t SoftwareBackend::resolve(
    const Colour& colour, std::int32_t x, std::int32_t y) noexcept {
    if (!colour.dither) return colour.even;
    return ((x + y) & 1) == 0 ? colour.even : colour.odd;
}

void SoftwareBackend::draw_line(const Primitive& primitive, float scale) {
    const auto x0 = primitive.position[0].x * scale;
    const auto y0 = primitive.position[0].y * scale;
    const auto x1 = primitive.position[1].x * scale;
    const auto y1 = primitive.position[1].y * scale;
    const auto steps = static_cast<std::int32_t>(std::lround(
        std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
    if (steps <= 0) {
        const auto ix = static_cast<std::int32_t>(std::lround(x0));
        const auto iy = static_cast<std::int32_t>(std::lround(y0));
        plot(ix, iy, resolve(primitive.colour, ix, iy), nullptr);
        return;
    }
    const auto dx = (x1 - x0) / static_cast<float>(steps);
    const auto dy = (y1 - y0) / static_cast<float>(steps);
    for (std::int32_t step = 0; step <= steps; ++step) {
        const auto ix = static_cast<std::int32_t>(
            std::lround(x0 + dx * static_cast<float>(step)));
        const auto iy = static_cast<std::int32_t>(
            std::lround(y0 + dy * static_cast<float>(step)));
        plot(ix, iy, resolve(primitive.colour, ix, iy), nullptr);
    }
}

void SoftwareBackend::draw_triangle(const Primitive& primitive, float scale) {
    Vertex2D vertex[3];
    for (std::size_t index = 0; index < 3U; ++index) {
        vertex[index].x = primitive.position[index].x * scale;
        vertex[index].y = primitive.position[index].y * scale;
    }
    auto area = edge(vertex[0], vertex[1], vertex[2].x, vertex[2].y);
    if (area == 0.0f) return;
    // The source emits both windings and does not cull, so neither is
    // dropped here.
    const auto flip = area < 0.0f ? -1.0f : 1.0f;
    area *= flip;

    const auto min_x = std::max(0, static_cast<std::int32_t>(std::floor(
        std::min({vertex[0].x, vertex[1].x, vertex[2].x}))));
    const auto max_x = std::min(static_cast<std::int32_t>(target_width_) - 1,
        static_cast<std::int32_t>(std::ceil(
            std::max({vertex[0].x, vertex[1].x, vertex[2].x}))));
    const auto min_y = std::max(0, static_cast<std::int32_t>(std::floor(
        std::min({vertex[0].y, vertex[1].y, vertex[2].y}))));
    const auto max_y = std::min(static_cast<std::int32_t>(target_height_) - 1,
        static_cast<std::int32_t>(std::ceil(
            std::max({vertex[0].y, vertex[1].y, vertex[2].y}))));

    const auto* texture = primitive.kind == PrimitiveKind::textured_triangle
        ? find_texture(primitive.texture) : nullptr;
    // The source shades a face flat, so its record is constant across the
    // triangle and needs no interpolation.
    const auto* record = primitive.surface.recorded ? &primitive.surface : nullptr;

    for (auto y = min_y; y <= max_y; ++y) {
        const auto sample_y = static_cast<float>(y) + 0.5f;
        for (auto x = min_x; x <= max_x; ++x) {
            const auto sample_x = static_cast<float>(x) + 0.5f;
            const auto w0 =
                edge(vertex[1], vertex[2], sample_x, sample_y) * flip;
            const auto w1 =
                edge(vertex[2], vertex[0], sample_x, sample_y) * flip;
            const auto w2 =
                edge(vertex[0], vertex[1], sample_x, sample_y) * flip;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            if (texture == nullptr) {
                plot(x, y, resolve(primitive.colour, x, y), record);
                continue;
            }
            const auto u = (w0 * primitive.texture_coordinate[0].u
                + w1 * primitive.texture_coordinate[1].u
                + w2 * primitive.texture_coordinate[2].u) / area;
            const auto v = (w0 * primitive.texture_coordinate[0].v
                + w1 * primitive.texture_coordinate[1].v
                + w2 * primitive.texture_coordinate[2].v) / area;
            const auto tx = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::floor(u))
                    & static_cast<std::int32_t>(texture->width - 1U));
            const auto ty = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::floor(v))
                    & static_cast<std::int32_t>(texture->height - 1U));
            const auto index = texture->indices[
                static_cast<std::size_t>(ty) * texture->width + tx];
            if (index == 0U) continue;
            plot(x, y, index, record);
        }
    }
}

const SoftwareBackend::Texture* SoftwareBackend::find_texture(
    TextureHandle handle) const noexcept {
    const auto found = textures_.find(handle.value);
    return found == textures_.end() ? nullptr : &found->second;
}

void SoftwareBackend::composite_image(const ImagePass& image, std::uint32_t scale) {
    if (image.width == 0U || image.height == 0U) return;
    if (image.pixels.size()
        < static_cast<std::size_t>(image.width) * image.height) {
        return;
    }
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto index = image.pixels[
                static_cast<std::size_t>(y) * image.width + x];
            if (index == 0U && image.transparent_index_zero) continue;
            const auto origin_x =
                (static_cast<std::int32_t>(x) + image.offset_x)
                * static_cast<std::int32_t>(scale);
            const auto origin_y =
                (static_cast<std::int32_t>(y) + image.offset_y)
                * static_cast<std::int32_t>(scale);
            for (std::uint32_t row = 0; row < scale; ++row) {
                for (std::uint32_t column = 0; column < scale; ++column) {
                    plot(origin_x + static_cast<std::int32_t>(column),
                        origin_y + static_cast<std::int32_t>(row), index,
                        nullptr);
                }
            }
        }
    }
}

bool SoftwareBackend::render(const Frame& frame, const RenderOptions& options) {
    const auto scale = std::max<std::uint32_t>(1U, options.render_scale);
    applied_ = options;
    applied_.render_scale = scale;
    applied_.sample_count = 1U;
    applied_.output_width = frame.width * scale;
    applied_.output_height = frame.height * scale;

    resize_target(frame.width * scale, frame.height * scale);
    std::fill(target_.begin(), target_.end(), frame.clear_index);
    recording_surfaces_ = options.record_surfaces;
    if (recording_surfaces_) {
        surfaces_.assign(target_.size(), SurfacePixel{});
    } else {
        surfaces_.clear();
        surfaces_.shrink_to_fit();
    }
    palette_.assign(frame.palette.begin(), frame.palette.end());

    for (const auto& pass : frame.passes) {
        if (pass.kind == PassKind::image) {
            composite_image(pass.image, scale);
            continue;
        }
        const auto offset_x = static_cast<float>(pass.scene.offset_x);
        const auto offset_y = static_cast<float>(pass.scene.offset_y);
        for (const auto& source : pass.scene.primitives) {
            auto primitive = source;
            for (auto& position : primitive.position) {
                position.x += offset_x;
                position.y += offset_y;
            }
            if (primitive.kind == PrimitiveKind::line) {
                draw_line(primitive, static_cast<float>(scale));
            } else {
                draw_triangle(primitive, static_cast<float>(scale));
            }
        }
    }
    return true;
}

bool SoftwareBackend::read_indexed(std::uint32_t& width, std::uint32_t& height,
    std::vector<std::uint8_t>& indices) {
    width = target_width_;
    height = target_height_;
    indices.assign(target_.begin(), target_.end());
    return true;
}

bool SoftwareBackend::read_surfaces(std::uint32_t& width, std::uint32_t& height,
    std::vector<SurfacePixel>& samples) {
    if (!recording_surfaces_) return false;
    width = target_width_;
    height = target_height_;
    samples.assign(surfaces_.begin(), surfaces_.end());
    return true;
}

bool SoftwareBackend::capture(std::uint32_t& width, std::uint32_t& height,
    std::vector<std::uint8_t>& rgba) {
    width = target_width_;
    height = target_height_;
    rgba.resize(static_cast<std::size_t>(target_width_) * target_height_ * 4U);
    for (std::size_t pixel = 0; pixel < target_.size(); ++pixel) {
        const auto index = target_[pixel];
        const auto colour = index < palette_.size()
            ? palette_[index] : render::Rgba8{};
        rgba[pixel * 4U] = colour.r;
        rgba[pixel * 4U + 1U] = colour.g;
        rgba[pixel * 4U + 2U] = colour.b;
        rgba[pixel * 4U + 3U] = colour.a;
    }
    return true;
}

} // namespace starfox::gfx
