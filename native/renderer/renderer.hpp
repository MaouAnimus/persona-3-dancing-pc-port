#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include "../rhythm/rhythm_engine.hpp"
#include "../rhythm/track_catalog.hpp"

namespace p3d::renderer {

struct Vertex2D {
    float x, y, z;
    float u, v;
    float r, g, b, a;
    float mode; // 0 = solid/font, 1 = background tex, 2 = note orb, 3 = target ring, 4 = scratch, 5 = shockwave, 6 = center hub
};

struct Vertex3D {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float r, g, b, a;
};

struct Matrix4x4 {
    float m[4][4];
};

struct Submesh3D {
    uint32_t tex_id{0};
    uint32_t index_start{0};
    uint32_t index_count{0};
};

struct ConstantBuffer3D {
    Matrix4x4 world;
    Matrix4x4 view;
    Matrix4x4 proj;
    float light_dir[4];
    float ambient[4];
    float mat_mode[4];
};

struct JudgmentEffect {
    rhythm::Judgment judgment{rhythm::Judgment::None};
    int lane{0};
    double trigger_time_ms{0.0};
};

class D3D11Renderer {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer();

    bool initialize(HWND hwnd, int width, int height);
    void resize(int width, int height);
    void begin_frame();
    void render_title_screen(double anim_time_ms);
    void render_main_menu(int selected_option, double anim_time_ms);
    void render_song_select(const rhythm::TrackCatalog& catalog, size_t selected_index, int selected_diff, double anim_time_ms);
    void render_rhythm_scene(const rhythm::RhythmEngine& engine, double song_time_ms, double song_duration_ms = 184000.0,
                             const std::string& track_title = "BURN MY DREAD", float bpm = 145.0f,
                             int track_id = 101, const std::string& diff_name = "NORMAL");
    void render_pause_menu(int selected_option);
    void render_results_screen(const rhythm::RhythmStats& stats, const std::string& track_title = "", const std::string& diff_name = "");
    void trigger_judgment_effect(rhythm::Judgment j, int lane, double song_time_ms);
    void end_frame();

private:
    HWND hwnd_{nullptr};
    int width_{1280};
    int height_{720};

    ID3D11Device* device_{nullptr};
    ID3D11DeviceContext* context_{nullptr};
    IDXGISwapChain* swapchain_{nullptr};
    ID3D11RenderTargetView* rtv_{nullptr};

    // 2D Pipeline
    ID3D11VertexShader* vs_{nullptr};
    ID3D11PixelShader* ps_{nullptr};
    ID3D11InputLayout* input_layout_{nullptr};
    ID3D11Buffer* vbo_{nullptr};

    // 3D Character Pipeline
    ID3D11VertexShader* vs_3d_{nullptr};
    ID3D11PixelShader* ps_3d_{nullptr};
    ID3D11InputLayout* input_layout_3d_{nullptr};
    ID3D11Buffer* char_vbo_{nullptr};
    ID3D11Buffer* char_ibo_{nullptr};
    ID3D11Buffer* cb_3d_{nullptr};
    std::vector<Submesh3D> char_submeshes_;
    ID3D11Texture2D* char_textures_[5]{nullptr, nullptr, nullptr, nullptr, nullptr};
    ID3D11ShaderResourceView* char_srvs_[5]{nullptr, nullptr, nullptr, nullptr, nullptr};
    bool char_loaded_{false};

    // Depth Stencil
    ID3D11Texture2D* depth_stencil_buffer_{nullptr};
    ID3D11DepthStencilView* dsv_{nullptr};
    ID3D11DepthStencilState* ds_state_3d_{nullptr};
    ID3D11DepthStencilState* ds_state_ui_{nullptr};

    // Textures and pipeline states
    ID3D11Texture2D* bg_texture_{nullptr};
    ID3D11ShaderResourceView* bg_srv_{nullptr};
    ID3D11Texture2D* jacket_texture_{nullptr};
    ID3D11ShaderResourceView* jacket_srv_{nullptr};
    int current_jacket_id_{-1};

    // Crisp Atlus Font Atlas
    struct GlyphInfo {
        float u0{0.0f}, v0{0.0f}, u1{1.0f}, v1{1.0f};
        float adv{0.6f};
    };
    GlyphInfo glyphs_[128];
    ID3D11Texture2D* font_texture_{nullptr};
    ID3D11ShaderResourceView* font_srv_{nullptr};
    bool init_font_atlas();

    ID3D11Texture2D* title_logo_tex_{nullptr};
    ID3D11ShaderResourceView* title_logo_srv_{nullptr};
    ID3D11Texture2D* title_start_tex_{nullptr};
    ID3D11ShaderResourceView* title_start_srv_{nullptr};

    ID3D11SamplerState* sampler_state_{nullptr};
    ID3D11BlendState* blend_state_{nullptr};
    ID3D11RasterizerState* rasterizer_state_{nullptr};

    std::vector<JudgmentEffect> active_effects_;
    std::vector<Vertex2D> batch_vertices_;

    bool create_shaders();
    bool create_states();
    bool create_depth_stencil(int width, int height);
    bool load_texture_rgba(const std::string& path, uint32_t width, uint32_t height, 
                           ID3D11Texture2D** out_tex, ID3D11ShaderResourceView** out_srv);
    bool load_background_texture(const std::string& path);
    bool load_character_model(const std::string& bin_path);
    void update_jacket_texture(int track_id);

    // Batching and geometry primitives
    void push_quad(float x, float y, float w, float h, 
                   float r, float g, float b, float a, 
                   float mode = 0.0f, 
                   float u0 = 0.0f, float v0 = 0.0f, 
                   float u1 = 1.0f, float v1 = 1.0f);

    void push_circle(float cx, float cy, float radius, 
                     float r, float g, float b, float a, 
                     float mode);

    void push_line(float x0, float y0, float x1, float y1, 
                   float thickness, float r, float g, float b, float a);

    void draw_text(const std::string& text, float x, float y, float size, 
                   float r, float g, float b, float a, bool shadow = true);

    void draw_glyph(int lane, float cx, float cy, float size, 
                    float r, float g, float b, float a);

    void draw_star(float cx, float cy, float size, 
                   float r, float g, float b, float a, bool filled);

    void flush_batch();

    // Scene stages
    void draw_background();
    void draw_character_3d(double song_time_ms);
    void draw_radial_tracks(double song_time_ms);
    void draw_note_targets();
    void draw_notes(const rhythm::RhythmEngine& engine, double song_time_ms);
    void draw_effects(double song_time_ms);
    void draw_hud(const rhythm::RhythmStats& stats, double song_time_ms, double song_duration_ms,
                  const std::string& track_title = "BURN MY DREAD", float bpm = 145.0f,
                  int track_id = 101, const std::string& diff_name = "NORMAL");
};

} // namespace p3d::renderer
