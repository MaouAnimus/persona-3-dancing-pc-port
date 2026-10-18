#include "renderer.hpp"
#include <d3dcompiler.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace p3d::renderer {

// 3D Matrix Math Utilities
static Matrix4x4 matrix_identity() {
    Matrix4x4 res = {};
    res.m[0][0] = 1.0f; res.m[1][1] = 1.0f; res.m[2][2] = 1.0f; res.m[3][3] = 1.0f;
    return res;
}

static Matrix4x4 matrix_multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 res = {};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            res.m[r][c] = a.m[r][0] * b.m[0][c] +
                          a.m[r][1] * b.m[1][c] +
                          a.m[r][2] * b.m[2][c] +
                          a.m[r][3] * b.m[3][c];
        }
    }
    return res;
}

static Matrix4x4 matrix_translation(float tx, float ty, float tz) {
    Matrix4x4 res = matrix_identity();
    res.m[3][0] = tx;
    res.m[3][1] = ty;
    res.m[3][2] = tz;
    return res;
}

static Matrix4x4 matrix_scale(float sx, float sy, float sz) {
    Matrix4x4 res = {};
    res.m[0][0] = sx;
    res.m[1][1] = sy;
    res.m[2][2] = sz;
    res.m[3][3] = 1.0f;
    return res;
}

static Matrix4x4 matrix_rotation_y(float angle) {
    Matrix4x4 res = matrix_identity();
    float c = std::cos(angle);
    float s = std::sin(angle);
    res.m[0][0] = c;
    res.m[0][2] = -s;
    res.m[2][0] = s;
    res.m[2][2] = c;
    return res;
}

static Matrix4x4 matrix_perspective_fov_lh(float fov_y_rad, float aspect, float near_z, float far_z) {
    Matrix4x4 res = {};
    float h = 1.0f / std::tan(fov_y_rad * 0.5f);
    float w = h / aspect;
    float range = far_z / (far_z - near_z);

    res.m[0][0] = w;
    res.m[1][1] = h;
    res.m[2][2] = range;
    res.m[2][3] = 1.0f;
    res.m[3][2] = -near_z * range;
    return res;
}

// 2D Multi-Mode HLSL Shader
static const char* s_hlsl_source = R"(
Texture2D txBackground : register(t0);
Texture2D txJacket     : register(t1);
Texture2D txFont       : register(t2);
SamplerState samLinear : register(s0);

struct VS_INPUT {
    float3 pos  : POSITION;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
    float  mode : TEXCOORD1;
};

struct PS_INPUT {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
    float  mode : TEXCOORD1;
};

PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.pos  = float4(input.pos, 1.0f);
    output.uv   = input.uv;
    output.col  = input.col;
    output.mode = input.mode;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_Target {
    float m = input.mode;

    // Mode 0: Solid Quad / Styled Font Pixel
    if (m < 0.5f) {
        return input.col;
    }

    // Mode 1: Official Background Artwork with gentle vignette
    if (m < 1.5f) {
        float4 tex = txBackground.Sample(samLinear, input.uv);
        float2 p = input.uv * 2.0f - 1.0f;
        float vig = 1.0f - dot(p, p) * 0.25f;
        return float4(tex.rgb * input.col.rgb * vig, input.col.a);
    }

    // Centered coordinates for circular modes (2..6)
    float2 p = input.uv * 2.0f - 1.0f;
    float d = length(p);

    // Mode 2: Luminous Note Orb
    if (m < 2.5f) {
        if (d > 1.05f) discard;
        float core = 1.0f - smoothstep(0.0f, 0.45f, d);
        float rim  = 1.0f - smoothstep(0.75f, 0.98f, d);
        float ring = smoothstep(0.12f, 0.0f, abs(d - 0.70f));
        float halo = exp(-d * 2.8f) * 0.40f;

        float3 baseCol = input.col.rgb;
        float3 c = lerp(baseCol, float3(1.0f, 1.0f, 1.0f), core * 0.85f + ring * 0.45f);
        float a = saturate(rim * input.col.a + halo * input.col.a);
        return float4(c, a);
    }

    // Mode 3: Holographic Target Receptor Ring
    if (m < 3.5f) {
        if (d > 1.05f) discard;
        float ring1 = smoothstep(0.10f, 0.0f, abs(d - 0.86f));
        float ring2 = smoothstep(0.06f, 0.0f, abs(d - 0.58f));
        float pip   = 1.0f - smoothstep(0.10f, 0.22f, d);
        float lens  = (1.0f - smoothstep(0.55f, 0.88f, d)) * 0.22f;

        float tickX = step(abs(p.x), 0.04f) * step(0.60f, abs(p.y)) * step(abs(p.y), 0.92f);
        float tickY = step(abs(p.y), 0.04f) * step(0.60f, abs(p.x)) * step(abs(p.x), 0.92f);
        float ticks = max(tickX, tickY);

        float intensity = max(max(ring1 * 1.1f, ring2 * 0.75f), max(pip * 0.9f, ticks * 0.9f));
        float3 col = lerp(input.col.rgb * 0.85f, float3(1.0f, 1.0f, 1.0f), ring1 * 0.4f + pip * 0.6f);
        float alpha = saturate((intensity + lens) * input.col.a);
        return float4(col, alpha);
    }

    // Mode 4: Scratch Golden Ring
    if (m < 4.5f) {
        if (d > 1.05f) discard;
        float r1 = smoothstep(0.10f, 0.0f, abs(d - 0.88f));
        float r2 = smoothstep(0.06f, 0.0f, abs(d - 0.62f));
        float r3 = smoothstep(0.04f, 0.0f, abs(d - 0.35f));
        float ang = atan2(p.y, p.x);
        float rays = 0.85f + 0.15f * sin(ang * 12.0f);
        float intensity = max(max(r1, r2 * 0.7f), r3 * 0.5f) * rays;
        float3 gold = float3(1.0f, 0.88f, 0.25f);
        return float4(gold, intensity * input.col.a);
    }

    // Mode 5: Hit Shockwave & Flare
    if (m < 5.5f) {
        if (d > 1.15f) discard;
        float shock = smoothstep(0.14f, 0.0f, abs(d - 0.75f));
        float burst = exp(-d * 2.4f) * 0.8f;
        float tot = saturate(shock * 1.3f + burst);
        float3 col = lerp(input.col.rgb, float3(1.0f, 1.0f, 1.0f), shock * 0.5f + burst * 0.5f);
        return float4(col, tot * input.col.a);
    }

    // Mode 6: Center Rhythmic Hub
    if (m < 6.5f) {
        if (d > 1.05f) discard;
        float ring = smoothstep(0.08f, 0.0f, abs(d - 0.90f));
        float inner = (1.0f - smoothstep(0.0f, 0.90f, d)) * 0.25f;
        return float4(input.col.rgb, saturate(ring + inner) * input.col.a);
    }

    // Mode 7: Official Song Jacket Artwork
    if (m > 6.5f && m < 7.5f) {
        float4 tex = txJacket.Sample(samLinear, input.uv);
        return float4(tex.rgb * input.col.rgb, tex.a * input.col.a);
    }

    // Mode 8: Crisp Anti-Aliased Atlus Font
    if (m > 7.5f && m < 8.5f) {
        float fontAlpha = txFont.Sample(samLinear, input.uv).a;
        return float4(input.col.rgb, fontAlpha * input.col.a);
    }

    return input.col;
}
)";

// 3D Character Cel-Shading HLSL Shader
static const char* s_hlsl_source_3d = R"(
cbuffer ConstantBuffer3D : register(b0) {
    row_major matrix g_World;
    row_major matrix g_View;
    row_major matrix g_Proj;
    float4 g_LightDir;
    float4 g_Ambient;
    float4 g_MatMode; // x: 0=body, 1=face, 2=hair, 3=eyes, 4=outline
};

Texture2D txChar : register(t1);
SamplerState samLinear3D : register(s0);

struct VS_INPUT_3D {
    float3 pos  : POSITION;
    float3 norm : NORMAL;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
};

struct PS_INPUT_3D {
    float4 pos   : SV_POSITION;
    float3 norm  : NORMAL;
    float2 uv    : TEXCOORD0;
    float4 col   : COLOR;
    float3 world : TEXCOORD1;
};

PS_INPUT_3D VSMain3D(VS_INPUT_3D input) {
    PS_INPUT_3D output;
    float4 wPos = mul(float4(input.pos, 1.0f), g_World);
    float4 vPos = mul(wPos, g_View);
    output.pos   = mul(vPos, g_Proj);
    output.norm  = normalize(mul(input.norm, (float3x3)g_World));
    output.uv    = input.uv;
    output.col   = input.col;
    output.world = wPos.xyz;
    return output;
}

float4 PSMain3D(PS_INPUT_3D input) : SV_Target {
    // Mode 4: Atlus Dark Ink Cel Outline
    if (g_MatMode.x > 3.5f) {
        return float4(0.06f, 0.08f, 0.14f, 1.0f);
    }

    float4 texColor = txChar.Sample(samLinear3D, input.uv);

    // Mode 3: Anime Eyes - vibrant and crisp
    if (g_MatMode.x > 2.5f) {
        return float4(texColor.rgb, 1.0f);
    }

    // Shading computation
    float3 L = normalize(g_LightDir.xyz);
    float NdotL = dot(input.norm, L);
    float toon = 1.0f;

    // Mode 1: Face - soft, flattering anime skin shading
    if (g_MatMode.x > 0.5f && g_MatMode.x < 1.5f) {
        toon = smoothstep(-0.35f, 0.35f, NdotL) * 0.20f + 0.80f;
    } else {
        // Mode 0 & 2: Body & Hair - crisp Atlus two-tone cel-shading
        toon = smoothstep(-0.10f, 0.28f, NdotL) * 0.45f + 0.55f;
    }

    // Stylized Persona 3 Velvet electric blue rim lighting
    float3 V = normalize(-input.world);
    float rim = pow(saturate(1.0f - dot(input.norm, V)), 3.2f) * 0.38f;

    float3 finalRgb = (texColor.rgb * toon * g_Ambient.rgb) + float3(0.20f, 0.60f, 1.0f) * rim;
    return float4(finalRgb, 1.0f);
}
)";

// High-precision 5x7 ASCII Bitmap Font Table (ASCII 32 ' ' through 95 '_')
static const uint8_t s_font5x7[64][7] = {
    {0, 0, 0, 0, 0, 0, 0},        // 32 ' '
    {4, 4, 4, 4, 4, 0, 4},        // 33 '!'
    {10, 10, 10, 0, 0, 0, 0},     // 34 '"'
    {10, 10, 31, 10, 31, 10, 10}, // 35 '#'
    {4, 15, 20, 14, 5, 30, 4},    // 36 '$'
    {25, 25, 2, 4, 8, 19, 19},    // 37 '%'
    {12, 18, 20, 8, 21, 18, 13},  // 38 '&'
    {4, 4, 8, 0, 0, 0, 0},        // 39 '\''
    {2, 4, 8, 8, 8, 4, 2},        // 40 '('
    {8, 4, 2, 2, 2, 4, 8},        // 41 ')'
    {0, 4, 21, 14, 21, 4, 0},     // 42 '*'
    {0, 4, 4, 31, 4, 4, 0},       // 43 '+'
    {0, 0, 0, 0, 6, 4, 8},        // 44 ','
    {0, 0, 0, 31, 0, 0, 0},       // 45 '-'
    {0, 0, 0, 0, 0, 12, 12},      // 46 '.'
    {1, 2, 4, 8, 16, 0, 0},       // 47 '/'
    {14, 17, 19, 21, 25, 17, 14}, // 48 '0'
    {4, 12, 4, 4, 4, 4, 14},      // 49 '1'
    {14, 17, 1, 2, 4, 8, 31},     // 50 '2'
    {31, 2, 4, 2, 1, 17, 14},     // 51 '3'
    {2, 6, 10, 18, 31, 2, 2},     // 52 '4'
    {31, 16, 30, 1, 1, 17, 14},   // 53 '5'
    {6, 8, 16, 30, 17, 17, 14},   // 54 '6'
    {31, 1, 2, 4, 8, 8, 8},       // 55 '7'
    {14, 17, 17, 14, 17, 17, 14}, // 56 '8'
    {14, 17, 17, 15, 1, 2, 12},   // 57 '9'
    {0, 12, 12, 0, 12, 12, 0},    // 58 ':'
    {0, 12, 12, 0, 12, 4, 8},     // 59 ';'
    {2, 4, 8, 16, 8, 4, 2},       // 60 '<'
    {0, 31, 0, 31, 0, 0, 0},      // 61 '='
    {8, 4, 2, 1, 2, 4, 8},        // 62 '>'
    {14, 17, 1, 2, 4, 0, 4},      // 63 '?'
    {14, 17, 1, 13, 21, 21, 14},  // 64 '@'
    {14, 17, 17, 31, 17, 17, 17}, // 65 'A'
    {30, 17, 17, 30, 17, 17, 30}, // 66 'B'
    {14, 17, 16, 16, 16, 17, 14}, // 67 'C'
    {28, 18, 17, 17, 17, 18, 28}, // 68 'D'
    {31, 16, 16, 30, 16, 16, 31}, // 69 'E'
    {31, 16, 16, 30, 16, 16, 16}, // 70 'F'
    {14, 17, 16, 23, 17, 17, 14}, // 71 'G'
    {17, 17, 17, 31, 17, 17, 17}, // 72 'H'
    {14, 4, 4, 4, 4, 4, 14},      // 73 'I'
    {7, 2, 2, 2, 2, 18, 12},      // 74 'J'
    {17, 18, 20, 24, 20, 18, 17}, // 75 'K'
    {16, 16, 16, 16, 16, 16, 31}, // 76 'L'
    {17, 27, 21, 21, 17, 17, 17}, // 77 'M'
    {17, 25, 21, 19, 17, 17, 17}, // 78 'N'
    {14, 17, 17, 17, 17, 17, 14}, // 79 'O'
    {30, 17, 17, 30, 16, 16, 16}, // 80 'P'
    {14, 17, 17, 17, 21, 18, 13}, // 81 'Q'
    {30, 17, 17, 30, 20, 18, 17}, // 82 'R'
    {14, 17, 16, 14, 1, 17, 14},  // 83 'S'
    {31, 4, 4, 4, 4, 4, 4},       // 84 'T'
    {17, 17, 17, 17, 17, 17, 14}, // 85 'U'
    {17, 17, 17, 17, 17, 10, 4},  // 86 'V'
    {17, 17, 17, 21, 21, 27, 17}, // 87 'W'
    {17, 17, 10, 4, 10, 17, 17},  // 88 'X'
    {17, 17, 10, 4, 4, 4, 4},     // 89 'Y'
    {31, 1, 2, 4, 8, 16, 31},     // 90 'Z'
    {14, 8, 8, 8, 8, 8, 14},      // 91 '['
    {16, 8, 4, 2, 1, 0, 0},       // 92 '\\'
    {14, 2, 2, 2, 2, 2, 14},      // 93 ']'
    {4, 10, 17, 0, 0, 0, 0},      // 94 '^'
    {0, 0, 0, 0, 0, 0, 31}        // 95 '_'
};

static struct { float x, y; float r, g, b; const char* key; } s_lane_data[7] = {
    {-0.68f,  0.42f, 0.15f, 0.85f, 1.00f, "[W]"},     // 0: LeftUp
    {-0.80f,  0.00f, 0.15f, 0.85f, 1.00f, "[A]"},     // 1: LeftLeft
    {-0.68f, -0.42f, 0.15f, 0.85f, 1.00f, "[S]"},     // 2: LeftDown
    { 0.68f,  0.42f, 1.00f, 0.25f, 0.75f, "[I]"},     // 3: RightUp
    { 0.80f,  0.00f, 1.00f, 0.25f, 0.75f, "[L]"},     // 4: RightRight
    { 0.68f, -0.42f, 1.00f, 0.25f, 0.75f, "[K]"},     // 5: RightDown
    { 0.00f, -0.05f, 1.00f, 0.85f, 0.20f, "[SPACE]"}  // 6: Scratch
};

D3D11Renderer::~D3D11Renderer() {
    if (ds_state_ui_) ds_state_ui_->Release();
    if (ds_state_3d_) ds_state_3d_->Release();
    if (dsv_) dsv_->Release();
    if (depth_stencil_buffer_) depth_stencil_buffer_->Release();

    if (cb_3d_) cb_3d_->Release();
    for (int i = 0; i < 5; ++i) {
        if (char_srvs_[i]) { char_srvs_[i]->Release(); char_srvs_[i] = nullptr; }
        if (char_textures_[i]) { char_textures_[i]->Release(); char_textures_[i] = nullptr; }
    }
    if (char_ibo_) char_ibo_->Release();
    if (char_vbo_) char_vbo_->Release();
    if (input_layout_3d_) input_layout_3d_->Release();
    if (vs_3d_) vs_3d_->Release();
    if (ps_3d_) ps_3d_->Release();

    if (rasterizer_state_) rasterizer_state_->Release();
    if (blend_state_) blend_state_->Release();
    if (sampler_state_) sampler_state_->Release();
    if (jacket_srv_) { jacket_srv_->Release(); jacket_srv_ = nullptr; }
    if (jacket_texture_) { jacket_texture_->Release(); jacket_texture_ = nullptr; }
    if (font_srv_) { font_srv_->Release(); font_srv_ = nullptr; }
    if (font_texture_) { font_texture_->Release(); font_texture_ = nullptr; }
    if (bg_srv_) bg_srv_->Release();
    if (bg_texture_) bg_texture_->Release();
    if (vbo_) vbo_->Release();
    if (input_layout_) input_layout_->Release();
    if (vs_) vs_->Release();
    if (ps_) ps_->Release();
    if (rtv_) rtv_->Release();
    if (swapchain_) swapchain_->Release();
    if (context_) context_->Release();
    if (device_) device_->Release();
}

bool D3D11Renderer::initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width_;
    scd.BufferDesc.Height = height_;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd_;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL feature_level;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        2,
        D3D11_SDK_VERSION,
        &scd,
        &swapchain_,
        &device_,
        &feature_level,
        &context_
    );

    if (FAILED(hr)) {
        std::cerr << "[D3D11Renderer] Failed to create D3D11 device and swapchain\n";
        return false;
    }

    // Render target view
    ID3D11Texture2D* backbuffer = nullptr;
    swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);
    device_->CreateRenderTargetView(backbuffer, nullptr, &rtv_);
    backbuffer->Release();

    // Viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context_->RSSetViewports(1, &vp);

    if (!create_depth_stencil(width_, height_)) return false;
    if (!create_shaders()) return false;
    if (!create_states()) return false;
    if (!init_font_atlas()) return false;

    // Load extracted official background artwork
    load_background_texture("extracted_assets/textures/bg_p3d.rgba");

    // Load extracted official 3D Makoto Yuki character model and textures
    load_character_model("extracted_assets/models/p3/pc101/makoto_yuki.bin");

    batch_vertices_.reserve(16384);
    return true;
}

bool D3D11Renderer::create_depth_stencil(int width, int height) {
    if (dsv_) { dsv_->Release(); dsv_ = nullptr; }
    if (depth_stencil_buffer_) { depth_stencil_buffer_->Release(); depth_stencil_buffer_ = nullptr; }

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = width;
    dd.Height = height;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = device_->CreateTexture2D(&dd, nullptr, &depth_stencil_buffer_);
    if (FAILED(hr)) return false;

    hr = device_->CreateDepthStencilView(depth_stencil_buffer_, nullptr, &dsv_);
    if (FAILED(hr)) return false;

    // 3D Depth State (Depth enabled, write enabled)
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device_->CreateDepthStencilState(&dsd, &ds_state_3d_);

    // UI Depth State (Depth test disabled)
    D3D11_DEPTH_STENCIL_DESC uids = {};
    uids.DepthEnable = FALSE;
    uids.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    device_->CreateDepthStencilState(&uids, &ds_state_ui_);

    return true;
}

bool D3D11Renderer::create_shaders() {
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err_blob = nullptr;

    // 1. Compile 2D Shaders
    HRESULT hr = D3DCompile(s_hlsl_source, strlen(s_hlsl_source), "VS", nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) { std::cerr << "[D3D11Renderer] 2D VS Error: " << (char*)err_blob->GetBufferPointer() << "\n"; err_blob->Release(); }
        return false;
    }
    device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs_);

    hr = D3DCompile(s_hlsl_source, strlen(s_hlsl_source), "PS", nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) { std::cerr << "[D3D11Renderer] 2D PS Error: " << (char*)err_blob->GetBufferPointer() << "\n"; err_blob->Release(); }
        return false;
    }
    device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_);

    D3D11_INPUT_ELEMENT_DESC layout2d[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,          0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device_->CreateInputLayout(layout2d, 4, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout_);
    vs_blob->Release();
    ps_blob->Release();

    // 2. Compile 3D Character Shaders
    hr = D3DCompile(s_hlsl_source_3d, strlen(s_hlsl_source_3d), "VS3D", nullptr, nullptr, "VSMain3D", "vs_4_0", 0, 0, &vs_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) { std::cerr << "[D3D11Renderer] 3D VS Error: " << (char*)err_blob->GetBufferPointer() << "\n"; err_blob->Release(); }
        return false;
    }
    device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs_3d_);

    hr = D3DCompile(s_hlsl_source_3d, strlen(s_hlsl_source_3d), "PS3D", nullptr, nullptr, "PSMain3D", "ps_4_0", 0, 0, &ps_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) { std::cerr << "[D3D11Renderer] 3D PS Error: " << (char*)err_blob->GetBufferPointer() << "\n"; err_blob->Release(); }
        return false;
    }
    device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps_3d_);

    D3D11_INPUT_ELEMENT_DESC layout3d[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device_->CreateInputLayout(layout3d, 4, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout_3d_);
    vs_blob->Release();
    ps_blob->Release();

    // 2D dynamic vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex2D) * 32768;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&bd, nullptr, &vbo_);

    // 3D Constant Buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(ConstantBuffer3D);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&cbd, nullptr, &cb_3d_);

    return true;
}

bool D3D11Renderer::create_states() {
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState(&bd, &blend_state_);

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState(&sd, &sampler_state_);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    device_->CreateRasterizerState(&rd, &rasterizer_state_);

    return true;
}

bool D3D11Renderer::load_background_texture(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    const uint32_t tex_w = 1920;
    const uint32_t tex_h = 1080;
    std::vector<uint8_t> buffer(tex_w * tex_h * 4);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = tex_w;
    td.Height = tex_h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = buffer.data();
    init_data.SysMemPitch = tex_w * 4;

    HRESULT hr = device_->CreateTexture2D(&td, &init_data, &bg_texture_);
    if (SUCCEEDED(hr)) {
        device_->CreateShaderResourceView(bg_texture_, nullptr, &bg_srv_);
        std::cout << "[D3D11Renderer] Loaded official Persona 3 Dancing background texture (1920x1080 RGBA8)\n";
        return true;
    }
    return false;
}

bool D3D11Renderer::load_texture_rgba(const std::string& path, uint32_t width, uint32_t height, 
                                      ID3D11Texture2D** out_tex, ID3D11ShaderResourceView** out_srv) {
    std::ifstream tf(path, std::ios::binary);
    if (!tf) return false;

    std::vector<uint8_t> cbuf(width * height * 4);
    tf.read(reinterpret_cast<char*>(cbuf.data()), cbuf.size());

    D3D11_TEXTURE2D_DESC ctd = {};
    ctd.Width = width;
    ctd.Height = height;
    ctd.MipLevels = 1;
    ctd.ArraySize = 1;
    ctd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ctd.SampleDesc.Count = 1;
    ctd.Usage = D3D11_USAGE_IMMUTABLE;
    ctd.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA cdata = {};
    cdata.pSysMem = cbuf.data();
    cdata.SysMemPitch = width * 4;

    HRESULT hr = device_->CreateTexture2D(&ctd, &cdata, out_tex);
    if (SUCCEEDED(hr)) {
        device_->CreateShaderResourceView(*out_tex, nullptr, out_srv);
        return true;
    }
    return false;
}

bool D3D11Renderer::load_character_model(const std::string& bin_path) {
    std::ifstream bf(bin_path, std::ios::binary);
    if (!bf) {
        std::cerr << "[D3D11Renderer] 3D Model file not found: " << bin_path << "\n";
        return false;
    }

    char magic[4];
    bf.read(magic, 4);

    uint32_t v_count = 0, i_count = 0, stride = 0;
    char_submeshes_.clear();

    if (magic[0] == 'P' && magic[1] == '3' && magic[2] == 'D' && magic[3] == '2') {
        uint32_t sm_count = 0;
        bf.read(reinterpret_cast<char*>(&v_count), 4);
        bf.read(reinterpret_cast<char*>(&i_count), 4);
        bf.read(reinterpret_cast<char*>(&stride), 4);
        bf.read(reinterpret_cast<char*>(&sm_count), 4);

        char_submeshes_.resize(sm_count);
        for (uint32_t s = 0; s < sm_count; ++s) {
            bf.read(reinterpret_cast<char*>(&char_submeshes_[s].tex_id), 4);
            bf.read(reinterpret_cast<char*>(&char_submeshes_[s].index_start), 4);
            bf.read(reinterpret_cast<char*>(&char_submeshes_[s].index_count), 4);
        }
    } else if (magic[0] == 'P' && magic[1] == '3' && magic[2] == 'D' && magic[3] == 'M') {
        bf.read(reinterpret_cast<char*>(&v_count), 4);
        bf.read(reinterpret_cast<char*>(&i_count), 4);
        bf.read(reinterpret_cast<char*>(&stride), 4);
        char_submeshes_.push_back({0, 0, i_count});
    } else {
        std::cerr << "[D3D11Renderer] Invalid 3D Model format\n";
        return false;
    }

    std::vector<uint8_t> v_data(v_count * stride);
    bf.read(reinterpret_cast<char*>(v_data.data()), v_data.size());

    std::vector<uint32_t> i_data(i_count);
    bf.read(reinterpret_cast<char*>(i_data.data()), i_count * 4);

    // Create 3D Vertex Buffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.ByteWidth = static_cast<UINT>(v_data.size());
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsd = {};
    vsd.pSysMem = v_data.data();
    device_->CreateBuffer(&vbd, &vsd, &char_vbo_);

    // Create 3D Index Buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.ByteWidth = static_cast<UINT>(i_data.size() * sizeof(uint32_t));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isd = {};
    isd.pSysMem = i_data.data();
    device_->CreateBuffer(&ibd, &isd, &char_ibo_);

    // Load Character Textures (Slot 0=Body, 1=Face, 2=Hair, 3=Eyes)
    const std::string tex_folder = "extracted_assets/models/p3/pc101/";
    load_texture_rgba(tex_folder + "pc101_01_body00.rgba", 2048, 2048, &char_textures_[0], &char_srvs_[0]);
    load_texture_rgba(tex_folder + "pc101_f1_tex00.rgba", 1024, 1024, &char_textures_[1], &char_srvs_[1]);
    load_texture_rgba(tex_folder + "pc101_h00_tex00.rgba", 2048, 1024, &char_textures_[2], &char_srvs_[2]);
    load_texture_rgba(tex_folder + "pc101_e1_tex00.rgba", 512, 256, &char_textures_[3], &char_srvs_[3]);

    // Create 1x1 solid dark navy ink outline texture for slot 4
    uint32_t ink_color = 0xFF14100E; // RGBA dark ink
    D3D11_TEXTURE2D_DESC otd = {};
    otd.Width = 1;
    otd.Height = 1;
    otd.MipLevels = 1;
    otd.ArraySize = 1;
    otd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    otd.SampleDesc.Count = 1;
    otd.Usage = D3D11_USAGE_IMMUTABLE;
    otd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA odata = {};
    odata.pSysMem = &ink_color;
    odata.SysMemPitch = 4;
    device_->CreateTexture2D(&otd, &odata, &char_textures_[4]);
    if (char_textures_[4]) {
        device_->CreateShaderResourceView(char_textures_[4], nullptr, &char_srvs_[4]);
    }

    char_loaded_ = true;
    std::cout << "[D3D11Renderer] Successfully loaded Makoto Yuki 3D character (P3D2 format, "
              << v_count << " vertices, " << (i_count / 3) << " triangles, "
              << char_submeshes_.size() << " submeshes)\n";
    return true;
}

void D3D11Renderer::update_jacket_texture(int track_id) {
    if (track_id == current_jacket_id_ && jacket_srv_ != nullptr) return;

    if (jacket_srv_) { jacket_srv_->Release(); jacket_srv_ = nullptr; }
    if (jacket_texture_) { jacket_texture_->Release(); jacket_texture_ = nullptr; }

    std::string path = "extracted_assets/song_bg/bg_" + std::to_string(track_id) + ".rgba";
    if (load_texture_rgba(path, 512, 512, &jacket_texture_, &jacket_srv_)) {
        current_jacket_id_ = track_id;
    } else {
        current_jacket_id_ = -1;
    }
}

bool D3D11Renderer::init_font_atlas() {
    const int atlas_w = 512;
    const int atlas_h = 512;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = atlas_w;
    bmi.bmiHeader.biHeight = -atlas_h; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dib_pixels = nullptr;
    HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &dib_pixels, nullptr, 0);
    HGDIOBJ old_bmp = SelectObject(mem_dc, dib);

    // Create modern, sleek, bold sans-serif font
    HFONT font = CreateFontA(
        36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
    HGDIOBJ old_font = SelectObject(mem_dc, font);

    // Fill background black
    RECT all_rc = {0, 0, atlas_w, atlas_h};
    FillRect(mem_dc, &all_rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, RGB(255, 255, 255));

    // 16 columns by 8 rows = 128 cells
    const int cell_w = atlas_w / 16; // 32
    const int cell_h = atlas_h / 8;  // 64

    for (int ch = 32; ch < 127; ++ch) {
        int col = ch % 16;
        int row = ch / 16;
        int cx = col * cell_w;
        int cy = row * cell_h;

        char s[2] = {static_cast<char>(ch), '\0'};
        SIZE sz = {};
        GetTextExtentPoint32A(mem_dc, s, 1, &sz);

        // Center vertically and horizontally in cell
        int tx = cx + (cell_w - sz.cx) / 2;
        int ty = cy + (cell_h - sz.cy) / 2;
        TextOutA(mem_dc, tx, ty, s, 1);

        glyphs_[ch].u0 = static_cast<float>(cx) / atlas_w;
        glyphs_[ch].v0 = static_cast<float>(cy) / atlas_h;
        glyphs_[ch].u1 = static_cast<float>(cx + cell_w) / atlas_w;
        glyphs_[ch].v1 = static_cast<float>(cy + cell_h) / atlas_h;
        glyphs_[ch].adv = (ch == 32) ? 0.38f : std::max(0.40f, std::min(1.0f, (sz.cx + 4.0f) / static_cast<float>(cell_w)));
    }

    // Convert grayscale RGB into RGBA with alpha = brightness
    uint32_t* p = reinterpret_cast<uint32_t*>(dib_pixels);
    for (int i = 0; i < atlas_w * atlas_h; ++i) {
        uint32_t col = p[i];
        uint32_t r = (col >> 16) & 0xFF;
        uint32_t g = (col >> 8) & 0xFF;
        uint32_t b = col & 0xFF;
        uint32_t lum = (r * 2 + g * 5 + b) / 8; // Luminance
        p[i] = (lum << 24) | 0x00FFFFFF;       // White color, alpha = luminance
    }

    // Upload to D3D11 Texture2D
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = atlas_w;
    td.Height = atlas_h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = dib_pixels;
    sd.SysMemPitch = atlas_w * 4;

    HRESULT hr = device_->CreateTexture2D(&td, &sd, &font_texture_);
    if (SUCCEEDED(hr)) {
        device_->CreateShaderResourceView(font_texture_, nullptr, &font_srv_);
        std::cout << "[D3D11Renderer] Crisp Atlus Font Atlas generated successfully (512x512 anti-aliased Segoe UI)\n";
    }

    // GDI Cleanup
    SelectObject(mem_dc, old_font);
    DeleteObject(font);
    SelectObject(mem_dc, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);

    return (font_srv_ != nullptr);
}

void D3D11Renderer::resize(int width, int height) {
    if (swapchain_ && width > 0 && height > 0) {
        width_ = width;
        height_ = height;
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }

        swapchain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0);

        ID3D11Texture2D* backbuffer = nullptr;
        swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);
        device_->CreateRenderTargetView(backbuffer, nullptr, &rtv_);
        backbuffer->Release();

        create_depth_stencil(width_, height_);

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width_);
        vp.Height = static_cast<float>(height_);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        context_->RSSetViewports(1, &vp);
    }
}

void D3D11Renderer::begin_frame() {
    const float clear_color[4] = {0.02f, 0.04f, 0.10f, 1.0f};
    context_->OMSetRenderTargets(1, &rtv_, dsv_);
    context_->ClearRenderTargetView(rtv_, clear_color);
    if (dsv_) {
        context_->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context_->OMSetBlendState(blend_state_, blend_factor, 0xffffffff);
    context_->RSSetState(rasterizer_state_);

    context_->IASetInputLayout(input_layout_);
    context_->VSSetShader(vs_, nullptr, 0);
    context_->PSSetShader(ps_, nullptr, 0);

    if (bg_srv_) {
        context_->PSSetShaderResources(0, 1, &bg_srv_);
        context_->PSSetSamplers(0, 1, &sampler_state_);
    }
    if (jacket_srv_) {
        context_->PSSetShaderResources(1, 1, &jacket_srv_);
    }
    if (font_srv_) {
        context_->PSSetShaderResources(2, 1, &font_srv_);
    }

    batch_vertices_.clear();
}

void D3D11Renderer::push_quad(float x, float y, float w, float h, 
                              float r, float g, float b, float a, 
                              float mode, 
                              float u0, float v0, float u1, float v1) {
    if (batch_vertices_.size() + 6 >= 32768) {
        flush_batch();
    }

    Vertex2D v[6] = {
        {x - w, y - h, 0.0f, u0, v1, r, g, b, a, mode},
        {x - w, y + h, 0.0f, u0, v0, r, g, b, a, mode},
        {x + w, y - h, 0.0f, u1, v1, r, g, b, a, mode},
        {x + w, y - h, 0.0f, u1, v1, r, g, b, a, mode},
        {x - w, y + h, 0.0f, u0, v0, r, g, b, a, mode},
        {x + w, y + h, 0.0f, u1, v0, r, g, b, a, mode}
    };
    for (int i = 0; i < 6; ++i) {
        batch_vertices_.push_back(v[i]);
    }
}

void D3D11Renderer::push_circle(float cx, float cy, float radius, 
                                float r, float g, float b, float a, 
                                float mode) {
    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float w = radius / aspect;
    float h = radius;
    push_quad(cx, cy, w, h, r, g, b, a, mode, 0.0f, 0.0f, 1.0f, 1.0f);
}

void D3D11Renderer::push_line(float x0, float y0, float x1, float y1, 
                              float thickness, float r, float g, float b, float a) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0001f) return;

    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float nx = (-dy / len) * (thickness / aspect);
    float ny = ( dx / len) * thickness;

    if (batch_vertices_.size() + 6 >= 32768) {
        flush_batch();
    }

    Vertex2D v[6] = {
        {x0 - nx, y0 - ny, 0.0f, 0.0f, 1.0f, r, g, b, a, 0.0f},
        {x0 + nx, y0 + ny, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
        {x1 - nx, y1 - ny, 0.0f, 1.0f, 1.0f, r, g, b, a, 0.0f},
        {x1 - nx, y1 - ny, 0.0f, 1.0f, 1.0f, r, g, b, a, 0.0f},
        {x0 + nx, y0 + ny, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
        {x1 + nx, y1 + ny, 0.0f, 1.0f, 0.0f, r, g, b, a, 0.0f}
    };
    for (int i = 0; i < 6; ++i) batch_vertices_.push_back(v[i]);
}

void D3D11Renderer::draw_text(const std::string& text, float x, float y, float size, 
                              float r, float g, float b, float a, bool shadow) {
    if (!font_srv_) return;

    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float char_h = size * 6.8f;
    float char_w = char_h / aspect;

    auto render_pass = [&](float ox, float oy, float cr, float cg, float cb, float ca) {
        float cur_x = x + ox;
        for (char ch : text) {
            uint8_t uc = static_cast<uint8_t>(ch);
            if (uc < 32 || uc > 126) uc = 32;
            const auto& g = glyphs_[uc];
            float char_box_w = char_w * g.adv;
            float half_w = char_box_w * 0.5f;
            float half_h = char_h * 0.5f;
            push_quad(cur_x + half_w, y + oy, half_w, half_h, cr, cg, cb, ca, 8.0f, g.u0, g.v0, g.u1, g.v1);
            cur_x += char_box_w;
        }
    };

    if (shadow) {
        render_pass(char_w * 0.06f, -char_h * 0.06f, 0.00f, 0.01f, 0.04f, a * 0.90f);
    }
    render_pass(0.0f, 0.0f, r, g, b, a);
}

void D3D11Renderer::draw_glyph(int lane, float cx, float cy, float size, 
                               float r, float g, float b, float a) {
    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float s_x = size / aspect;
    float s_y = size;

    switch (lane) {
        case 0: // LeftUp -> Up Chevron
            push_line(cx - s_x * 0.5f, cy - s_y * 0.2f, cx, cy + s_y * 0.5f, 0.004f, r, g, b, a);
            push_line(cx + s_x * 0.5f, cy - s_y * 0.2f, cx, cy + s_y * 0.5f, 0.004f, r, g, b, a);
            break;
        case 1: // LeftLeft -> Left Chevron
            push_line(cx + s_x * 0.2f, cy + s_y * 0.5f, cx - s_x * 0.5f, cy, 0.004f, r, g, b, a);
            push_line(cx + s_x * 0.2f, cy - s_y * 0.5f, cx - s_x * 0.5f, cy, 0.004f, r, g, b, a);
            break;
        case 2: // LeftDown -> Down Chevron
            push_line(cx - s_x * 0.5f, cy + s_y * 0.2f, cx, cy - s_y * 0.5f, 0.004f, r, g, b, a);
            push_line(cx + s_x * 0.5f, cy + s_y * 0.2f, cx, cy - s_y * 0.5f, 0.004f, r, g, b, a);
            break;
        case 3: // RightUp -> PlayStation Triangle
            push_line(cx, cy + s_y * 0.55f, cx - s_x * 0.5f, cy - s_y * 0.35f, 0.004f, r, g, b, a);
            push_line(cx, cy + s_y * 0.55f, cx + s_x * 0.5f, cy - s_y * 0.35f, 0.004f, r, g, b, a);
            push_line(cx - s_x * 0.5f, cy - s_y * 0.35f, cx + s_x * 0.5f, cy - s_y * 0.35f, 0.004f, r, g, b, a);
            break;
        case 4: // RightRight -> PlayStation Circle
            push_circle(cx, cy, size * 0.45f, r, g, b, a * 0.9f, 6.0f);
            break;
        case 5: // RightDown -> PlayStation Cross
            push_line(cx - s_x * 0.4f, cy - s_y * 0.4f, cx + s_x * 0.4f, cy + s_y * 0.4f, 0.004f, r, g, b, a);
            push_line(cx - s_x * 0.4f, cy + s_y * 0.4f, cx + s_x * 0.4f, cy - s_y * 0.4f, 0.004f, r, g, b, a);
            break;
        default:
            break;
    }
}

void D3D11Renderer::flush_batch() {
    if (batch_vertices_.empty()) return;

    D3D11_MAPPED_SUBRESOURCE ms;
    HRESULT hr = context_->Map(vbo_, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    if (SUCCEEDED(hr)) {
        size_t bytes = batch_vertices_.size() * sizeof(Vertex2D);
        memcpy(ms.pData, batch_vertices_.data(), bytes);
        context_->Unmap(vbo_, 0);

        UINT stride = sizeof(Vertex2D);
        UINT offset = 0;
        context_->IASetVertexBuffers(0, 1, &vbo_, &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->Draw(static_cast<UINT>(batch_vertices_.size()), 0);
    }
    batch_vertices_.clear();
}

void D3D11Renderer::draw_star(float cx, float cy, float size, float r, float g, float b, float a, bool filled) {
    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float w = size / aspect;
    float h = size;

    if (filled) {
        if (batch_vertices_.size() + 6 >= 32768) flush_batch();
        Vertex2D v[6] = {
            {cx, cy + h, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
            {cx - w, cy, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
            {cx + w, cy, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
            {cx, cy - h, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
            {cx - w, cy, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f},
            {cx + w, cy, 0.0f, 0.0f, 0.0f, r, g, b, a, 0.0f}
        };
        for (int i = 0; i < 6; ++i) batch_vertices_.push_back(v[i]);
    } else {
        push_line(cx, cy + h, cx + w, cy, 0.0022f, r, g, b, a * 0.45f);
        push_line(cx + w, cy, cx, cy - h, 0.0022f, r, g, b, a * 0.45f);
        push_line(cx, cy - h, cx - w, cy, 0.0022f, r, g, b, a * 0.45f);
        push_line(cx - w, cy, cx, cy + h, 0.0022f, r, g, b, a * 0.45f);
    }
}

void D3D11Renderer::draw_background() {
    if (bg_srv_) {
        push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.88f, 0.88f, 0.95f, 0.95f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    } else {
        push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.03f, 0.08f, 0.20f, 1.0f, 0.0f);
    }
    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.02f, 0.04f, 0.10f, 0.35f, 0.0f);
    flush_batch(); // Flush background quad before rendering 3D character
}

void D3D11Renderer::draw_character_3d(double song_time_ms) {
    if (!char_loaded_ || !char_vbo_ || !char_ibo_ || char_submeshes_.empty()) return;

    // Enable Depth Stencil Testing for 3D Character
    context_->OMSetDepthStencilState(ds_state_3d_, 1);

    // Rhythm groove and head bobbing synced to music (BPM 145 = 413.79ms)
    float beat_phase = static_cast<float>(std::fmod(song_time_ms, 413.79) / 413.79);
    float dance_bob = std::sin(beat_phase * 6.2831853f) * 0.018f;
    float dance_sway = std::sin(static_cast<float>(song_time_ms * 0.0020)) * 0.06f;

    // World Transform: center model, scale to realistic stage perspective
    // Model bounds: X: [-74.77, 74.77], Y: [-0.09, 172.17], Z: [-14.32, 19.62]
    Matrix4x4 w = matrix_multiply(matrix_translation(0.0f, -86.0f, 0.0f), matrix_scale(0.0085f, 0.0085f, 0.0085f));
    w = matrix_multiply(w, matrix_rotation_y(3.14159265f + dance_sway));
    w = matrix_multiply(w, matrix_translation(0.0f, -0.32f + dance_bob, 3.85f));

    Matrix4x4 v = matrix_identity();
    float aspect = (height_ > 0) ? (static_cast<float>(width_) / height_) : (16.0f / 9.0f);
    Matrix4x4 p = matrix_perspective_fov_lh(0.785398f, aspect, 0.1f, 100.0f);

    // Set 3D Pipeline
    context_->IASetInputLayout(input_layout_3d_);
    context_->VSSetShader(vs_3d_, nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, &cb_3d_);
    context_->PSSetShader(ps_3d_, nullptr, 0);
    context_->PSSetConstantBuffers(0, 1, &cb_3d_);
    context_->PSSetSamplers(0, 1, &sampler_state_);

    UINT stride = sizeof(Vertex3D);
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, &char_vbo_, &stride, &offset);
    context_->IASetIndexBuffer(char_ibo_, DXGI_FORMAT_R32_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Render each submesh with its exact material slot and texture
    for (const auto& sm : char_submeshes_) {
        uint32_t tid = sm.tex_id;
        if (tid == 4) continue; // Skip inverted outline hull that covered Makoto with black ink
        if (tid > 4) tid = 0;

        // Update Constant Buffer with current submesh material mode
        D3D11_MAPPED_SUBRESOURCE ms;
        if (SUCCEEDED(context_->Map(cb_3d_, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            ConstantBuffer3D* cb = reinterpret_cast<ConstantBuffer3D*>(ms.pData);
            cb->world = w;
            cb->view = v;
            cb->proj = p;
            cb->light_dir[0] = 0.2f; cb->light_dir[1] = 0.8f; cb->light_dir[2] = -0.6f; cb->light_dir[3] = 0.0f;
            cb->ambient[0] = 1.0f;   cb->ambient[1] = 1.0f;   cb->ambient[2] = 1.08f;  cb->ambient[3] = 1.0f;
            cb->mat_mode[0] = static_cast<float>(tid);
            cb->mat_mode[1] = 0.0f; cb->mat_mode[2] = 0.0f; cb->mat_mode[3] = 0.0f;
            context_->Unmap(cb_3d_, 0);
        }

        if (char_srvs_[tid]) {
            context_->PSSetShaderResources(1, 1, &char_srvs_[tid]);
        }
        context_->DrawIndexed(sm.index_count, sm.index_start, 0);
    }

    // Restore 2D Pipeline for rhythm overlay
    context_->OMSetDepthStencilState(ds_state_ui_, 0);
    context_->IASetInputLayout(input_layout_);
    context_->VSSetShader(vs_, nullptr, 0);
    context_->PSSetShader(ps_, nullptr, 0);
    if (bg_srv_) {
        context_->PSSetShaderResources(0, 1, &bg_srv_);
    }
    if (jacket_srv_) {
        context_->PSSetShaderResources(1, 1, &jacket_srv_);
    }
    if (font_srv_) {
        context_->PSSetShaderResources(2, 1, &font_srv_);
    }
}

void D3D11Renderer::draw_radial_tracks(double song_time_ms) {
    // Elegant left and right track arcs (connecting the 3 left receptors and 3 right receptors)
    // Left Arc: LeftUp -> LeftLeft -> LeftDown
    push_line(s_lane_data[0].x, s_lane_data[0].y, s_lane_data[1].x, s_lane_data[1].y, 0.0035f, 0.0f, 0.75f, 1.0f, 0.50f);
    push_line(s_lane_data[1].x, s_lane_data[1].y, s_lane_data[2].x, s_lane_data[2].y, 0.0035f, 0.0f, 0.75f, 1.0f, 0.50f);

    // Right Arc: RightUp -> RightRight -> RightDown
    push_line(s_lane_data[3].x, s_lane_data[3].y, s_lane_data[4].x, s_lane_data[4].y, 0.0035f, 1.0f, 0.25f, 0.75f, 0.50f);
    push_line(s_lane_data[4].x, s_lane_data[4].y, s_lane_data[5].x, s_lane_data[5].y, 0.0035f, 1.0f, 0.25f, 0.75f, 0.50f);
}

void D3D11Renderer::draw_note_targets() {
    for (int i = 0; i < 6; ++i) {
        float x = s_lane_data[i].x;
        float y = s_lane_data[i].y;
        float r = s_lane_data[i].r;
        float g = s_lane_data[i].g;
        float b = s_lane_data[i].b;

        push_circle(x, y, 0.095f, r, g, b, 0.95f, 3.0f);
        draw_glyph(i, x, y, 0.045f, r, g, b, 0.90f);

        float label_x = (x < 0.0f) ? (x - 0.12f) : (x + 0.06f);
        draw_text(s_lane_data[i].key, label_x, y + 0.015f, 0.0048f, r, g, b, 0.85f);
    }
}

void D3D11Renderer::draw_notes(const rhythm::RhythmEngine& engine, double song_time_ms) {
    const double approach_time_ms = 1200.0;
    const float cx = 0.0f;
    const float cy = -0.05f;

    for (const auto& note : engine.notes()) {
        if (note.judged) continue;

        double time_until_hit = note.target_time_ms - song_time_ms;
        if (time_until_hit > approach_time_ms || time_until_hit < -rhythm::RhythmEngine::TIMING_GOOD_MS) {
            continue;
        }

        float t = static_cast<float>(1.0 - (time_until_hit / approach_time_ms));
        t = std::max(0.0f, std::min(1.0f, t));

        int lane_idx = static_cast<int>(note.lane);

        if (note.type == rhythm::NoteType::Scratch) {
            float scratch_radius = 0.08f + 0.70f * t;
            push_circle(cx, cy, scratch_radius, 1.0f, 0.85f, 0.2f, 0.95f, 4.0f);
        } else {
            float target_x = s_lane_data[lane_idx].x;
            float target_y = s_lane_data[lane_idx].y;

            float current_x = cx + (target_x - cx) * t;
            float current_y = cy + (target_y - cy) * t;

            float rad = 0.035f + 0.055f * t;
            float r = s_lane_data[lane_idx].r;
            float g = s_lane_data[lane_idx].g;
            float b = s_lane_data[lane_idx].b;

            push_circle(current_x, current_y, rad, r, g, b, 1.0f, 2.0f);
        }
    }
}

void D3D11Renderer::trigger_judgment_effect(rhythm::Judgment j, int lane, double song_time_ms) {
    active_effects_.push_back({j, lane, song_time_ms});
}

void D3D11Renderer::draw_effects(double song_time_ms) {
    const double effect_duration_ms = 450.0;

    active_effects_.erase(
        std::remove_if(active_effects_.begin(), active_effects_.end(), [&](const JudgmentEffect& e) {
            return (song_time_ms - e.trigger_time_ms) > effect_duration_ms;
        }),
        active_effects_.end()
    );

    for (const auto& e : active_effects_) {
        double elapsed = song_time_ms - e.trigger_time_ms;
        float progress = static_cast<float>(elapsed / effect_duration_ms);
        float alpha = 1.0f - progress;
        alpha = std::max(0.0f, std::min(1.0f, alpha));

        int lane_idx = e.lane % 7;
        float x = (lane_idx == 6) ? 0.0f : s_lane_data[lane_idx].x;
        float y = (lane_idx == 6) ? -0.05f : s_lane_data[lane_idx].y;

        float r = 1.0f, g = 1.0f, b = 1.0f;
        const char* j_badge = "MISS";
        if (e.judgment == rhythm::Judgment::Perfect) {
            r = 1.0f; g = 0.85f; b = 0.15f;
            j_badge = "PERFECT!!";
        } else if (e.judgment == rhythm::Judgment::Great) {
            r = 0.15f; g = 0.95f; b = 0.40f;
            j_badge = "GREAT!";
        } else if (e.judgment == rhythm::Judgment::Good) {
            r = 0.20f; g = 0.70f; b = 1.00f;
            j_badge = "GOOD";
        } else {
            r = 0.95f; g = 0.15f; b = 0.20f;
            j_badge = "MISS...";
        }

        float shock_rad = (lane_idx == 6) ? (0.78f + 0.15f * progress) : (0.095f + 0.11f * progress);
        push_circle(x, y, shock_rad, r, g, b, alpha * 0.90f, 5.0f);

        float text_y = (lane_idx == 6) ? 0.15f : (y + 0.09f + 0.03f * progress);
        draw_text(j_badge, x - 0.07f, text_y, 0.0070f, r, g, b, alpha);
    }
}

void D3D11Renderer::draw_hud(const rhythm::RhythmStats& stats, double song_time_ms, double song_duration_ms,
                             const std::string& track_title, float bpm, int track_id, const std::string& diff_name) {
    // 1. Top HUD Header Bar
    push_quad(0.0f, 0.880f, 1.0f, 0.055f, 0.02f, 0.05f, 0.14f, 0.92f, 0.0f);
    push_quad(0.0f, 0.825f, 1.0f, 0.0025f, 0.00f, 0.85f, 1.00f, 0.85f, 0.0f);

    std::string title_line = "P3D  " + track_title;
    draw_text(title_line, -0.92f, 0.905f, 0.0044f, 0.95f, 0.98f, 1.0f, 1.0f);

    char info_buf[128];
    snprintf(info_buf, sizeof(info_buf), "BPM: %3.0f  |  TRACK: %02d  |  %s", bpm, track_id, diff_name.c_str());
    draw_text(info_buf, -0.92f, 0.855f, 0.0032f, 0.35f, 0.85f, 1.0f, 0.90f);

    char score_buf[64];
    snprintf(score_buf, sizeof(score_buf), "SCORE  %08u", stats.score);
    draw_text(score_buf, 0.44f, 0.905f, 0.0050f, 1.0f, 0.88f, 0.25f, 1.0f);

    char fever_buf[64];
    if (stats.fever_active) {
        snprintf(fever_buf, sizeof(fever_buf), "FEVER ACTIVE! 2X");
        draw_text(fever_buf, 0.44f, 0.855f, 0.0034f, 1.0f, 0.20f, 0.80f, 1.0f);
    } else {
        snprintf(fever_buf, sizeof(fever_buf), "FEVER CHARGE  %3.0f%%", stats.fever_gauge);
        draw_text(fever_buf, 0.44f, 0.855f, 0.0032f, 1.0f, 0.80f, 0.20f, 0.95f);
    }

    // 2. Persona 3 Dancing SEES Groove Gauge (Hype Meter)
    float gg_w = 0.30f;
    float gg_y = 0.770f;
    push_quad(0.0f, gg_y, gg_w, 0.009f, 0.02f, 0.04f, 0.10f, 0.85f, 0.0f);
    push_quad(0.0f, gg_y + 0.008f, gg_w, 0.0015f, 0.20f, 0.60f, 0.90f, 0.80f, 0.0f);
    push_quad(0.0f, gg_y - 0.008f, gg_w, 0.0015f, 0.20f, 0.60f, 0.90f, 0.80f, 0.0f);

    float fill_pct = std::max(0.0f, std::min(100.0f, stats.groove_gauge)) / 100.0f;
    float fill_w = gg_w * fill_pct;
    float gr = 0.0f, gg = 0.85f, gb = 1.0f;
    if (stats.groove_gauge < 30.0f) { gr = 0.95f; gg = 0.20f; gb = 0.25f; }
    else if (stats.groove_gauge >= 70.0f) { gr = 0.00f; gg = 0.95f; gb = 0.45f; }
    push_quad(-gg_w + fill_w, gg_y, fill_w, 0.0070f, gr, gg, gb, 0.95f, 0.0f);

    // Clear mark indicator at 50%
    push_line(0.0f, gg_y - 0.012f, 0.0f, gg_y + 0.012f, 0.002f, 1.0f, 0.85f, 0.20f, 0.90f);
    draw_text("[CLEAR]", -0.038f, gg_y - 0.016f, 0.0024f, 1.0f, 0.85f, 0.20f, 0.85f);
    draw_text("[S.E.E.S.]", -gg_w - 0.08f, gg_y + 0.006f, 0.0026f, 0.00f, 0.85f, 1.00f, 0.90f);

    // 3. Fever Time Pulsing Banner (during Fever mode)
    if (stats.fever_active) {
        float f_pulse = (std::sin(static_cast<float>(song_time_ms * 0.008)) + 1.0f) * 0.5f;
        draw_text("★ ★ FEVER TIME!! (2x SCORE) ★ ★", -0.32f, 0.69f, 0.0050f, 1.0f, 0.80f + 0.20f * f_pulse, 0.20f, 0.95f);
    }

    // 4. Song timeline progress bar at top edge
    float progress = (song_duration_ms > 0.0) ? static_cast<float>(song_time_ms / song_duration_ms) : 0.0f;
    progress = std::max(0.0f, std::min(1.0f, progress));
    push_quad(0.0f, 0.940f, 1.0f, 0.004f, 0.04f, 0.08f, 0.16f, 0.80f, 0.0f);
    float prog_w = progress;
    push_quad(-1.0f + prog_w, 0.940f, prog_w, 0.004f, 0.00f, 0.85f, 1.0f, 0.95f, 0.0f);

    // 5. Dynamic Combo Counter
    if (stats.combo > 0) {
        char combo_buf[64];
        snprintf(combo_buf, sizeof(combo_buf), "%u COMBO", stats.combo);
        float combo_scale = 0.007f + std::min(0.0025f, (stats.combo % 10) * 0.00025f);
        draw_text(combo_buf, -0.10f, 0.28f, combo_scale, 0.95f, 0.98f, 1.0f, 0.95f);
    }
}

void D3D11Renderer::render_pause_menu(int selected_option) {
    // Frosted glass dark overlay
    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.01f, 0.02f, 0.05f, 0.72f, 0.0f);

    // Slanted Atlus Pause Box
    float pcx = 0.0f, pcy = 0.05f;
    push_quad(pcx, pcy, 0.36f, 0.42f, 0.03f, 0.08f, 0.20f, 0.96f, 0.0f);
    push_quad(pcx, pcy + 0.418f, 0.36f, 0.004f, 0.00f, 0.85f, 1.00f, 1.0f, 0.0f);
    push_quad(pcx, pcy - 0.418f, 0.36f, 0.004f, 0.00f, 0.85f, 1.00f, 1.0f, 0.0f);

    // Header
    draw_text("PAUSED", -0.11f, 0.35f, 0.0075f, 1.00f, 0.88f, 0.20f, 1.0f);
    push_line(-0.28f, 0.28f, 0.28f, 0.28f, 0.0025f, 0.20f, 0.75f, 1.00f, 0.70f);

    const char* options[3] = {
        "RESUME DANCE",
        "RESTART TRACK",
        "QUIT TO SONG SELECT"
    };

    for (int i = 0; i < 3; ++i) {
        float oy = 0.14f - i * 0.14f;
        bool is_sel = (i == selected_option);

        if (is_sel) {
            push_quad(pcx, oy, 0.32f, 0.052f, 0.08f, 0.35f, 0.75f, 0.95f, 0.0f);
            push_quad(pcx, oy + 0.050f, 0.32f, 0.0025f, 0.00f, 0.95f, 1.00f, 1.0f, 0.0f);
            push_quad(pcx, oy - 0.050f, 0.32f, 0.0025f, 0.00f, 0.95f, 1.00f, 1.0f, 0.0f);
            draw_text(">>", -0.28f, oy + 0.015f, 0.0046f, 1.00f, 0.88f, 0.20f, 1.0f);
            draw_text(options[i], -0.20f, oy + 0.015f, 0.0044f, 1.00f, 1.00f, 1.0f, 1.0f);
        } else {
            push_quad(pcx, oy, 0.30f, 0.045f, 0.02f, 0.05f, 0.12f, 0.70f, 0.0f);
            draw_text(options[i], -0.20f, oy + 0.015f, 0.0040f, 0.50f, 0.65f, 0.85f, 0.80f);
        }
    }

    draw_text("[W/S or UP/DOWN] SELECT   |   [ENTER] CONFIRM   |   [ESC] RESUME",
              -0.34f, -0.26f, 0.0030f, 0.40f, 0.75f, 0.95f, 0.85f);

    flush_batch();
}

void D3D11Renderer::render_results_screen(const rhythm::RhythmStats& stats, const std::string& track_title, const std::string& diff_name) {
    draw_background();

    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.02f, 0.04f, 0.10f, 0.88f, 0.0f);

    push_quad(0.0f, 0.0f, 0.52f, 0.78f, 0.04f, 0.09f, 0.22f, 0.95f, 0.0f);
    push_quad(0.0f,  0.77f, 0.52f, 0.005f, 1.0f, 0.85f, 0.2f, 1.0f, 0.0f);
    push_quad(0.0f, -0.77f, 0.52f, 0.005f, 1.0f, 0.85f, 0.2f, 1.0f, 0.0f);

    draw_text("STAGE RESULTS", -0.19f, 0.71f, 0.0075f, 0.2f, 0.85f, 1.0f, 1.0f);

    if (!track_title.empty()) {
        std::string song_sub = track_title + "  [" + diff_name + "]";
        draw_text(song_sub, -0.38f, 0.63f, 0.0042f, 1.0f, 0.85f, 0.25f, 0.95f);
    }

    float acc = stats.accuracy();
    float rank_r = 1.0f, rank_g = 0.85f, rank_b = 0.15f;
    const char* rank_text = "KING CRAZY";

    if (acc >= 95.0f && stats.miss_count == 0) {
        rank_text = "KING CRAZY";
        rank_r = 1.0f; rank_g = 0.85f; rank_b = 0.15f;
    } else if (acc >= 85.0f) {
        rank_text = "BRILLIANT";
        rank_r = 0.25f; rank_g = 0.85f; rank_b = 1.0f;
    } else if (acc >= 65.0f) {
        rank_text = "STAGE CLEARED";
        rank_r = 0.20f; rank_g = 0.95f; rank_b = 0.40f;
    } else {
        rank_text = "STAGE FAILED";
        rank_r = 0.95f; rank_g = 0.20f; rank_b = 0.20f;
    }

    push_quad(0.0f, 0.56f, 0.42f, 0.06f, rank_r * 0.25f, rank_g * 0.25f, rank_b * 0.25f, 0.90f, 0.0f);
    draw_text(rank_text, -0.17f, 0.58f, 0.0085f, rank_r, rank_g, rank_b, 1.0f);

    char buf[128];
    snprintf(buf, sizeof(buf), "FINAL SCORE:   %u", stats.score);
    draw_text(buf, -0.38f, 0.42f, 0.0055f, 1.0f, 0.88f, 0.25f, 1.0f);

    snprintf(buf, sizeof(buf), "MAX COMBO:     %u", stats.max_combo);
    draw_text(buf, -0.38f, 0.33f, 0.0055f, 0.95f, 0.95f, 1.0f, 1.0f);

    snprintf(buf, sizeof(buf), "ACCURACY:      %5.1f%%", acc);
    draw_text(buf, -0.38f, 0.24f, 0.0055f, 0.30f, 0.90f, 1.0f, 1.0f);

    snprintf(buf, sizeof(buf), "PERFECT:       %u", stats.perfect_count);
    draw_text(buf, -0.38f, 0.12f, 0.0050f, 1.0f, 0.85f, 0.20f, 1.0f);

    snprintf(buf, sizeof(buf), "GREAT:         %u", stats.great_count);
    draw_text(buf, -0.38f, 0.03f, 0.0050f, 0.20f, 0.95f, 0.40f, 1.0f);

    snprintf(buf, sizeof(buf), "GOOD:          %u", stats.good_count);
    draw_text(buf, -0.38f, -0.06f, 0.0050f, 0.20f, 0.70f, 1.0f, 1.0f);

    snprintf(buf, sizeof(buf), "MISS:          %u", stats.miss_count);
    draw_text(buf, -0.38f, -0.15f, 0.0050f, 0.95f, 0.20f, 0.20f, 1.0f);

    push_quad(0.0f, -0.62f, 0.44f, 0.045f, 0.08f, 0.18f, 0.38f, 0.90f, 0.0f);
    draw_text("[R / X] PLAY AGAIN      [ENTER / B / ESC] SONG SELECT", -0.38f, -0.605f, 0.0044f, 0.90f, 0.95f, 1.0f, 1.0f);

    flush_batch();
}

void D3D11Renderer::render_title_screen(double anim_time_ms) {
    // 1. Background with deep Club Velvet tint
    draw_background();

    // 2. Ambient Velvet Room club lighting
    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.01f, 0.03f, 0.08f, 0.50f, 0.0f);

    // Dynamic sweeping laser beams
    float beam_sway = std::sin(static_cast<float>(anim_time_ms * 0.0018f)) * 0.4f;
    push_line(-1.0f, -0.8f + beam_sway, 0.8f, 1.0f, 0.003f, 0.00f, 0.85f, 1.00f, 0.35f);
    push_line(-0.8f, -1.0f, 1.0f, 0.8f - beam_sway, 0.003f, 0.20f, 0.50f, 1.00f, 0.25f);

    // Luminous Concentric Moon Vinyl Hub in Center
    float pulse = (std::sin(static_cast<float>(anim_time_ms * 0.004f)) + 1.0f) * 0.5f;
    push_circle(0.0f, 0.12f, 0.36f + 0.02f * pulse, 0.05f, 0.25f, 0.65f, 0.30f, 6.0f);
    push_circle(0.0f, 0.12f, 0.26f, 0.00f, 0.80f, 1.00f, 0.55f, 3.0f);
    push_circle(0.0f, 0.12f, 0.06f, 1.00f, 0.85f, 0.20f, 0.70f, 2.0f);

    // Atlus Presents Header
    draw_text("ATLUS SOUND TEAM PRESENTS", -0.22f, 0.58f, 0.0040f, 1.00f, 0.85f, 0.25f, 0.95f);

    // Persona 3: Dancing in Moonlight Title
    draw_text("PERSONA 3", -0.28f, 0.44f, 0.0090f, 1.00f, 1.00f, 1.00f, 1.0f);
    draw_text("DANCING IN MOONLIGHT", -0.42f, 0.32f, 0.0062f, 0.00f, 0.90f, 1.00f, 1.0f);
    draw_text("PC NATIVE RECOMPILATION", -0.26f, 0.22f, 0.0036f, 0.30f, 0.95f, 0.55f, 0.95f);

    // Center Pulsing Prompt
    float p_glow = 0.45f + 0.55f * pulse;
    push_quad(0.0f, -0.32f, 0.46f, 0.052f, 0.02f, 0.06f, 0.16f, 0.88f, 0.0f);
    push_quad(0.0f, -0.32f + 0.050f, 0.46f, 0.0025f, 0.00f, 0.85f, 1.00f, p_glow, 0.0f);
    push_quad(0.0f, -0.32f - 0.050f, 0.46f, 0.0025f, 0.00f, 0.85f, 1.00f, p_glow, 0.0f);

    draw_text(">>  PRESS ANY BUTTON TO DANCE  <<", -0.44f, -0.305f, 0.0046f, 1.00f, 0.90f, 0.25f, p_glow);
    draw_text("[ ENTER  /  SPACE  /  GAMEPAD START ]", -0.34f, -0.40f, 0.0032f, 0.45f, 0.75f, 0.95f, 0.80f);

    // Bottom System / Legal Bar
    push_quad(0.0f, -0.925f, 1.0f, 0.065f, 0.02f, 0.05f, 0.14f, 0.95f, 0.0f);
    push_quad(0.0f, -0.860f, 1.0f, 0.0025f, 0.00f, 0.85f, 1.00f, 0.90f, 0.0f);
    draw_text("(C) ATLUS  (C) SEGA   |   STANDALONE WIN32 x64 DIRECTX 11 PORT   |   V1.0 FINAL",
              -0.66f, -0.915f, 0.0030f, 0.50f, 0.65f, 0.85f, 0.85f);

    flush_batch();
}

void D3D11Renderer::render_song_select(const rhythm::TrackCatalog& catalog, size_t selected_index, int selected_diff, double anim_time_ms) {
    // 1. Draw official background
    draw_background();

    // 2. Persona 3 Electric Velvet Club Ambient Overlays
    // Midnight gradient giving an authentic PS4 club atmosphere
    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.01f, 0.03f, 0.08f, 0.40f, 0.0f);
    
    // Diagonal decorative Atlus cyan club light beams in the background
    push_line(-1.0f, -0.6f, 0.2f, 1.0f, 0.003f, 0.00f, 0.70f, 1.00f, 0.18f);
    push_line(-0.8f, -1.0f, 0.5f, 1.0f, 0.002f, 0.00f, 0.50f, 0.90f, 0.12f);

    // 3. Top Header Banner
    push_quad(0.0f, 0.915f, 1.0f, 0.065f, 0.02f, 0.05f, 0.14f, 0.96f, 0.0f);
    push_quad(0.0f, 0.850f, 1.0f, 0.003f, 0.00f, 0.85f, 1.00f, 0.95f, 0.0f);
    draw_text("PERSONA 3: DANCING IN MOONLIGHT", -0.92f, 0.942f, 0.0050f, 0.95f, 0.98f, 1.0f, 1.0f);
    draw_text("TRACK SELECTION  --  25 OFFICIAL TRACKS", -0.92f, 0.888f, 0.0034f, 0.35f, 0.85f, 1.0f, 0.95f);
    draw_text("AUDIO: [OFFICIAL PS4 MASTER]   |   DIRECTX 11 NATIVE", 0.18f, 0.888f, 0.0032f, 0.45f, 0.75f, 0.95f, 0.85f);

    size_t total_tracks = catalog.count();
    const auto& cur = catalog.get_track(selected_index);

    // 4. Update and Bind Selected Album Jacket Artwork
    update_jacket_texture(cur.id);
    if (jacket_srv_) {
        context_->PSSetShaderResources(1, 1, &jacket_srv_);
    }

    // 5. Left Side: Song Carousel Card Wheel
    // Backdrop panel
    push_quad(-0.54f, -0.05f, 0.42f, 0.78f, 0.02f, 0.05f, 0.12f, 0.82f, 0.0f);
    push_quad(-0.54f, 0.73f, 0.42f, 0.003f, 0.00f, 0.85f, 1.00f, 0.90f, 0.0f);
    push_quad(-0.54f, -0.83f, 0.42f, 0.003f, 0.00f, 0.85f, 1.00f, 0.90f, 0.0f);

    if (total_tracks > 0) {
        int visible_half = 3;
        for (int off = -visible_half; off <= visible_half; ++off) {
            int idx = static_cast<int>(selected_index) + off;
            idx = (idx % static_cast<int>(total_tracks) + static_cast<int>(total_tracks)) % static_cast<int>(total_tracks);
            const auto& t = catalog.get_track(idx);

            float y = -0.05f - off * 0.205f;
            if (y > 0.70f || y < -0.80f) continue;

            if (off == 0) {
                // Active Selected Song Card
                push_quad(-0.54f, y, 0.40f, 0.078f, 0.06f, 0.26f, 0.60f, 0.95f, 0.0f);
                push_quad(-0.54f, y + 0.076f, 0.40f, 0.003f, 0.00f, 0.95f, 1.00f, 1.0f, 0.0f);
                push_quad(-0.54f, y - 0.076f, 0.40f, 0.003f, 0.00f, 0.95f, 1.00f, 1.0f, 0.0f);
                // Left glowing indicator bar
                push_quad(-0.935f, y, 0.005f, 0.076f, 1.00f, 0.85f, 0.20f, 1.0f, 0.0f);

                char num_buf[16];
                snprintf(num_buf, sizeof(num_buf), ">> %02d", idx + 1);
                draw_text(num_buf, -0.91f, y + 0.030f, 0.0044f, 1.00f, 0.88f, 0.20f, 1.0f);

                std::string title_short = t.title.substr(0, 19);
                draw_text(title_short, -0.76f, y + 0.030f, 0.0044f, 1.00f, 1.00f, 1.00f, 1.0f);

                std::string artist_short = t.artist.substr(0, 24);
                draw_text(artist_short, -0.76f, y - 0.030f, 0.0030f, 0.35f, 0.85f, 1.00f, 0.95f);

                std::string dancer_tag = "[" + t.dancer + "]";
                draw_text(dancer_tag, -0.38f, y - 0.030f, 0.0028f, 1.00f, 0.65f, 0.95f, 0.90f);
            } else {
                float dim = 1.0f - std::abs(off) * 0.25f;
                // Unselected Song Card
                push_quad(-0.54f, y, 0.38f, 0.058f, 0.02f, 0.05f, 0.12f, 0.65f * dim, 0.0f);

                char num_buf[16];
                snprintf(num_buf, sizeof(num_buf), "   %02d", idx + 1);
                draw_text(num_buf, -0.91f, y + 0.016f, 0.0036f, 0.40f * dim, 0.65f * dim, 0.90f * dim, dim);

                std::string title_short = t.title.substr(0, 20);
                draw_text(title_short, -0.76f, y + 0.016f, 0.0036f, 0.75f * dim, 0.85f * dim, 0.95f * dim, dim);

                std::string artist_short = t.artist.substr(0, 24);
                draw_text(artist_short, -0.76f, y - 0.022f, 0.0026f, 0.30f * dim, 0.60f * dim, 0.80f * dim, dim * 0.8f);
            }
        }
    }

    // 6. Right Side: Album Jacket Artwork and Track Details
    float aspect = (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    float r_cx = 0.45f;

    // Right background panel
    push_quad(r_cx, -0.05f, 0.46f, 0.78f, 0.02f, 0.05f, 0.14f, 0.88f, 0.0f);
    push_quad(r_cx, 0.73f, 0.46f, 0.003f, 1.00f, 0.85f, 0.20f, 1.0f, 0.0f);
    push_quad(r_cx, -0.83f, 0.46f, 0.003f, 1.00f, 0.85f, 0.20f, 1.0f, 0.0f);

    // Official Album Jacket Card (Square in 16:9)
    float jh = 0.26f;
    float jw = jh / aspect;
    float jx = r_cx;
    float jy = 0.44f;

    // Outer glow / shadow
    push_quad(jx + 0.008f, jy - 0.008f, jw + 0.008f, jh + 0.008f, 0.0f, 0.0f, 0.0f, 0.65f, 0.0f);
    push_quad(jx, jy, jw + 0.006f, jh + 0.006f, 0.00f, 0.85f, 1.00f, 0.95f, 0.0f);

    // Render the 512x512 Atlus Album Jacket Quad!
    if (jacket_srv_) {
        push_quad(jx, jy, jw, jh, 1.0f, 1.0f, 1.0f, 1.0f, 7.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    } else {
        push_quad(jx, jy, jw, jh, 0.04f, 0.10f, 0.22f, 1.0f, 0.0f);
    }

    // Ribbon over jacket top
    push_quad(jx - jw * 0.45f, jy + jh - 0.022f, jw * 0.52f, 0.022f, 0.02f, 0.08f, 0.22f, 0.92f, 0.0f);
    char badge[32];
    snprintf(badge, sizeof(badge), "TRACK #%02zu", selected_index + 1);
    draw_text(badge, jx - jw * 0.92f, jy + jh - 0.009f, 0.0034f, 1.0f, 0.85f, 0.20f, 1.0f);

    // Track Metadata
    float text_left = 0.03f;
    std::string card_title = cur.title;
    draw_text(card_title, text_left, 0.12f, 0.0048f, 1.00f, 1.00f, 1.00f, 1.0f);

    std::string artist_line = "ARTIST: " + cur.artist;
    draw_text(artist_line, text_left, 0.05f, 0.0034f, 0.35f, 0.85f, 1.00f, 0.95f);

    std::string dancer_line = "LEAD DANCER: " + cur.dancer;
    draw_text(dancer_line, text_left, -0.01f, 0.0034f, 1.00f, 0.65f, 0.95f, 0.95f);

    char bpm_buf[64];
    int dur_sec = static_cast<int>(cur.duration_ms / 1000.0);
    snprintf(bpm_buf, sizeof(bpm_buf), "BPM: %3.0f   |   LENGTH: %02d:%02d", cur.bpm, dur_sec / 60, dur_sec % 60);
    draw_text(bpm_buf, text_left, -0.07f, 0.0034f, 0.25f, 0.95f, 0.50f, 0.95f);

    // Difficulty Header
    push_quad(r_cx, -0.155f, 0.42f, 0.002f, 0.20f, 0.80f, 1.00f, 0.70f, 0.0f);
    draw_text("SELECT DIFFICULTY:", text_left, -0.19f, 0.0038f, 1.00f, 0.88f, 0.25f, 1.0f);

    struct DiffConfig {
        const char* name;
        float r, g, b;
        int stars;
    };
    DiffConfig diffs[4] = {
        {"EASY",      0.00f, 0.85f, 1.00f, 1},
        {"NORMAL",    0.00f, 0.90f, 0.40f, 3},
        {"HARD",      1.00f, 0.75f, 0.10f, 4},
        {"ALL NIGHT", 1.00f, 0.15f, 0.60f, 5}
    };

    for (int d = 0; d < 4; ++d) {
        float dy = -0.27f - d * 0.115f;
        bool is_sel = (d == selected_diff);

        if (is_sel) {
            push_quad(r_cx, dy, 0.42f, 0.046f, diffs[d].r * 0.28f, diffs[d].g * 0.28f, diffs[d].b * 0.28f, 0.95f, 0.0f);
            push_quad(r_cx, dy + 0.044f, 0.42f, 0.0025f, diffs[d].r, diffs[d].g, diffs[d].b, 1.0f, 0.0f);
            push_quad(r_cx, dy - 0.044f, 0.42f, 0.0025f, diffs[d].r, diffs[d].g, diffs[d].b, 1.0f, 0.0f);
            push_quad(text_left - 0.005f, dy, 0.004f, 0.044f, diffs[d].r, diffs[d].g, diffs[d].b, 1.0f, 0.0f);

            draw_text(">>", text_left + 0.015f, dy + 0.015f, 0.0042f, diffs[d].r, diffs[d].g, diffs[d].b, 1.0f);
            draw_text(diffs[d].name, text_left + 0.08f, dy + 0.015f, 0.0042f, 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            push_quad(r_cx, dy, 0.42f, 0.040f, 0.02f, 0.05f, 0.12f, 0.70f, 0.0f);
            draw_text(diffs[d].name, text_left + 0.08f, dy + 0.015f, 0.0038f, diffs[d].r * 0.75f, diffs[d].g * 0.75f, diffs[d].b * 0.75f, 0.80f);
        }

        // Draw 5 Difficulty Diamond Stars
        for (int s = 0; s < 5; ++s) {
            float sx = 0.38f + s * 0.030f;
            bool filled = (s < diffs[d].stars);
            draw_star(sx, dy + 0.002f, 0.013f, diffs[d].r, diffs[d].g, diffs[d].b, filled ? 1.0f : 0.35f, filled);
        }

        // High Score / Cleared Record
        uint32_t hs = cur.high_score[d];
        if (hs > 0) {
            char hs_buf[64];
            snprintf(hs_buf, sizeof(hs_buf), "%u [%s]", hs, cur.best_rank[d].c_str());
            draw_text(hs_buf, 0.58f, dy + 0.015f, 0.0032f, 1.0f, 0.85f, 0.20f, 1.0f);
        } else {
            draw_text("NO RECORD", 0.62f, dy + 0.015f, 0.0030f, 0.40f, 0.50f, 0.65f, 0.65f);
        }
    }

    // 7. Bottom Navigation Guide Bar
    push_quad(0.0f, -0.925f, 1.0f, 0.065f, 0.02f, 0.05f, 0.14f, 0.95f, 0.0f);
    push_quad(0.0f, -0.860f, 1.0f, 0.0025f, 0.00f, 0.85f, 1.00f, 0.90f, 0.0f);
    draw_text("[W/S or UP/DOWN] TRACK   |   [A/D or 1..4] DIFFICULTY   |   [ENTER or SPACE] DANCE!   |   [ESC] QUIT",
              -0.78f, -0.915f, 0.0032f, 0.90f, 0.95f, 1.0f, 1.0f);

    flush_batch();
}

void D3D11Renderer::render_rhythm_scene(const rhythm::RhythmEngine& engine, double song_time_ms, double song_duration_ms,
                                        const std::string& track_title, float bpm, int track_id, const std::string& diff_name) {
    // 1. Draw official 3D stage background (Paulownia Mall, Club Escapade, etc.)
    update_jacket_texture(track_id);
    if (jacket_srv_) {
        context_->PSSetShaderResources(1, 1, &jacket_srv_);
        push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.95f, 0.95f, 1.0f, 1.0f, 7.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    } else {
        draw_background();
    }
    // Atmospheric dark velvet club tint
    push_quad(0.0f, 0.0f, 1.0f, 1.0f, 0.01f, 0.03f, 0.08f, 0.20f, 0.0f);

    // 3D stage illuminated neon dance podium
    push_circle(0.0f, -0.62f, 0.52f, 0.05f, 0.35f, 0.85f, 0.40f, 6.0f);
    push_circle(0.0f, -0.62f, 0.38f, 0.00f, 0.85f, 1.00f, 0.55f, 3.0f);
    flush_batch(); // Flush background & floor before 3D character pass

    // 2. Dynamic club lighting effects based on song beat
    float beat_ms = static_cast<float>(60000.0 / std::max(60.0f, bpm));
    float beat_phase = static_cast<float>(std::fmod(song_time_ms, beat_ms) / beat_ms);
    float pulse = std::sin(beat_phase * 3.14159f);
    float beam_sway = std::sin(static_cast<float>(song_time_ms * 0.0018f)) * 0.35f;

    // Laser sweeps and club spotlights
    push_line(-1.0f, -0.6f + beam_sway, 0.4f, 1.0f, 0.004f, 0.00f, 0.85f, 1.00f, 0.18f + 0.16f * pulse);
    push_line(1.0f, -0.6f - beam_sway, -0.4f, 1.0f, 0.004f, 1.00f, 0.20f, 0.80f, 0.18f + 0.16f * pulse);

    // 3. Draw 3D Makoto Yuki Character in the center stage dancing to the beat!
    draw_character_3d(song_time_ms);

    // 4. Draw 2D rhythm gameplay overlay (tracks, receptors, notes, effects, HUD)
    draw_radial_tracks(song_time_ms);
    draw_note_targets();
    draw_notes(engine, song_time_ms);
    draw_effects(song_time_ms);
    draw_hud(engine.stats(), song_time_ms, song_duration_ms, track_title, bpm, track_id, diff_name);

    flush_batch();
}

void D3D11Renderer::end_frame() {
    swapchain_->Present(1, 0); // VSync 60Hz lock
}

} // namespace p3d::renderer
