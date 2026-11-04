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

// The ambient glow of the bars: the colour of the nearest point of the picture's edge (from the coarse grid, smoothly interpolated), faded with the distance from the edge.
cbuffer Glow : register(b0) { float4 vp; float4 prm; };   // vp = left, top, right, bottom of the picture in pixels; prm = strength, fall-off length in pixels
Texture2D glowTex : register(t1);
SamplerState lin : register(s1);
float4 psbg(VSOut i) : SV_Target {
    float2 p = i.pos.xy;
    float2 q = clamp(p, vp.xy, vp.zw);
    float d = length(p - q);
    float2 uv = saturate((q - vp.xy) / (vp.zw - vp.xy));
    float3 c = glowTex.Sample(lin, uv).rgb;
    float a = prm.x * exp(-d / prm.y);
    float n = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;   // 1/255 of noise: the slow fade does not show steps
    return float4(saturate(c * a + n / 255.0), 1.0);
}
)";

}  // namespace

struct D3dRenderer::Impl {
    HWND hwnd = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> picture;
    ComPtr<ID3D11ShaderResourceView> pictureView;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps, psBackground;
    ComPtr<ID3D11Texture2D> glow;
    ComPtr<ID3D11ShaderResourceView> glowView;
    ComPtr<ID3D11SamplerState> linear;
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
    ComPtr<ID3DBlob> bgBlob;
    if (FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "psbg", "ps_4_0", 0, 0, &bgBlob, &err))) return false;
    if (FAILED(d.device->CreatePixelShader(bgBlob->GetBufferPointer(), bgBlob->GetBufferSize(), nullptr, &d.psBackground))) return false;

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

    D3D11_SAMPLER_DESC ss{};
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;   // nearest neighbour
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ss.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(d.device->CreateSamplerState(&ss, &d.sampler))) return false;
    D3D11_TEXTURE2D_DESC gd = td;   // the coarse colours of the ambient glow
    gd.Width = kGlowW;
    gd.Height = kGlowH;
    if (FAILED(d.device->CreateTexture2D(&gd, nullptr, &d.glow))) return false;
    if (FAILED(d.device->CreateShaderResourceView(d.glow.Get(), nullptr, &d.glowView))) return false;
    D3D11_SAMPLER_DESC ls = ss;
    ls.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // smooth between the cells
    if (FAILED(d.device->CreateSamplerState(&ls, &d.linear))) return false;
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

bool D3dRenderer::draw(const std::uint32_t* argb, const Viewport& vp, const BackgroundFrame& bg) {
    Impl& d = *d_;
    if (!d.swap || !d.rtv) return false;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(d.context->Map(d.picture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return false;
    for (int y = 0; y < kGameH; ++y) std::memcpy(static_cast<char*>(m.pData) + static_cast<std::size_t>(y) * m.RowPitch, argb + static_cast<std::size_t>(y) * kGameW, kGameW * 4);
    d.context->Unmap(d.picture.Get(), 0);

    const float black[4] = {0, 0, 0, 1};
    ID3D11RenderTargetView* rt = d.rtv.Get();
    d.context->OMSetRenderTargets(1, &rt, nullptr);
    d.context->ClearRenderTargetView(d.rtv.Get(), black);
    if (bg.kind == Background::AmbientGlow && bg.glowBgra && !vp.empty()) {   // the glow: one pass over the whole window; the picture is drawn over its middle afterwards
        D3D11_MAPPED_SUBRESOURCE gm{};
        if (SUCCEEDED(d.context->Map(d.glow.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &gm))) {
            for (int y = 0; y < kGlowH; ++y) std::memcpy(static_cast<char*>(gm.pData) + static_cast<std::size_t>(y) * gm.RowPitch, bg.glowBgra + static_cast<std::size_t>(y) * kGlowW * 4, kGlowW * 4);
            d.context->Unmap(d.glow.Get(), 0);
        }
        D3D11_MAPPED_SUBRESOURCE cm{};
        if (SUCCEEDED(d.context->Map(d.glowConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cm))) {
            const float strength = 0.25f;                                     // at most a quarter of the cell's colour, right at the picture's edge
            const float reach = 0.16f * static_cast<float>(std::min(vp.w, vp.h));   // and fading out over about a sixth of the picture's size
            const float c[8] = {static_cast<float>(vp.x), static_cast<float>(vp.y), static_cast<float>(vp.x + vp.w), static_cast<float>(vp.y + vp.h), strength, reach, 0.0f, 0.0f};
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
        ID3D11ShaderResourceView* gsrv = d.glowView.Get();
        d.context->PSSetShaderResources(1, 1, &gsrv);
        ID3D11SamplerState* lsmp = d.linear.Get();
        d.context->PSSetSamplers(1, 1, &lsmp);
        d.context->Draw(3, 0);
    }
    if (!vp.empty()) {
        D3D11_VIEWPORT v{static_cast<float>(vp.x), static_cast<float>(vp.y), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f};
        d.context->RSSetViewports(1, &v);
        d.context->RSSetState(d.raster.Get());
        d.context->IASetInputLayout(nullptr);
        d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d.context->VSSetShader(d.vs.Get(), nullptr, 0);
        d.context->PSSetShader(d.ps.Get(), nullptr, 0);
        ID3D11ShaderResourceView* srv = d.pictureView.Get();
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
