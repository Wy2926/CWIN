#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <vector>

#include "Capsule.h"

namespace cwin {

// DirectComposition + Direct2D renderer bound to a layered window.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    HRESULT Initialize(HWND hwnd);
    HRESULT DrawCapsules(const std::vector<CapsuleRenderData>& capsules);
    void Commit();

private:
    HRESULT EnsureSurface(UINT width, UINT height);
    void DrawCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                     const D2D1_RECT_F& rect);

    // Per-capsule bespoke layouts.
    void DrawClockCapsule(ID2D1DeviceContext*, const CapsuleRenderData&,
                          const D2D1_RECT_F&);
    void DrawCpuCapsule(ID2D1DeviceContext*, const CapsuleRenderData&,
                        const D2D1_RECT_F&);
    void DrawWeatherCapsule(ID2D1DeviceContext*, const CapsuleRenderData&,
                            const D2D1_RECT_F&);
    void DrawNetSpeedCapsule(ID2D1DeviceContext*, const CapsuleRenderData&,
                             const D2D1_RECT_F&);
    void DrawGenericCapsule(ID2D1DeviceContext*, const CapsuleRenderData&,
                            const D2D1_RECT_F&);

    void DrawText(ID2D1DeviceContext* dc, const std::wstring& text,
                  IDWriteTextFormat* format, const D2D1_RECT_F& rect, ID2D1Brush* brush,
                  DWRITE_TEXT_ALIGNMENT h, DWRITE_PARAGRAPH_ALIGNMENT v);

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> bigFormat_;      // ~21px time/temp
    Microsoft::WRL::ComPtr<IDWriteTextFormat> valueFormat_;    // ~13px values
    Microsoft::WRL::ComPtr<IDWriteTextFormat> labelFormat_;    // ~10px labels
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> rootVisual_;
    Microsoft::WRL::ComPtr<IDCompositionSurface> surface_;
    UINT surfaceWidth_ = 0;
    UINT surfaceHeight_ = 0;
    HWND hwnd_ = nullptr;
};

} // namespace cwin
