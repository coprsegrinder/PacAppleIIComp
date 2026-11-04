#include "d3drenderer.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "glow.h"

namespace pacman::app {

using Microsoft::WRL::ComPtr;

namespace {

const char* kShader = R"(
Texture2D tex : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut vs(uint id : SV_VertexID) {
    float2 p = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.pos = float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = p;
    return o;
}
float4 ps(VSOut i) : SV_Target { return float4(tex.Sample(smp, i.uv).rgb, 1.0); }

// A background drawn in code (the CRT bezel, the desktop): the image of the window's size, pixel for pixel.
Texture2D artTex : register(t3);
float4 psart(VSOut i) : SV_Target { return float4(artTex.Load(int3(i.pos.xy, 0)).rgb, 1.0); }

// The ambient glow of the bars: the colour of the nearest point of the picture's edge (from the coarse grid, smoothly interpolated), faded with the distance from the edge.
cbuffer Glow : register(b0) { float4 vp; float4 prm; };   // vp = left, top, right, bottom of the picture in pixels; prm = strength, fade length, blend width, zoom (screen pixels per game pixel)
Texture2D glowTex : register(t1);   // the blurred copy of the game picture (280 x 192)
Texture2D edgeTex : register(t2);   // 280 x 4: the picture's own edge colours: top, bottom, left, right
SamplerState lin : register(s1);
SamplerState mir : register(s2);   // the backdrop repeats outwards, mirrored at its edges
float4 psbg(VSOut i) : SV_Target {
    float2 p = i.pos.xy;
    float2 q = clamp(p, vp.xy, vp.zw);
    float2 dv = p - q;
    float d = length(dv);
    // the backdrop: the game picture, repeated outwards (mirrored at its edges) and a little smaller than the picture itself, centred behind it: the shapes and the movement of the game, far in the background
    float2 bu = float2(0.5, 0.5) + (p - (vp.xy + vp.zw) * 0.5) / (float2(280.0, 192.0) * prm.w);
    float3 c = glowTex.Sample(mir, bu).rgb;
    float m = max(c.r, max(c.g, c.b));
    float lift = min(1.0, m * 5.0) / max(m, 0.0001) * smoothstep(0.004, 0.03, m);   // dim content shows its colour, empty parts stay black
    float3 g = c * lift * prm.x * (0.35 + 0.65 * exp(-d / prm.y));                    // faint, and a little fainter the further from the picture
    bool vert = abs(dv.y) >= abs(dv.x);                                               // above or below the picture: its top / bottom edge; at its sides: the left / right edge
    float row = vert ? (dv.y < 0.0 ? 0.0 : 1.0) : (dv.x < 0.0 ? 2.0 : 3.0);
    float ex = vert ? lerp(0.5, 279.5, saturate((q.x - vp.x) / (vp.z - vp.x))) : lerp(0.5, 191.5, saturate((q.y - vp.y) / (vp.w - vp.y)));
    float3 e = edgeTex.Sample(lin, float2(ex / 280.0, (row + 0.5) / 4.0)).rgb;      // what the picture itself has right there
    float w = exp(-0.5 * d * d / (prm.z * prm.z));                                    // next to the picture: the picture's own colour, so there is no step; further out: the backdrop
    float n = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;          // 1/255 of noise: the slow fades do not show steps
    return float4(saturate(lerp(g, e, w) + n / 255.0), 1.0);
}
// The CRT filter (crtfilter.h is the CPU model of exactly this): one pass over the picture's viewport.
cbuffer Crt : register(b1) { float4 ca; float4 cb; float4 cc; };   // ca = viewport width, height, whole-number scale, texture width; cb = scan, sigma, tau, glow; cc = blend, curvature, gain, offset
float3 srcLoad(float x, int row) {
    int ix = clamp((int)floor(x * ca.w / 280.0 + 0.002), 0, (int)ca.w - 1);
    return tex.Load(int3(ix, row, 0)).rgb;
}
float4 pscrt(VSOut i) : SV_Target {
    float W = ca.x, H = ca.y;
    float ccx = i.uv.x * 2.0 - 1.0, ccy = i.uv.y * 2.0 - 1.0;
    if (cc.y > 0.0) {
        float nx = ccx * (1.0 + cc.y * ccy * ccy), ny = ccy * (1.0 + cc.y * ccx * ccx);
        ccx = nx;
        ccy = ny;
    }
    float m = saturate((1.0 - abs(ccx)) * W * 0.5 + 0.5) * saturate((1.0 - abs(ccy)) * H * 0.5 + 0.5);
    if (m <= 0.0) return float4(0.0, 0.0, 0.0, 1.0);
    float gx = (ccx * 0.5 + 0.5) * 280.0, gy = (ccy * 0.5 + 0.5) * 192.0;
    int row = clamp((int)floor(gy), 0, 191);
    float fr = gy - floor(gy);
    float scan = 1.0;
    if (ca.z >= 3.0) scan = (1.0 - cb.x) + cb.x * (0.5 - 0.5 * cos(6.2831853 * fr));
    else if (ca.z >= 2.0) scan = fr < 0.5 ? 1.0 : 1.0 - cb.x;
    float3 acc = float3(0.0, 0.0, 0.0);
    float sw = 0.0;
    for (int t = -8; t <= 8; t++) {
        float d = (float)t * 0.25;
        float wt = cb.y > 0.0 ? exp(-d * d / (2.0 * cb.y * cb.y)) : (t == 0 ? 1.0 : 0.0);
        if (cb.z > 0.0 && t < 0) wt += 0.25 * exp(d / cb.z);
        if (wt > 0.0) {
            float3 c = srcLoad(gx + d, row);
            acc += wt * c * c;
            sw += wt;
        }
    }
    float3 col = acc / sw;
    if (cc.x > 0.0) {
        float3 bl = float3(0.0, 0.0, 0.0);
        for (int e = -2; e <= 2; e++) {
            float3 c = srcLoad(gx + (float)e * 0.5, row);
            float3 l = c * c;
            float y = dot(l, float3(0.299, 0.587, 0.114));
            bl += (float)(3 - abs(e)) * (l - y);
        }
        float yc = dot(col, float3(0.299, 0.587, 0.114));
        col += cc.x * (bl / 9.0 - (col - yc));
    }
    col *= scan;
    if (cb.w > 0.0) {
        float3 g = float3(0.0, 0.0, 0.0);
        float gw = 0.0;
        for (int j = -2; j <= 2; j++)
            for (int k = -2; k <= 2; k++) {
                float wt = exp(-(float)(k * k + j * j) / (2.0 * 1.44));
                float3 c = tex.SampleLevel(lin, float2((gx + (float)k) / 280.0, (gy + (float)j) / 192.0), 0).rgb;
                g += wt * c * c;
                gw += wt;
            }
        col += g / gw * cb.w;
    }
    float3 o = saturate(sqrt(max(col, 0.0)) * cc.z + cc.w);
    return float4(o * m, 1.0);
}
)";

}  // namespace

struct D3dRenderer::Impl {
    HWND hwnd = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> picture, mono;
    ComPtr<ID3D11ShaderResourceView> pictureView, monoView;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps, psBackground, psArt, psCrt;
    ComPtr<ID3D11Buffer> crtConstants;
    bool forceCrt = false;
    ComPtr<ID3D11Texture2D> art;
    ComPtr<ID3D11ShaderResourceView> artView;
    int artW = 0, artH = 0;
    std::uint64_t artVersion = 0;
    ComPtr<ID3D11Texture2D> glow, edge;
    ComPtr<ID3D11ShaderResourceView> glowView, edgeView;
    ComPtr<ID3D11SamplerState> linear, mirror;
    ComPtr<ID3D11Buffer> glowConstants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11Texture2D> staging;
    bool warp = false;
    int w = 0, h = 0;

    bool makeTarget() {
        rtv.Reset();
        ComPtr<ID3D11Texture2D> back;
        if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
        if (FAILED(device->CreateRenderTargetView(back.Get(), nullptr, &rtv))) return false;
        D3D11_TEXTURE2D_DESC d{};
        back->GetDesc(&d);
        w = static_cast<int>(d.Width);
        h = static_cast<int>(d.Height);
        staging.Reset();
        return true;
    }

    bool createDevice() {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        for (int pass = 0; pass < 2; ++pass) {
            const D3D_DRIVER_TYPE type = pass == 0 ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
            if (SUCCEEDED(D3D11CreateDevice(nullptr, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &device, nullptr, &context))) {
                warp = pass == 1;
                return true;
            }
        }
        return false;
    }
};

D3dRenderer::D3dRenderer() : d_(std::make_unique<Impl>()) {}
D3dRenderer::~D3dRenderer() = default;

bool D3dRenderer::software() const noexcept { return d_->warp; }
int D3dRenderer::width() const noexcept { return d_->w; }
int D3dRenderer::height() const noexcept { return d_->h; }

bool D3dRenderer::init(HWND hwnd) {
    Impl& d = *d_;
    d.hwnd = hwnd;
    if (!d.createDevice()) return false;
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(d.device.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 0;   // the client area
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Scaling = DXGI_SCALING_NONE;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (FAILED(factory->CreateSwapChainForHwnd(d.device.Get(), hwnd, &sd, nullptr, nullptr, &d.swap))) return false;
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);   // Alt+Enter is the window's own hotkey
    if (!d.makeTarget()) return false;

    ComPtr<ID3DBlob> vsBlob, psBlob, err;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "vs", "vs_4_0", 0, 0, &vsBlob, &err))) return false;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "ps", "ps_4_0", 0, 0, &psBlob, &err))) return false;
    if (FAILED(d.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &d.vs))) return false;
    if (FAILED(d.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &d.ps))) return false;
    ComPtr<ID3DBlob> artBlob;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "psart", "ps_4_0", 0, 0, &artBlob, &err))) return false;
    if (FAILED(d.device->CreatePixelShader(artBlob->GetBufferPointer(), artBlob->GetBufferSize(), nullptr, &d.psArt))) return false;
    ComPtr<ID3DBlob> bgBlob;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "psbg", "ps_4_0", 0, 0, &bgBlob, &err))) return false;
    if (FAILED(d.device->CreatePixelShader(bgBlob->GetBufferPointer(), bgBlob->GetBufferSize(), nullptr, &d.psBackground))) return false;

    ComPtr<ID3DBlob> crtBlob;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "pscrt", "ps_4_0", 0, 0, &crtBlob, &err))) return false;
    if (FAILED(d.device->CreatePixelShader(crtBlob->GetBufferPointer(), crtBlob->GetBufferSize(), nullptr, &d.psCrt))) return false;
    {
        D3D11_BUFFER_DESC kb{};
        kb.ByteWidth = 48;
        kb.Usage = D3D11_USAGE_DYNAMIC;
        kb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        kb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(d.device->CreateBuffer(&kb, nullptr, &d.crtConstants))) return false;
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = kGameW;
    td.Height = kGameH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(d.device->CreateTexture2D(&td, nullptr, &d.picture))) return false;
    if (FAILED(d.device->CreateShaderResourceView(d.picture.Get(), nullptr, &d.pictureView))) return false;
    D3D11_TEXTURE2D_DESC md = td;   // the monochrome picture: 560 dot positions across
    md.Width = 560;
    if (FAILED(d.device->CreateTexture2D(&md, nullptr, &d.mono))) return false;
    if (FAILED(d.device->CreateShaderResourceView(d.mono.Get(), nullptr, &d.monoView))) return false;

    D3D11_SAMPLER_DESC ss{};
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;   // nearest neighbour
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ss.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(d.device->CreateSamplerState(&ss, &d.sampler))) return false;
    D3D11_TEXTURE2D_DESC gd = td;   // the blurred copy of the picture behind the picture (ambient glow)
    if (FAILED(d.device->CreateTexture2D(&gd, nullptr, &d.glow))) return false;
    if (FAILED(d.device->CreateShaderResourceView(d.glow.Get(), nullptr, &d.glowView))) return false;
    D3D11_TEXTURE2D_DESC ed = td;   // the picture's own edge colours
    ed.Width = kEdgeW;
    ed.Height = 4;
    if (FAILED(d.device->CreateTexture2D(&ed, nullptr, &d.edge))) return false;
    if (FAILED(d.device->CreateShaderResourceView(d.edge.Get(), nullptr, &d.edgeView))) return false;
    D3D11_SAMPLER_DESC ls = ss;
    ls.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // smooth between the cells
    if (FAILED(d.device->CreateSamplerState(&ls, &d.linear))) return false;
    ls.AddressU = ls.AddressV = ls.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    if (FAILED(d.device->CreateSamplerState(&ls, &d.mirror))) return false;
    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = 32;
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(d.device->CreateBuffer(&cb, nullptr, &d.glowConstants))) return false;
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = TRUE;
    if (FAILED(d.device->CreateRasterizerState(&rs, &d.raster))) return false;
    return true;
}

void D3dRenderer::resize(int clientW, int clientH) {
    Impl& d = *d_;
    if (!d.swap || clientW <= 0 || clientH <= 0 || (clientW == d.w && clientH == d.h)) return;
    d.context->OMSetRenderTargets(0, nullptr, nullptr);
    d.rtv.Reset();
    d.staging.Reset();
    if (SUCCEEDED(d.swap->ResizeBuffers(0, static_cast<UINT>(clientW), static_cast<UINT>(clientH), DXGI_FORMAT_UNKNOWN, 0))) d.makeTarget();
}

void D3dRenderer::forceCrtShader(bool on) { d_->forceCrt = on; }

bool D3dRenderer::draw(const std::uint32_t* argb, const Viewport& vp, const BackgroundFrame& bg, const std::uint32_t* mono560, const CrtFilter* crt) {
    Impl& d = *d_;
    if (!d.swap || !d.rtv) return false;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(d.context->Map(d.picture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return false;
    for (int y = 0; y < kGameH; ++y) std::memcpy(static_cast<char*>(m.pData) + static_cast<std::size_t>(y) * m.RowPitch, argb + static_cast<std::size_t>(y) * kGameW, kGameW * 4);
    d.context->Unmap(d.picture.Get(), 0);
    if (mono560) {
        D3D11_MAPPED_SUBRESOURCE mm{};
        if (FAILED(d.context->Map(d.mono.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mm))) return false;
        for (int y = 0; y < kGameH; ++y) std::memcpy(static_cast<char*>(mm.pData) + static_cast<std::size_t>(y) * mm.RowPitch, mono560 + static_cast<std::size_t>(y) * 560, 560 * 4);
        d.context->Unmap(d.mono.Get(), 0);
    }

    const float black[4] = {0, 0, 0, 1};
    ID3D11RenderTargetView* rt = d.rtv.Get();
    d.context->OMSetRenderTargets(1, &rt, nullptr);
    d.context->ClearRenderTargetView(d.rtv.Get(), black);
    if (bg.kind == Background::AmbientGlow && bg.backdropBgra && !vp.empty()) {   // the glow: one pass over the whole window; the picture is drawn over its middle afterwards
        D3D11_MAPPED_SUBRESOURCE gm{};
        if (SUCCEEDED(d.context->Map(d.glow.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &gm))) {
            for (int y = 0; y < kGameH; ++y) std::memcpy(static_cast<char*>(gm.pData) + static_cast<std::size_t>(y) * gm.RowPitch, bg.backdropBgra + static_cast<std::size_t>(y) * kGameW * 4, kGameW * 4);
            d.context->Unmap(d.glow.Get(), 0);
        }
        if (bg.edgeBgra) {
            D3D11_MAPPED_SUBRESOURCE em{};
            if (SUCCEEDED(d.context->Map(d.edge.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &em))) {
                for (int y = 0; y < 4; ++y) std::memcpy(static_cast<char*>(em.pData) + static_cast<std::size_t>(y) * em.RowPitch, bg.edgeBgra + static_cast<std::size_t>(y) * kEdgeW * 4, kEdgeW * 4);
                d.context->Unmap(d.edge.Get(), 0);
            }
        }
        D3D11_MAPPED_SUBRESOURCE cm{};
        if (SUCCEEDED(d.context->Map(d.glowConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cm))) {
            const float side = static_cast<float>(std::min(vp.w, vp.h));
            const float strength = 0.15f;          // far in the background: at most 15 % of the colour
            const float reach = 0.80f * side;      // a little fainter the further from the picture
            const float blend = 0.03f * side;      // the picture's own edge colours give way to the backdrop over about this many pixels
            const float zoom = static_cast<float>(GlowField::kBackdropScale * static_cast<double>(vp.w) / kGameW);   // screen pixels per game pixel of the repeated copies: a little less than the picture's own
            const float c[8] = {static_cast<float>(vp.x), static_cast<float>(vp.y), static_cast<float>(vp.x + vp.w), static_cast<float>(vp.y + vp.h), strength, reach, blend, zoom};
            std::memcpy(cm.pData, c, sizeof(c));
            d.context->Unmap(d.glowConstants.Get(), 0);
        }
        D3D11_VIEWPORT whole{0.0f, 0.0f, static_cast<float>(d.w), static_cast<float>(d.h), 0.0f, 1.0f};
        d.context->RSSetViewports(1, &whole);
        d.context->RSSetState(d.raster.Get());
        d.context->IASetInputLayout(nullptr);
        d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d.context->VSSetShader(d.vs.Get(), nullptr, 0);
        d.context->PSSetShader(d.psBackground.Get(), nullptr, 0);
        ID3D11Buffer* cbuf = d.glowConstants.Get();
        d.context->PSSetConstantBuffers(0, 1, &cbuf);
        ID3D11ShaderResourceView* gsrv[2] = {d.glowView.Get(), d.edgeView.Get()};
        d.context->PSSetShaderResources(1, 2, gsrv);
        ID3D11SamplerState* lsmp[2] = {d.linear.Get(), d.mirror.Get()};
        d.context->PSSetSamplers(1, 2, lsmp);
        d.context->Draw(3, 0);
    }
    if ((bg.kind == Background::CrtBezel || bg.kind == Background::Desktop) && bg.artArgb && bg.artWidth == d.w && bg.artHeight == d.h) {   // a background drawn in code: its image, then the picture over its middle
        if (!d.art || d.artW != bg.artWidth || d.artH != bg.artHeight) {
            D3D11_TEXTURE2D_DESC ad{};
            ad.Width = static_cast<UINT>(bg.artWidth);
            ad.Height = static_cast<UINT>(bg.artHeight);
            ad.MipLevels = 1;
            ad.ArraySize = 1;
            ad.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            ad.SampleDesc.Count = 1;
            ad.Usage = D3D11_USAGE_DYNAMIC;
            ad.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ad.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            d.art.Reset();
            d.artView.Reset();
            d.artVersion = 0;
            if (SUCCEEDED(d.device->CreateTexture2D(&ad, nullptr, &d.art))) d.device->CreateShaderResourceView(d.art.Get(), nullptr, &d.artView);
            d.artW = bg.artWidth;
            d.artH = bg.artHeight;
        }
        if (d.art && d.artView) {
            if (d.artVersion != bg.artVersion) {
                D3D11_MAPPED_SUBRESOURCE am{};
                if (SUCCEEDED(d.context->Map(d.art.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &am))) {
                    for (int y = 0; y < bg.artHeight; ++y) std::memcpy(static_cast<char*>(am.pData) + static_cast<std::size_t>(y) * am.RowPitch, bg.artArgb + static_cast<std::size_t>(y) * static_cast<std::size_t>(bg.artWidth), static_cast<std::size_t>(bg.artWidth) * 4);
                    d.context->Unmap(d.art.Get(), 0);
                    d.artVersion = bg.artVersion;
                }
            }
            D3D11_VIEWPORT whole{0.0f, 0.0f, static_cast<float>(d.w), static_cast<float>(d.h), 0.0f, 1.0f};
            d.context->RSSetViewports(1, &whole);
            d.context->RSSetState(d.raster.Get());
            d.context->IASetInputLayout(nullptr);
            d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            d.context->VSSetShader(d.vs.Get(), nullptr, 0);
            d.context->PSSetShader(d.psArt.Get(), nullptr, 0);
            ID3D11ShaderResourceView* asrv = d.artView.Get();
            d.context->PSSetShaderResources(3, 1, &asrv);
            d.context->Draw(3, 0);
        }
    }
    if (!vp.empty()) {
        D3D11_VIEWPORT v{static_cast<float>(vp.x), static_cast<float>(vp.y), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f};
        d.context->RSSetViewports(1, &v);
        d.context->RSSetState(d.raster.Get());
        d.context->IASetInputLayout(nullptr);
        d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d.context->VSSetShader(d.vs.Get(), nullptr, 0);
        const bool useCrt = crt && (d.forceCrt || !crt->shaderNeutral());   // every option neutral: the plain nearest-neighbour shader, the picture exactly as before
        d.context->PSSetShader(useCrt ? d.psCrt.Get() : d.ps.Get(), nullptr, 0);
        if (useCrt) {
            const CrtParams q = crtParams(*crt);
            D3D11_MAPPED_SUBRESOURCE km{};
            if (SUCCEEDED(d.context->Map(d.crtConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &km))) {
                const float c[12] = {static_cast<float>(vp.w), static_cast<float>(vp.h), static_cast<float>(vp.scale), mono560 ? 560.0f : 280.0f, q.scan, q.sigma, q.tau, q.glow, q.blend, q.curv, q.gain, q.offset};
                std::memcpy(km.pData, c, sizeof(c));
                d.context->Unmap(d.crtConstants.Get(), 0);
            }
            ID3D11Buffer* kbuf = d.crtConstants.Get();
            d.context->PSSetConstantBuffers(1, 1, &kbuf);
            ID3D11SamplerState* lsmp = d.linear.Get();
            d.context->PSSetSamplers(1, 1, &lsmp);
        }
        ID3D11ShaderResourceView* srv = mono560 ? d.monoView.Get() : d.pictureView.Get();
        d.context->PSSetShaderResources(0, 1, &srv);
        ID3D11SamplerState* smp = d.sampler.Get();
        d.context->PSSetSamplers(0, 1, &smp);
        d.context->Draw(3, 0);
    }
    return true;
}

bool D3dRenderer::present() {
    Impl& d = *d_;
    if (!d.swap) return false;
    const HRESULT hr = d.swap->Present(0, 0);
    return SUCCEEDED(hr);
}

bool D3dRenderer::readBack(std::vector<std::uint32_t>& out, int& w, int& h) {
    Impl& d = *d_;
    if (!d.swap || !d.rtv) return false;
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(d.swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    D3D11_TEXTURE2D_DESC bd{};
    back->GetDesc(&bd);
    if (!d.staging) {
        D3D11_TEXTURE2D_DESC sd = bd;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        if (FAILED(d.device->CreateTexture2D(&sd, nullptr, &d.staging))) return false;
    }
    d.context->CopyResource(d.staging.Get(), back.Get());
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(d.context->Map(d.staging.Get(), 0, D3D11_MAP_READ, 0, &m))) return false;
    w = static_cast<int>(bd.Width);
    h = static_cast<int>(bd.Height);
    out.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) std::memcpy(out.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w), static_cast<const char*>(m.pData) + static_cast<std::size_t>(y) * m.RowPitch, static_cast<std::size_t>(w) * 4);
    d.context->Unmap(d.staging.Get(), 0);
    return true;
}

}  // namespace pacman::app
