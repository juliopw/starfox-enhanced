#include "starfox/gfx/gl_backend.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace starfox::gfx {
namespace {

// The slice of OpenGL used here, declared so this file needs no OpenGL
// headers and no import library.
using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLboolean = unsigned char;
using GLintptr = std::intptr_t;
using GLsizeiptr = std::intptr_t;

constexpr GLenum GL_FALSE_ = 0;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_LINES = 0x0001;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_RED = 0x1903;
constexpr GLenum GL_R8 = 0x8229;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STREAM_DRAW = 0x88E0;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_COLOR_ATTACHMENT1 = 0x8CE1;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_NONE_ = 0;
constexpr GLenum GL_COLOR = 0x1800;
constexpr GLenum GL_RGBA32F = 0x8814;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_MAX_TEXTURE_SIZE = 0x0D33;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_CULL_FACE = 0x0B44;

struct Api {
    void (*Enable)(GLenum);
    void (*Disable)(GLenum);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*Clear)(GLbitfield);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*GetIntegerv)(GLenum, GLint*);
    void (*PixelStorei)(GLenum, GLint);
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void (*GenTextures)(GLsizei, GLuint*);
    void (*DeleteTextures)(GLsizei, const GLuint*);
    void (*BindTexture)(GLenum, GLuint);
    void (*ActiveTexture)(GLenum);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
        GLenum, const void*);
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
        GLenum, GLenum, const void*);
    void (*GenBuffers)(GLsizei, GLuint*);
    void (*DeleteBuffers)(GLsizei, const GLuint*);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void (*GenVertexArrays)(GLsizei, GLuint*);
    void (*DeleteVertexArrays)(GLsizei, const GLuint*);
    void (*BindVertexArray)(GLuint);
    void (*EnableVertexAttribArray)(GLuint);
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei,
        const void*);
    GLuint (*CreateShader)(GLenum);
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void (*CompileShader)(GLuint);
    void (*GetShaderiv)(GLuint, GLenum, GLint*);
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*DeleteShader)(GLuint);
    GLuint (*CreateProgram)();
    void (*AttachShader)(GLuint, GLuint);
    void (*LinkProgram)(GLuint);
    void (*GetProgramiv)(GLuint, GLenum, GLint*);
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*UseProgram)(GLuint);
    void (*DeleteProgram)(GLuint);
    GLint (*GetUniformLocation)(GLuint, const GLchar*);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform2f)(GLint, GLfloat, GLfloat);
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*GenFramebuffers)(GLsizei, GLuint*);
    void (*DeleteFramebuffers)(GLsizei, const GLuint*);
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum (*CheckFramebufferStatus)(GLenum);
    void (*DrawBuffers)(GLsizei, const GLenum*);
    void (*ReadBuffer)(GLenum);
    void (*ClearBufferfv)(GLenum, GLint, const GLfloat*);
};

// One shader pair serves every pass.
//
// The scene target holds palette indices, not colour: the port composites its
// layers, post-processes them and fades them in index space, so that is the
// form a backend has to leave the frame in. Resolving through the palette is
// the last step before a frame is shown or captured, not part of drawing it.
//
// The second output is the surface record the port's optional effects read.
// It is only attached while a frame asks for it, and a draw that must not
// disturb it - a line, cartridge art - is issued with that draw buffer set to
// GL_NONE rather than writing a blank over it.
constexpr const char* vertex_source = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColour;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aSurface;
uniform vec2 uTarget;
out vec3 vColour;
out vec2 vTexCoord;
out vec4 vSurface;
void main() {
    vColour = aColour;
    vTexCoord = aTexCoord;
    vSurface = aSurface;
    vec2 ndc = vec2(aPosition.x / uTarget.x, aPosition.y / uTarget.y) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

constexpr const char* fragment_source = R"(#version 330 core
in vec3 vColour;
in vec2 vTexCoord;
in vec4 vSurface;
uniform sampler2D uSource;
uniform int uMode;          // 0 flat, 1 indexed texture, 2 indexed image
uniform int uDiscardZero;
layout(location = 0) out vec4 oIndex;
layout(location = 1) out vec4 oSurface;
void main() {
    float index;
    if (uMode == 0) {
        float parity = mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0);
        index = (vColour.z > 0.5 && parity > 0.5) ? vColour.y : vColour.x;
    } else {
        index = texture(uSource, vTexCoord).r * 255.0;
    }
    if (uDiscardZero != 0 && index < 0.5) discard;
    // An eight-bit normalised target round-trips every one of the 256 indices
    // exactly, so this is a store and not an approximation.
    oIndex = vec4(index / 255.0, 0.0, 0.0, 1.0);
    oSurface = vSurface;
}
)";

struct Vertex {
    GLfloat x{};
    GLfloat y{};
    GLfloat even{};
    GLfloat odd{};
    GLfloat dither{};
    GLfloat u{};
    GLfloat v{};
    GLfloat normal_x{};
    GLfloat normal_y{};
    GLfloat normal_z{};
    GLfloat depth{};
};

} // namespace

struct GlBackend::State {
    Api api{};
    bool ready{};
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLuint scene_texture{};
    GLuint scene_framebuffer{};
    GLuint surface_texture{};
    GLuint image_texture{};
    GLint uniform_target{-1};
    GLint uniform_source{-1};
    GLint uniform_mode{-1};
    GLint uniform_discard{-1};
    std::uint32_t target_width{};
    std::uint32_t target_height{};
    std::uint32_t surface_width{};
    std::uint32_t surface_height{};
    bool recording_surfaces{};
    std::uint32_t max_texture{};
    RenderOptions applied{};
    std::vector<Vertex> batch;
    std::vector<render::Rgba8> palette;

    struct Texture {
        GLuint id{};
        std::uint32_t width{};
        std::uint32_t height{};
    };
    std::unordered_map<std::uint32_t, Texture> textures;
    std::uint32_t next_handle{1U};
};

GlBackend::GlBackend() : state_(std::make_unique<State>()) {}
GlBackend::~GlBackend() { shutdown(); }

namespace {

template <typename Fn>
bool load(Fn& slot, void* (*loader)(const char*), const char* name) {
    slot = reinterpret_cast<Fn>(loader(name));
    return slot != nullptr;
}

} // namespace

bool GlBackend::initialise(const BackendInit& init) {
    if (init.load_symbol == nullptr) {
        set_error("OpenGL backend needs a symbol loader");
        return false;
    }
    auto& api = state_->api;
    auto* get = init.load_symbol;
    bool ok = true;
    ok &= load(api.Enable, get, "glEnable");
    ok &= load(api.Disable, get, "glDisable");
    ok &= load(api.Viewport, get, "glViewport");
    ok &= load(api.ClearColor, get, "glClearColor");
    ok &= load(api.Clear, get, "glClear");
    ok &= load(api.DrawArrays, get, "glDrawArrays");
    ok &= load(api.GetIntegerv, get, "glGetIntegerv");
    ok &= load(api.PixelStorei, get, "glPixelStorei");
    ok &= load(api.ReadPixels, get, "glReadPixels");
    ok &= load(api.GenTextures, get, "glGenTextures");
    ok &= load(api.DeleteTextures, get, "glDeleteTextures");
    ok &= load(api.BindTexture, get, "glBindTexture");
    ok &= load(api.ActiveTexture, get, "glActiveTexture");
    ok &= load(api.TexParameteri, get, "glTexParameteri");
    ok &= load(api.TexImage2D, get, "glTexImage2D");
    ok &= load(api.TexSubImage2D, get, "glTexSubImage2D");
    ok &= load(api.GenBuffers, get, "glGenBuffers");
    ok &= load(api.DeleteBuffers, get, "glDeleteBuffers");
    ok &= load(api.BindBuffer, get, "glBindBuffer");
    ok &= load(api.BufferData, get, "glBufferData");
    ok &= load(api.GenVertexArrays, get, "glGenVertexArrays");
    ok &= load(api.DeleteVertexArrays, get, "glDeleteVertexArrays");
    ok &= load(api.BindVertexArray, get, "glBindVertexArray");
    ok &= load(api.EnableVertexAttribArray, get, "glEnableVertexAttribArray");
    ok &= load(api.VertexAttribPointer, get, "glVertexAttribPointer");
    ok &= load(api.CreateShader, get, "glCreateShader");
    ok &= load(api.ShaderSource, get, "glShaderSource");
    ok &= load(api.CompileShader, get, "glCompileShader");
    ok &= load(api.GetShaderiv, get, "glGetShaderiv");
    ok &= load(api.GetShaderInfoLog, get, "glGetShaderInfoLog");
    ok &= load(api.DeleteShader, get, "glDeleteShader");
    ok &= load(api.CreateProgram, get, "glCreateProgram");
    ok &= load(api.AttachShader, get, "glAttachShader");
    ok &= load(api.LinkProgram, get, "glLinkProgram");
    ok &= load(api.GetProgramiv, get, "glGetProgramiv");
    ok &= load(api.GetProgramInfoLog, get, "glGetProgramInfoLog");
    ok &= load(api.UseProgram, get, "glUseProgram");
    ok &= load(api.DeleteProgram, get, "glDeleteProgram");
    ok &= load(api.GetUniformLocation, get, "glGetUniformLocation");
    ok &= load(api.Uniform1i, get, "glUniform1i");
    ok &= load(api.Uniform2f, get, "glUniform2f");
    ok &= load(api.Uniform4f, get, "glUniform4f");
    ok &= load(api.GenFramebuffers, get, "glGenFramebuffers");
    ok &= load(api.DeleteFramebuffers, get, "glDeleteFramebuffers");
    ok &= load(api.BindFramebuffer, get, "glBindFramebuffer");
    ok &= load(api.FramebufferTexture2D, get, "glFramebufferTexture2D");
    ok &= load(api.CheckFramebufferStatus, get, "glCheckFramebufferStatus");
    ok &= load(api.DrawBuffers, get, "glDrawBuffers");
    ok &= load(api.ReadBuffer, get, "glReadBuffer");
    ok &= load(api.ClearBufferfv, get, "glClearBufferfv");
    if (!ok) {
        set_error("the OpenGL 3.3 core entry points are not all available");
        return false;
    }

    const auto compile = [&](GLenum type, const char* source) -> GLuint {
        const auto shader = api.CreateShader(type);
        api.ShaderSource(shader, 1, &source, nullptr);
        api.CompileShader(shader);
        GLint status = 0;
        api.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == 0) {
            char log[1024]{};
            api.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
            set_error(std::string{"shader did not compile: "} + log);
            api.DeleteShader(shader);
            return 0;
        }
        return shader;
    };
    const auto vertex = compile(GL_VERTEX_SHADER, vertex_source);
    if (vertex == 0) return false;
    const auto fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment == 0) return false;
    state_->program = api.CreateProgram();
    api.AttachShader(state_->program, vertex);
    api.AttachShader(state_->program, fragment);
    api.LinkProgram(state_->program);
    GLint linked = 0;
    api.GetProgramiv(state_->program, GL_LINK_STATUS, &linked);
    api.DeleteShader(vertex);
    api.DeleteShader(fragment);
    if (linked == 0) {
        char log[1024]{};
        api.GetProgramInfoLog(state_->program, sizeof(log), nullptr, log);
        set_error(std::string{"shader program did not link: "} + log);
        return false;
    }

    state_->uniform_target = api.GetUniformLocation(state_->program, "uTarget");
    state_->uniform_source = api.GetUniformLocation(state_->program, "uSource");
    state_->uniform_mode = api.GetUniformLocation(state_->program, "uMode");
    state_->uniform_discard = api.GetUniformLocation(state_->program, "uDiscardZero");

    api.GenVertexArrays(1, &state_->vao);
    api.GenBuffers(1, &state_->vbo);
    api.BindVertexArray(state_->vao);
    api.BindBuffer(GL_ARRAY_BUFFER, state_->vbo);
    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    api.EnableVertexAttribArray(0);
    api.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE_, stride, nullptr);
    api.EnableVertexAttribArray(1);
    api.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE_, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, even)));
    api.EnableVertexAttribArray(2);
    api.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE_, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, u)));
    api.EnableVertexAttribArray(3);
    api.VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE_, stride,
        reinterpret_cast<const void*>(offsetof(Vertex, normal_x)));
    api.BindVertexArray(0);

    api.GenTextures(1, &state_->image_texture);
    api.GenFramebuffers(1, &state_->scene_framebuffer);
    api.GenTextures(1, &state_->scene_texture);
    api.GenTextures(1, &state_->surface_texture);

    GLint max_texture = 2048;
    api.GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    state_->max_texture = static_cast<std::uint32_t>(std::max(max_texture, 1));
    state_->ready = true;
    return true;
}

void GlBackend::shutdown() noexcept {
    if (state_ == nullptr || !state_->ready) return;
    auto& api = state_->api;
    for (const auto& [handle, texture] : state_->textures) {
        api.DeleteTextures(1, &texture.id);
    }
    state_->textures.clear();
    api.DeleteTextures(1, &state_->image_texture);
    api.DeleteTextures(1, &state_->scene_texture);
    api.DeleteTextures(1, &state_->surface_texture);
    api.DeleteFramebuffers(1, &state_->scene_framebuffer);
    api.DeleteBuffers(1, &state_->vbo);
    api.DeleteVertexArrays(1, &state_->vao);
    api.DeleteProgram(state_->program);
    // A later initialise() generates fresh names, so the sizes recorded for
    // the old ones must not persuade it that its targets are already there.
    state_->target_width = 0U;
    state_->target_height = 0U;
    state_->surface_width = 0U;
    state_->surface_height = 0U;
    state_->recording_surfaces = false;
    state_->palette.clear();
    state_->batch.clear();
    state_->ready = false;
}

Capabilities GlBackend::capabilities() const noexcept {
    Capabilities capabilities;
    capabilities.textured_primitives = true;
    capabilities.multisampling = false;
    capabilities.frame_capture = true;
    capabilities.indexed_readback = true;
    capabilities.surface_attributes = true;
    capabilities.max_target_size = state_->max_texture;
    return capabilities;
}

RenderOptions GlBackend::applied_options() const noexcept {
    return state_->applied;
}

TextureHandle GlBackend::create_texture(const TextureDescription& description) {
    if (!state_->ready) return {};
    if (description.width == 0U || description.height == 0U
        || description.indices.size()
            < static_cast<std::size_t>(description.width) * description.height) {
        set_error("texture description does not cover its own extent");
        return {};
    }
    auto& api = state_->api;
    State::Texture texture;
    texture.width = description.width;
    texture.height = description.height;
    api.GenTextures(1, &texture.id);
    api.BindTexture(GL_TEXTURE_2D, texture.id);
    api.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    api.TexImage2D(GL_TEXTURE_2D, 0, GL_R8,
        static_cast<GLsizei>(description.width),
        static_cast<GLsizei>(description.height), 0, GL_RED, GL_UNSIGNED_BYTE,
        description.indices.data());
    const auto handle = state_->next_handle++;
    state_->textures.emplace(handle, texture);
    return TextureHandle{handle};
}

void GlBackend::destroy_texture(TextureHandle handle) noexcept {
    if (!state_->ready) return;
    const auto found = state_->textures.find(handle.value);
    if (found == state_->textures.end()) return;
    state_->api.DeleteTextures(1, &found->second.id);
    state_->textures.erase(found);
}

bool GlBackend::render(const Frame& frame, const RenderOptions& options) {
    if (!state_->ready) {
        set_error("backend is not initialised");
        return false;
    }
    auto& api = state_->api;
    auto scale = std::max<std::uint32_t>(1U, options.render_scale);
    const auto longest = std::max(frame.width, frame.height);
    if (longest != 0U && longest * scale > state_->max_texture) {
        scale = std::max<std::uint32_t>(1U, state_->max_texture / longest);
    }
    const auto width = frame.width * scale;
    const auto height = frame.height * scale;

    state_->applied = options;
    state_->applied.render_scale = scale;
    state_->applied.sample_count = 1U;
    state_->recording_surfaces = options.record_surfaces;

    const auto attach = [&](std::uint32_t target,
                            std::uint32_t attachment_width,
                            std::uint32_t attachment_height,
                            GLenum attachment, GLint internal_format,
                            GLenum format, GLenum type) {
        api.BindTexture(GL_TEXTURE_2D, target);
        api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        api.TexImage2D(GL_TEXTURE_2D, 0, internal_format,
            static_cast<GLsizei>(attachment_width),
            static_cast<GLsizei>(attachment_height), 0, format, type, nullptr);
        api.BindFramebuffer(GL_FRAMEBUFFER, state_->scene_framebuffer);
        api.FramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
            target, 0);
    };

    if (width != state_->target_width || height != state_->target_height) {
        // Eight bits of index, not colour: see the note above the shaders.
        attach(state_->scene_texture, width, height, GL_COLOR_ATTACHMENT0,
            GL_R8, GL_RED, GL_UNSIGNED_BYTE);
        state_->target_width = width;
        state_->target_height = height;
        state_->surface_width = 0U;
        state_->surface_height = 0U;
    }
    // The surface record is the largest thing a frame holds at a high render
    // scale, so it is only allocated while something reads it. It is detached
    // when it is not, rather than left attached at a stale size: a bound
    // attachment narrows the framebuffer to the smallest one it holds, so a
    // leftover would silently clip every later frame to its extent.
    const auto surface_width = state_->recording_surfaces ? width : 0U;
    const auto surface_height = state_->recording_surfaces ? height : 0U;
    if (surface_width != state_->surface_width
        || surface_height != state_->surface_height) {
        if (surface_width == 0U) {
            api.BindFramebuffer(GL_FRAMEBUFFER, state_->scene_framebuffer);
            api.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                GL_TEXTURE_2D, 0, 0);
        } else {
            // Full float: a face's camera depth runs into the thousands,
            // which a half would round away. GLES needs
            // EXT_color_buffer_float for this, which is core from 3.2.
            attach(state_->surface_texture, surface_width, surface_height,
                GL_COLOR_ATTACHMENT1, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        }
        state_->surface_width = surface_width;
        state_->surface_height = surface_height;
    }
    api.BindFramebuffer(GL_FRAMEBUFFER, state_->scene_framebuffer);
    if (api.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        set_error("scene framebuffer is incomplete");
        state_->recording_surfaces = false;
        state_->applied.record_surfaces = false;
        return false;
    }

    // Kept for capture, which is the one place a frame turns into colour.
    state_->palette.assign(frame.palette.begin(), frame.palette.end());

    api.Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    api.Disable(GL_DEPTH_TEST);
    api.Disable(GL_CULL_FACE);
    api.Disable(GL_BLEND);

    const GLenum index_only[2] = {GL_COLOR_ATTACHMENT0, GL_NONE_};
    const GLenum index_and_surface[2] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    const GLfloat clear_index[4] = {
        static_cast<GLfloat>(frame.clear_index) / 255.0f, 0.0f, 0.0f, 1.0f};
    if (state_->recording_surfaces) {
        api.DrawBuffers(2, index_and_surface);
        api.ClearBufferfv(GL_COLOR, 0, clear_index);
        // An all-zero normal is not a direction any face can have, so it is
        // what "no face covered this pixel" reads as.
        const GLfloat clear_surface[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        api.ClearBufferfv(GL_COLOR, 1, clear_surface);
    } else {
        api.DrawBuffers(2, index_only);
        api.ClearBufferfv(GL_COLOR, 0, clear_index);
    }

    api.UseProgram(state_->program);
    api.Uniform2f(state_->uniform_target, static_cast<GLfloat>(width),
        static_cast<GLfloat>(height));
    api.Uniform1i(state_->uniform_source, 0);
    api.BindVertexArray(state_->vao);
    api.BindBuffer(GL_ARRAY_BUFFER, state_->vbo);

    const auto scale_f = static_cast<GLfloat>(scale);
    const auto flush = [&](GLenum mode, GLuint texture, int shader_mode,
                           int discard_zero, bool records) {
        if (state_->batch.empty()) return;
        // A draw that must leave the record alone is masked off it rather
        // than writing a blank over what an earlier face put there.
        api.DrawBuffers(2, records && state_->recording_surfaces
            ? index_and_surface : index_only);
        api.Uniform1i(state_->uniform_mode, shader_mode);
        api.Uniform1i(state_->uniform_discard, discard_zero);
        api.ActiveTexture(GL_TEXTURE0);
        api.BindTexture(GL_TEXTURE_2D, texture);
        api.BufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(state_->batch.size() * sizeof(Vertex)),
            state_->batch.data(), GL_STREAM_DRAW);
        api.DrawArrays(mode, 0, static_cast<GLsizei>(state_->batch.size()));
        state_->batch.clear();
    };

    for (const auto& pass : frame.passes) {
        if (pass.kind == PassKind::image) {
            const auto& image = pass.image;
            if (image.width == 0U || image.height == 0U) continue;
            api.BindTexture(GL_TEXTURE_2D, state_->image_texture);
            api.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
            api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            api.TexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                static_cast<GLsizei>(image.width),
                static_cast<GLsizei>(image.height), 0, GL_RED, GL_UNSIGNED_BYTE,
                image.pixels.data());
            const auto left = static_cast<GLfloat>(image.offset_x) * scale_f;
            const auto top = static_cast<GLfloat>(image.offset_y) * scale_f;
            const auto right = left + static_cast<GLfloat>(image.width) * scale_f;
            const auto bottom = top + static_cast<GLfloat>(image.height) * scale_f;
            const Vertex quad[6] = {
                {left, top, 0, 0, 0, 0.0f, 0.0f},
                {right, top, 0, 0, 0, 1.0f, 0.0f},
                {right, bottom, 0, 0, 0, 1.0f, 1.0f},
                {left, top, 0, 0, 0, 0.0f, 0.0f},
                {right, bottom, 0, 0, 0, 1.0f, 1.0f},
                {left, bottom, 0, 0, 0, 0.0f, 1.0f},
            };
            state_->batch.assign(std::begin(quad), std::end(quad));
            // Cartridge art keeps the look it was drawn with and claims no
            // surface, exactly as it does in the built-in fill.
            flush(GL_TRIANGLES, state_->image_texture, 2,
                image.transparent_index_zero ? 1 : 0, false);
            continue;
        }

        const auto offset_x = static_cast<GLfloat>(pass.scene.offset_x);
        const auto offset_y = static_cast<GLfloat>(pass.scene.offset_y);
        PrimitiveKind batched = PrimitiveKind::triangle;
        GLuint batched_texture = 0;
        bool batched_records = false;
        bool open = false;
        const auto flush_batch = [&] {
            flush(batched == PrimitiveKind::line ? GL_LINES : GL_TRIANGLES,
                batched_texture,
                batched == PrimitiveKind::textured_triangle ? 1 : 0,
                batched == PrimitiveKind::textured_triangle ? 1 : 0,
                batched_records);
        };
        for (const auto& primitive : pass.scene.primitives) {
            GLuint texture = 0;
            if (primitive.kind == PrimitiveKind::textured_triangle) {
                const auto found = state_->textures.find(primitive.texture.value);
                if (found != state_->textures.end()) texture = found->second.id;
            }
            // A line never claims a surface, so it cannot share a draw with a
            // face that does.
            const auto records = primitive.surface.recorded
                && primitive.kind != PrimitiveKind::line;
            // Order is the contract, so a batch only extends while the kind,
            // texture and record mask stay the same.
            if (open && (primitive.kind != batched || texture != batched_texture
                    || records != batched_records)) {
                flush_batch();
                open = false;
            }
            batched = primitive.kind;
            batched_texture = texture;
            batched_records = records;
            open = true;
            const auto count = primitive.kind == PrimitiveKind::line ? 2U : 3U;
            for (std::uint32_t index = 0; index < count; ++index) {
                Vertex vertex;
                vertex.x = (primitive.position[index].x + offset_x) * scale_f;
                vertex.y = (primitive.position[index].y + offset_y) * scale_f;
                vertex.even = static_cast<GLfloat>(primitive.colour.even);
                vertex.odd = static_cast<GLfloat>(primitive.colour.odd);
                vertex.dither = primitive.colour.dither ? 1.0f : 0.0f;
                if (records) {
                    vertex.normal_x = primitive.surface.normal_x;
                    vertex.normal_y = primitive.surface.normal_y;
                    vertex.normal_z = primitive.surface.normal_z;
                    vertex.depth = primitive.surface.depth;
                }
                if (primitive.kind == PrimitiveKind::textured_triangle
                    && batched_texture != 0) {
                    const auto found =
                        state_->textures.find(primitive.texture.value);
                    const auto tw = static_cast<GLfloat>(found->second.width);
                    const auto th = static_cast<GLfloat>(found->second.height);
                    vertex.u = primitive.texture_coordinate[index].u / tw;
                    vertex.v = primitive.texture_coordinate[index].v / th;
                }
                state_->batch.push_back(vertex);
            }
        }
        if (open) flush_batch();
    }

    api.DrawBuffers(2, state_->recording_surfaces
        ? index_and_surface : index_only);
    api.BindVertexArray(0);
    api.BindFramebuffer(GL_FRAMEBUFFER, 0);
    state_->applied.record_surfaces = state_->recording_surfaces;
    return true;
}

namespace {

// OpenGL reads bottom-up; every caller here expects the top row first.
template <typename T>
void flip_rows(std::vector<T>& pixels, std::uint32_t width, std::uint32_t height) {
    const auto stride = static_cast<std::size_t>(width);
    for (std::uint32_t row = 0; row < height / 2U; ++row) {
        std::swap_ranges(
            pixels.begin() + static_cast<std::ptrdiff_t>(row * stride),
            pixels.begin() + static_cast<std::ptrdiff_t>((row + 1U) * stride),
            pixels.begin()
                + static_cast<std::ptrdiff_t>((height - 1U - row) * stride));
    }
}

} // namespace

bool GlBackend::read_indexed(std::uint32_t& width, std::uint32_t& height,
    std::vector<std::uint8_t>& indices) {
    if (!state_->ready || state_->target_width == 0U) return false;
    auto& api = state_->api;
    width = state_->target_width;
    height = state_->target_height;
    indices.assign(static_cast<std::size_t>(width) * height, 0U);
    api.BindFramebuffer(GL_FRAMEBUFFER, state_->scene_framebuffer);
    api.PixelStorei(GL_PACK_ALIGNMENT, 1);
    api.ReadBuffer(GL_COLOR_ATTACHMENT0);
    api.ReadPixels(0, 0, static_cast<GLsizei>(width),
        static_cast<GLsizei>(height), GL_RED, GL_UNSIGNED_BYTE, indices.data());
    api.BindFramebuffer(GL_FRAMEBUFFER, 0);
    flip_rows(indices, width, height);
    return true;
}

bool GlBackend::read_surfaces(std::uint32_t& width, std::uint32_t& height,
    std::vector<SurfacePixel>& samples) {
    if (!state_->ready || !state_->recording_surfaces
        || state_->surface_width == 0U) {
        return false;
    }
    auto& api = state_->api;
    width = state_->surface_width;
    height = state_->surface_height;
    const auto count = static_cast<std::size_t>(width) * height;
    std::vector<GLfloat> raw(count * 4U, 0.0f);
    api.BindFramebuffer(GL_FRAMEBUFFER, state_->scene_framebuffer);
    api.PixelStorei(GL_PACK_ALIGNMENT, 1);
    api.ReadBuffer(GL_COLOR_ATTACHMENT1);
    api.ReadPixels(0, 0, static_cast<GLsizei>(width),
        static_cast<GLsizei>(height), GL_RGBA, GL_FLOAT, raw.data());
    api.BindFramebuffer(GL_FRAMEBUFFER, 0);
    flip_rows(raw, width * 4U, height);

    // The palette index belongs to the same pixel of the same frame, so it
    // comes from the index target rather than a second copy of it.
    std::uint32_t index_width = 0U;
    std::uint32_t index_height = 0U;
    std::vector<std::uint8_t> indices;
    const auto have_indices = read_indexed(index_width, index_height, indices)
        && index_width == width && index_height == height;

    samples.assign(count, SurfacePixel{});
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        const auto normal_x = raw[pixel * 4U];
        const auto normal_y = raw[pixel * 4U + 1U];
        const auto normal_z = raw[pixel * 4U + 2U];
        if (normal_x == 0.0f && normal_y == 0.0f && normal_z == 0.0f) continue;
        samples[pixel].surface = {normal_x, normal_y, normal_z,
            raw[pixel * 4U + 3U], true};
        samples[pixel].palette_index = have_indices ? indices[pixel] : 0U;
    }
    return true;
}

bool GlBackend::capture(std::uint32_t& width, std::uint32_t& height,
    std::vector<std::uint8_t>& rgba) {
    // Drawing leaves the frame as indices, so capture is where a palette is
    // applied - the same order the port itself uses.
    std::vector<std::uint8_t> indices;
    if (!read_indexed(width, height, indices)) return false;
    rgba.assign(indices.size() * 4U, 0U);
    for (std::size_t pixel = 0; pixel < indices.size(); ++pixel) {
        const auto index = indices[pixel];
        const auto colour = index < state_->palette.size()
            ? state_->palette[index] : render::Rgba8{};
        rgba[pixel * 4U] = colour.r;
        rgba[pixel * 4U + 1U] = colour.g;
        rgba[pixel * 4U + 2U] = colour.b;
        rgba[pixel * 4U + 3U] = colour.a;
    }
    return true;
}

} // namespace starfox::gfx
