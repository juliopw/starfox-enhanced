#include "starfox/gfx/draw_list.hpp"
#include "starfox/gfx/render_backend.hpp"
#include "starfox/gfx/software_backend.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<starfox::render::Rgba8> test_palette() {
    std::vector<starfox::render::Rgba8> palette(256);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        palette[index] = {static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255U - index),
            static_cast<std::uint8_t>(index / 2U), 255U};
    }
    return palette;
}

void test_registry_always_offers_a_backend() {
    const auto names = starfox::gfx::available_backends();
    require(!names.empty(), "no render backend is compiled in");
    require(std::find(names.begin(), names.end(), "software") != names.end(),
            "the software reference backend must always be present");
    require(starfox::gfx::make_render_backend("software") != nullptr,
            "the software backend could not be created by name");
    // An unknown name must fall back rather than leave a caller with nothing.
    const auto fallback = starfox::gfx::make_render_backend("no-such-backend");
    require(fallback != nullptr, "an unknown backend name returned nothing");
}

void test_frame_builder_keeps_submission_order() {
    starfox::gfx::FrameBuilder builder;
    builder.reset(16U, 16U, 0U);
    starfox::gfx::ImagePass backdrop;
    std::vector<std::uint8_t> pixels(16U * 16U, 3U);
    backdrop.pixels = pixels;
    backdrop.width = 16U;
    backdrop.height = 16U;
    backdrop.transparent_index_zero = false;
    builder.add_image(backdrop);
    builder.begin_scene(0, 0, 16U, 16U);
    starfox::gfx::Primitive primitive;
    primitive.position[0] = {0.0f, 0.0f};
    primitive.position[1] = {8.0f, 0.0f};
    primitive.position[2] = {0.0f, 8.0f};
    primitive.colour = {9U, 9U, false};
    builder.add_primitive(primitive);
    builder.add_primitive(primitive);
    builder.add_image(backdrop);

    const auto palette = test_palette();
    const auto frame = builder.build(palette);
    require(frame.passes.size() == 3U, "frame did not retain every pass");
    require(frame.passes[0].kind == starfox::gfx::PassKind::image,
            "the first pass should be the backdrop image");
    require(frame.passes[1].kind == starfox::gfx::PassKind::scene,
            "the second pass should be the scene");
    require(frame.passes[1].scene.primitives.size() == 2U,
            "the scene pass lost primitives");
    require(frame.passes[2].kind == starfox::gfx::PassKind::image,
            "passes were reordered");
}

void test_software_backend_composites_in_order() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");

    starfox::gfx::FrameBuilder builder;
    builder.reset(8U, 8U, 1U);
    std::vector<std::uint8_t> cover(8U * 8U, 5U);
    starfox::gfx::ImagePass image;
    image.pixels = cover;
    image.width = 8U;
    image.height = 8U;
    image.transparent_index_zero = false;
    builder.add_image(image);

    const auto palette = test_palette();
    const auto frame = builder.build(palette);
    starfox::gfx::RenderOptions options;
    options.render_scale = 1U;
    require(backend.render(frame, options), "software backend failed to render");
    const auto target = backend.indexed_target();
    require(target.size() == 64U, "target is the wrong size");
    require(std::all_of(target.begin(), target.end(),
                [](std::uint8_t index) { return index == 5U; }),
            "an opaque image pass did not cover the clear colour");
}

void test_scene_scales_while_images_keep_their_resolution() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");

    starfox::gfx::FrameBuilder builder;
    builder.reset(8U, 8U, 0U);
    builder.begin_scene(0, 0, 8U, 8U);
    // A half-pixel-wide sliver: at source resolution it can vanish, and at a
    // higher render scale it must survive. That difference is the whole point
    // of scaling only the drawing step.
    starfox::gfx::Primitive sliver;
    sliver.position[0] = {1.0f, 1.0f};
    sliver.position[1] = {1.4f, 1.0f};
    sliver.position[2] = {1.4f, 7.0f};
    sliver.colour = {7U, 7U, false};
    builder.add_primitive(sliver);

    const auto palette = test_palette();
    const auto frame = builder.build(palette);

    starfox::gfx::RenderOptions options;
    options.render_scale = 4U;
    require(backend.render(frame, options), "scaled render failed");
    require(backend.applied_options().render_scale == 4U,
            "the backend did not report the scale it used");
    const auto scaled = backend.indexed_target();
    require(scaled.size() == 32U * 32U, "scaled target has the wrong size");
    const auto scaled_covered = std::count(scaled.begin(), scaled.end(), 7U);
    require(scaled_covered > 0, "the sliver disappeared at a higher scale");

    options.render_scale = 1U;
    require(backend.render(frame, options), "unscaled render failed");
    const auto plain = backend.indexed_target();
    const auto plain_covered = std::count(plain.begin(), plain.end(), 7U);
    // Four times the edge length is sixteen times the area, so a shape that
    // covers anything at all must cover proportionally more.
    require(scaled_covered > plain_covered * 4,
            "the scaled draw did not resolve the shape more finely");
}

void test_capture_resolves_through_the_palette() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");
    starfox::gfx::FrameBuilder builder;
    builder.reset(4U, 4U, 2U);
    const auto palette = test_palette();
    const auto frame = builder.build(palette);
    starfox::gfx::RenderOptions options;
    require(backend.render(frame, options), "render failed");

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    require(backend.capture(width, height, rgba), "capture failed");
    require(width == 4U && height == 4U, "capture reported the wrong size");
    require(rgba.size() == 4U * 4U * 4U, "capture buffer is the wrong size");
    require(rgba[0] == palette[2].r && rgba[1] == palette[2].g
                && rgba[2] == palette[2].b,
            "the clear colour did not resolve through the palette");
}

void test_capabilities_state_what_this_port_needs() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");
    const auto capabilities = backend.capabilities();
    // The port composites and post-processes as palette indices and its
    // surface effects read the fill step's record. The reference backend has
    // to be able to do both, or it is not a yardstick for a device one.
    require(capabilities.indexed_readback,
            "the reference backend cannot return the indexed scene");
    require(capabilities.surface_attributes,
            "the reference backend cannot record surfaces");
}

void test_indexed_readback_matches_the_composed_target() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");
    starfox::gfx::FrameBuilder builder;
    builder.reset(8U, 8U, 3U);
    builder.begin_scene(0, 0, 8U, 8U);
    starfox::gfx::Primitive face;
    face.position[0] = {1.0f, 1.0f};
    face.position[1] = {7.0f, 1.0f};
    face.position[2] = {1.0f, 7.0f};
    face.colour = {9U, 9U, false};
    builder.add_primitive(face);

    const auto palette = test_palette();
    const auto frame = builder.build(palette);
    starfox::gfx::RenderOptions options;
    options.render_scale = 2U;
    require(backend.render(frame, options), "render failed");

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> indices;
    require(backend.read_indexed(width, height, indices),
            "indexed readback was refused");
    require(width == 16U && height == 16U,
            "indexed readback reported the wrong extent");
    const auto target = backend.indexed_target();
    require(indices.size() == target.size()
                && std::equal(indices.begin(), indices.end(), target.begin()),
            "indexed readback diverged from the composed target");
}

void test_surfaces_are_recorded_only_when_asked_for() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");

    starfox::gfx::FrameBuilder builder;
    builder.reset(8U, 8U, 0U);
    builder.begin_scene(0, 0, 8U, 8U);
    starfox::gfx::Primitive face;
    face.position[0] = {1.0f, 1.0f};
    face.position[1] = {7.0f, 1.0f};
    face.position[2] = {1.0f, 7.0f};
    face.colour = {9U, 9U, false};
    face.surface = {0.0f, 0.0f, 1.0f, 512.0f, true};
    builder.add_primitive(face);
    // A line covers pixels but claims no surface, exactly as it does in the
    // built-in fill.
    starfox::gfx::Primitive line;
    line.kind = starfox::gfx::PrimitiveKind::line;
    line.position[0] = {0.0f, 7.0f};
    line.position[1] = {7.0f, 7.0f};
    line.colour = {11U, 11U, false};
    builder.add_primitive(line);

    const auto palette = test_palette();
    const auto frame = builder.build(palette);

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<starfox::gfx::SurfacePixel> samples;

    starfox::gfx::RenderOptions options;
    require(backend.render(frame, options), "unrecorded render failed");
    require(!backend.applied_options().record_surfaces,
            "the backend claimed a record it was not asked for");
    require(!backend.read_surfaces(width, height, samples),
            "a record was returned for a frame that did not ask for one");
    require(backend.surface_target().empty(),
            "an unrequested record still cost a frame's worth of memory");

    options.record_surfaces = true;
    require(backend.render(frame, options), "recorded render failed");
    require(backend.applied_options().record_surfaces,
            "the backend did not report the record it made");
    require(backend.read_surfaces(width, height, samples),
            "the recorded surfaces could not be read back");
    require(width == 8U && height == 8U && samples.size() == 64U,
            "the record does not parallel the target");

    std::vector<std::uint8_t> indices;
    std::uint32_t index_width = 0;
    std::uint32_t index_height = 0;
    require(backend.read_indexed(index_width, index_height, indices),
            "indexed readback was refused");

    std::size_t recorded = 0;
    for (std::size_t pixel = 0; pixel < samples.size(); ++pixel) {
        const auto& sample = samples[pixel];
        if (!sample.surface.recorded) continue;
        ++recorded;
        require(sample.palette_index == indices[pixel],
                "a recorded pixel disagrees with the index drawn there");
        require(sample.palette_index == 9U,
                "the line's pixels were recorded as a surface");
        require(sample.surface.depth == 512.0f
                    && sample.surface.normal_z == 1.0f,
                "the face's normal and depth did not survive the fill");
    }
    require(recorded > 4U, "the face recorded almost nothing");
    require(recorded < samples.size(),
            "the record covered pixels no face reached");
}

void test_a_record_survives_the_render_scale() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");
    starfox::gfx::FrameBuilder builder;
    builder.reset(8U, 8U, 0U);
    builder.begin_scene(0, 0, 8U, 8U);
    starfox::gfx::Primitive face;
    face.position[0] = {1.0f, 1.0f};
    face.position[1] = {7.0f, 1.0f};
    face.position[2] = {1.0f, 7.0f};
    face.colour = {9U, 9U, false};
    face.surface = {0.0f, 0.0f, 1.0f, 512.0f, true};
    builder.add_primitive(face);
    const auto palette = test_palette();
    const auto frame = builder.build(palette);

    const auto covered = [&](std::uint32_t scale) {
        starfox::gfx::RenderOptions options;
        options.render_scale = scale;
        options.record_surfaces = true;
        require(backend.render(frame, options), "render failed");
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<starfox::gfx::SurfacePixel> samples;
        require(backend.read_surfaces(width, height, samples),
                "surfaces could not be read back");
        require(width == 8U * scale && height == 8U * scale,
                "the record was not sized to the scaled target");
        return std::count_if(samples.begin(), samples.end(),
            [](const starfox::gfx::SurfacePixel& sample) {
                return sample.surface.recorded;
            });
    };

    // Reading the record at the source raster while the scene was drawn
    // larger puts the effects on the wrong pixels, so it has to grow with the
    // picture: four times the edge is sixteen times the area.
    const auto plain = covered(1U);
    const auto scaled = covered(4U);
    require(plain > 0, "nothing was recorded at the source raster");
    require(scaled > plain * 8, "the record did not follow the render scale");
}

void test_texture_rejects_a_short_description() {
    starfox::gfx::SoftwareBackend backend;
    require(backend.initialise({}), "software backend refused to initialise");
    std::array<std::uint8_t, 4> pixels{};
    starfox::gfx::TextureDescription description;
    description.indices = pixels;
    description.width = 8U;
    description.height = 8U;
    require(!backend.create_texture(description).valid(),
            "a texture smaller than its own extent was accepted");
    description.width = 2U;
    description.height = 2U;
    require(backend.create_texture(description).valid(),
            "a well-formed texture was rejected");
}

} // namespace

int main() {
    test_registry_always_offers_a_backend();
    test_frame_builder_keeps_submission_order();
    test_software_backend_composites_in_order();
    test_scene_scales_while_images_keep_their_resolution();
    test_capture_resolves_through_the_palette();
    test_capabilities_state_what_this_port_needs();
    test_indexed_readback_matches_the_composed_target();
    test_surfaces_are_recorded_only_when_asked_for();
    test_a_record_survives_the_render_scale();
    test_texture_rejects_a_short_description();
    std::cout << "All gfx tests passed.\n";
    return 0;
}
