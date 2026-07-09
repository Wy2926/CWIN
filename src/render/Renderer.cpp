#include "Renderer.h"

#include <d3d11.h>
#include <dcomp.h>

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace cwin {

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kCapsuleGap = 8.0f;
constexpr float kCornerRadius = 8.0f;
constexpr float kStripRadius = 12.0f;
// Translucent Win11-style card drawn inside the DComp tree (stands in for a
// real backdrop brush; WCA acrylic can't be used on a NOREDIRECTIONBITMAP
// window). Alpha high enough that capsules are legible over any taskbar.
const D2D1_COLOR_F kStripFill = D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.72f);
const D2D1_COLOR_F kStripBorder = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
const D2D1_COLOR_F kCapsuleFill = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f);
const D2D1_COLOR_F kTextColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);
const D2D1_COLOR_F kLabelColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f);
const D2D1_COLOR_F kAccentColor = D2D1::ColorF(0.38f, 0.62f, 1.0f, 0.95f);
}  // namespace

HRESULT Renderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   d3dDevice_.GetAddressOf(), nullptr, nullptr);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                               nullptr, 0, D3D11_SDK_VERSION,
                               d3dDevice_.GetAddressOf(), nullptr, nullptr);
    }
    if (FAILED(hr)) return hr;

    D2D1_FACTORY_OPTIONS opts{};
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           __uuidof(ID2D1Factory1), &opts,
                           reinterpret_cast<void**>(d2dFactory_.GetAddressOf()));
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice_.As(&dxgiDevice);
    if (FAILED(hr)) return hr;

    hr = d2dFactory_->CreateDevice(dxgiDevice.Get(), d2dDevice_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                         d2dContext_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) return hr;

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"",
        primaryFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;
    primaryFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    primaryFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI Variable Small", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"",
        labelFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;
    labelFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    labelFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = DCompositionCreateDevice(dxgiDevice.Get(), __uuidof(IDCompositionDevice),
                                  reinterpret_cast<void**>(dcompDevice_.GetAddressOf()));
    if (FAILED(hr)) return hr;

    hr = dcompDevice_->CreateTargetForHwnd(hwnd_, TRUE, dcompTarget_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = dcompDevice_->CreateVisual(rootVisual_.GetAddressOf());
    if (FAILED(hr)) return hr;

    return dcompTarget_->SetRoot(rootVisual_.Get());
}

HRESULT Renderer::EnsureSurface(UINT width, UINT height) {
    if (surface_ && surfaceWidth_ == width && surfaceHeight_ == height) return S_OK;

    HRESULT hr = dcompDevice_->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                                             DXGI_ALPHA_MODE_PREMULTIPLIED,
                                             surface_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    surfaceWidth_ = width;
    surfaceHeight_ = height;

    hr = rootVisual_->SetContent(surface_.Get());
    return hr;
}

HRESULT Renderer::DrawCapsules(const std::vector<CapsuleRenderData>& capsules) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const UINT width = std::max<LONG>(rc.right - rc.left, 1);
    const UINT height = std::max<LONG>(rc.bottom - rc.top, 1);

    HRESULT hr = EnsureSurface(width, height);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGISurface> dxgiSurface;
    POINT offset{};
    hr = surface_->BeginDraw(nullptr, IID_PPV_ARGS(dxgiSurface.GetAddressOf()), &offset);
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> bitmap;
    hr = d2dContext_->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), &props,
                                                  bitmap.GetAddressOf());
    if (FAILED(hr)) {
        surface_->EndDraw();
        return hr;
    }

    d2dContext_->SetTarget(bitmap.Get());
    d2dContext_->BeginDraw();
    d2dContext_->SetTransform(
        D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x),
                                      static_cast<float>(offset.y)));
    d2dContext_->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (!capsules.empty()) {
        // Translucent card behind the capsules (frosted-glass stand-in).
        ComPtr<ID2D1SolidColorBrush> stripFill, stripBorder;
        d2dContext_->CreateSolidColorBrush(kStripFill, stripFill.GetAddressOf());
        d2dContext_->CreateSolidColorBrush(kStripBorder, stripBorder.GetAddressOf());
        D2D1_ROUNDED_RECT strip = D2D1::RoundedRect(
            D2D1::RectF(0.5f, 0.5f, static_cast<float>(width) - 0.5f,
                        static_cast<float>(height) - 0.5f),
            kStripRadius, kStripRadius);
        if (stripFill) d2dContext_->FillRoundedRectangle(strip, stripFill.Get());
        if (stripBorder) d2dContext_->DrawRoundedRectangle(strip, stripBorder.Get(), 1.0f);

        const float totalGap = kCapsuleGap * (capsules.size() - 1);
        const float capsuleWidth =
            (static_cast<float>(width) - totalGap) / capsules.size();
        float x = 0.0f;
        for (const auto& capsule : capsules) {
            D2D1_RECT_F rect =
                D2D1::RectF(x, 0.0f, x + capsuleWidth, static_cast<float>(height));
            DrawCapsule(d2dContext_.Get(), capsule, rect);
            x += capsuleWidth + kCapsuleGap;
        }
    }

    hr = d2dContext_->EndDraw();
    d2dContext_->SetTarget(nullptr);
    surface_->EndDraw();
    return hr;
}

void Renderer::DrawCenteredText(ID2D1DeviceContext* dc, const std::wstring& text,
                                IDWriteTextFormat* format, const D2D1_RECT_F& rect,
                                ID2D1Brush* brush) {
    if (text.empty()) return;
    dc->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush,
                  D2D1_DRAW_TEXT_OPTIONS_NONE);
}

void Renderer::DrawCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                           const D2D1_RECT_F& rect) {
    ComPtr<ID2D1SolidColorBrush> fill, text, label, accent;
    dc->CreateSolidColorBrush(kCapsuleFill, fill.GetAddressOf());
    dc->CreateSolidColorBrush(kTextColor, text.GetAddressOf());
    dc->CreateSolidColorBrush(kLabelColor, label.GetAddressOf());
    dc->CreateSolidColorBrush(kAccentColor, accent.GetAddressOf());
    if (!fill || !text || !label || !accent) return;

    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, kCornerRadius, kCornerRadius);
    dc->FillRoundedRectangle(rounded, fill.Get());

    const float h = rect.bottom - rect.top;

    switch (capsule.templateKind) {
        case CapsuleTemplate::Text: {
            DrawCenteredText(dc, capsule.primaryText, primaryFormat_.Get(), rect,
                             text.Get());
            break;
        }
        case CapsuleTemplate::IconText: {
            D2D1_RECT_F labelRect = rect;
            labelRect.bottom = rect.top + h * 0.38f;
            D2D1_RECT_F valueRect = rect;
            valueRect.top = labelRect.bottom;
            DrawCenteredText(dc, capsule.secondaryText, labelFormat_.Get(), labelRect,
                             label.Get());
            DrawCenteredText(dc, capsule.primaryText, primaryFormat_.Get(), valueRect,
                             text.Get());
            break;
        }
        case CapsuleTemplate::Sparkline: {
            const float pad = 6.0f;
            D2D1_RECT_F chart = D2D1::RectF(rect.left + pad, rect.top + pad,
                                            rect.left + (rect.right - rect.left) * 0.5f,
                                            rect.bottom - pad);
            if (capsule.series.size() >= 2) {
                float minV = *std::min_element(capsule.series.begin(), capsule.series.end());
                float maxV = *std::max_element(capsule.series.begin(), capsule.series.end());
                const float range = std::max(maxV - minV, 1e-3f);
                const float stepX =
                    (chart.right - chart.left) / (capsule.series.size() - 1);
                for (size_t i = 1; i < capsule.series.size(); ++i) {
                    auto pointAt = [&](size_t idx) {
                        const float norm = (capsule.series[idx] - minV) / range;
                        return D2D1::Point2F(chart.left + stepX * idx,
                                             chart.bottom - norm * (chart.bottom - chart.top));
                    };
                    dc->DrawLine(pointAt(i - 1), pointAt(i), accent.Get(), 1.5f);
                }
            }
            D2D1_RECT_F valueRect = rect;
            valueRect.left = rect.left + (rect.right - rect.left) * 0.5f;
            DrawCenteredText(dc, capsule.primaryText, primaryFormat_.Get(), valueRect,
                             text.Get());
            break;
        }
        case CapsuleTemplate::ProgressRing: {
            const float radius = h * 0.32f;
            const D2D1_POINT_2F center =
                D2D1::Point2F(rect.left + h * 0.5f, rect.top + h * 0.5f);
            ComPtr<ID2D1EllipseGeometry> track;
            d2dFactory_->CreateEllipseGeometry(D2D1::Ellipse(center, radius, radius),
                                               track.GetAddressOf());
            if (track) dc->DrawGeometry(track.Get(), label.Get(), 2.0f);

            const float progress = std::clamp(capsule.progress, 0.0f, 1.0f);
            if (progress > 0.001f) {
                ComPtr<ID2D1PathGeometry> arc;
                d2dFactory_->CreatePathGeometry(arc.GetAddressOf());
                ComPtr<ID2D1GeometrySink> sink;
                if (arc && SUCCEEDED(arc->Open(sink.GetAddressOf()))) {
                    const float startAngle = -kPi / 2.0f;
                    const float sweep = progress * 2.0f * kPi;
                    const D2D1_POINT_2F start =
                        D2D1::Point2F(center.x + radius * std::cos(startAngle),
                                      center.y + radius * std::sin(startAngle));
                    const D2D1_POINT_2F end =
                        D2D1::Point2F(center.x + radius * std::cos(startAngle + sweep),
                                      center.y + radius * std::sin(startAngle + sweep));
                    sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
                    sink->AddArc(D2D1::ArcSegment(
                        end, D2D1::SizeF(radius, radius), 0.0f,
                        D2D1_SWEEP_DIRECTION_CLOCKWISE,
                        sweep > kPi ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    sink->Close();
                    dc->DrawGeometry(arc.Get(), accent.Get(), 2.5f);
                }
            }

            D2D1_RECT_F valueRect = rect;
            valueRect.left = rect.left + h;
            DrawCenteredText(dc, capsule.primaryText, primaryFormat_.Get(), valueRect,
                             text.Get());
            break;
        }
    }
}

void Renderer::Commit() {
    if (dcompDevice_) dcompDevice_->Commit();
}

} // namespace cwin
