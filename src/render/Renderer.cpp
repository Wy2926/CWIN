#include "Renderer.h"

#include <d3d11.h>
#include <dcomp.h>

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace cwin {

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kStripRadius = 12.0f;
// Translucent Win11-style card drawn inside the DComp tree (stands in for a
// real backdrop brush; WCA acrylic can't be used on a NOREDIRECTIONBITMAP
// window). Alpha high enough that capsules are legible over any taskbar.
const D2D1_COLOR_F kStripFill = D2D1::ColorF(0.12f, 0.12f, 0.14f, 0.72f);
const D2D1_COLOR_F kStripBorder = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
const D2D1_COLOR_F kTextColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);
const D2D1_COLOR_F kLabelColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f);
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
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.0f, L"",
        bigFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"",
        valueFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = dwriteFactory_->CreateTextFormat(
        L"Segoe UI Variable Small", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"",
        labelFormat_.GetAddressOf());
    if (FAILED(hr)) return hr;

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

        const float capsuleWidth = static_cast<float>(width) / capsules.size();
        ComPtr<ID2D1SolidColorBrush> divider;
        d2dContext_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f),
                                           divider.GetAddressOf());
        for (size_t i = 0; i < capsules.size(); ++i) {
            const float left = capsuleWidth * i;
            D2D1_RECT_F rect =
                D2D1::RectF(left, 0.0f, left + capsuleWidth, static_cast<float>(height));
            if (i > 0 && divider) {
                const float h = static_cast<float>(height);
                d2dContext_->DrawLine(D2D1::Point2F(left, h * 0.24f),
                                      D2D1::Point2F(left, h * 0.76f), divider.Get(), 1.0f);
            }
            DrawCapsule(d2dContext_.Get(), capsules[i], rect);
        }
    }

    hr = d2dContext_->EndDraw();
    d2dContext_->SetTarget(nullptr);
    surface_->EndDraw();
    return hr;
}

void Renderer::DrawText(ID2D1DeviceContext* dc, const std::wstring& text,
                        IDWriteTextFormat* format, const D2D1_RECT_F& rect,
                        ID2D1Brush* brush, DWRITE_TEXT_ALIGNMENT h,
                        DWRITE_PARAGRAPH_ALIGNMENT v) {
    if (text.empty() || !format || !brush) return;
    format->SetTextAlignment(h);
    format->SetParagraphAlignment(v);
    dc->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush,
                  D2D1_DRAW_TEXT_OPTIONS_NONE);
}

namespace {

ComPtr<ID2D1SolidColorBrush> Brush(ID2D1DeviceContext* dc, const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    dc->CreateSolidColorBrush(color, brush.GetAddressOf());
    return brush;
}

void DrawArc(ID2D1Factory1* factory, ID2D1DeviceContext* dc, D2D1_POINT_2F center,
             float radius, float sweep, ID2D1Brush* brush, float strokeWidth) {
    if (sweep <= 0.001f) return;
    ComPtr<ID2D1PathGeometry> arc;
    factory->CreatePathGeometry(arc.GetAddressOf());
    ComPtr<ID2D1GeometrySink> sink;
    if (!arc || FAILED(arc->Open(sink.GetAddressOf()))) return;
    const float startAngle = -kPi / 2.0f;
    const D2D1_POINT_2F start = D2D1::Point2F(center.x + radius * std::cos(startAngle),
                                              center.y + radius * std::sin(startAngle));
    const D2D1_POINT_2F end =
        D2D1::Point2F(center.x + radius * std::cos(startAngle + sweep),
                      center.y + radius * std::sin(startAngle + sweep));
    sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddArc(D2D1::ArcSegment(end, D2D1::SizeF(radius, radius), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                  sweep > kPi ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    dc->DrawGeometry(arc.Get(), brush, strokeWidth);
}

// Filled triangle pointing up (pointUp=true) or down, used as net-speed arrows.
void DrawArrow(ID2D1Factory1* factory, ID2D1DeviceContext* dc, D2D1_POINT_2F center,
               float size, bool pointUp, ID2D1Brush* brush) {
    ComPtr<ID2D1PathGeometry> tri;
    factory->CreatePathGeometry(tri.GetAddressOf());
    ComPtr<ID2D1GeometrySink> sink;
    if (!tri || FAILED(tri->Open(sink.GetAddressOf()))) return;
    const float half = size * 0.5f;
    const float tipY = pointUp ? center.y - half : center.y + half;
    const float baseY = pointUp ? center.y + half : center.y - half;
    sink->BeginFigure(D2D1::Point2F(center.x, tipY), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(center.x - half, baseY));
    sink->AddLine(D2D1::Point2F(center.x + half, baseY));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    dc->FillGeometry(tri.Get(), brush);
}

// Load-dependent accent: calm green -> amber -> alert red.
D2D1_COLOR_F LoadColor(float fraction) {
    if (fraction < 0.60f) return D2D1::ColorF(0.42f, 0.85f, 0.55f, 0.95f);
    if (fraction < 0.85f) return D2D1::ColorF(1.0f, 0.76f, 0.35f, 0.95f);
    return D2D1::ColorF(1.0f, 0.45f, 0.45f, 0.95f);
}

}  // namespace

void Renderer::DrawCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                           const D2D1_RECT_F& rect) {
    if (capsule.id == L"clock") return DrawClockCapsule(dc, capsule, rect);
    if (capsule.id == L"hardware") return DrawCpuCapsule(dc, capsule, rect);
    if (capsule.id == L"weather") return DrawWeatherCapsule(dc, capsule, rect);
    if (capsule.id == L"netspeed") return DrawNetSpeedCapsule(dc, capsule, rect);
    DrawGenericCapsule(dc, capsule, rect);
}

// Clock: pure typography — large time over a small weekday/date line.
void Renderer::DrawClockCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                                const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    if (!text || !label) return;
    const float h = rect.bottom - rect.top;
    D2D1_RECT_F timeRect = rect;
    timeRect.bottom = rect.top + h * 0.66f;
    D2D1_RECT_F dateRect = rect;
    dateRect.top = rect.top + h * 0.62f;
    dateRect.bottom = rect.bottom - h * 0.06f;
    DrawText(dc, capsule.primaryText, bigFormat_.Get(), timeRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    DrawText(dc, capsule.secondaryText, labelFormat_.Get(), dateRect, label.Get(),
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// CPU: load ring (green/amber/red) with % inside, label + fixed-scale
// CPU-history sparkline on the right.
void Renderer::DrawCpuCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                              const D2D1_RECT_F& rect) {
    const float h = rect.bottom - rect.top;
    const float pad = h * 0.14f;
    float cpu = capsule.series.empty() ? 0.0f : capsule.series.back() / 100.0f;
    cpu = std::clamp(cpu, 0.0f, 1.0f);

    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    auto track = Brush(dc, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f));
    auto loadBrush = Brush(dc, LoadColor(cpu));
    if (!text || !label || !track || !loadBrush) return;

    const float radius = h * 0.30f;
    const D2D1_POINT_2F center =
        D2D1::Point2F(rect.left + pad + radius, rect.top + h * 0.5f);
    ComPtr<ID2D1EllipseGeometry> ring;
    d2dFactory_->CreateEllipseGeometry(D2D1::Ellipse(center, radius, radius),
                                       ring.GetAddressOf());
    if (ring) dc->DrawGeometry(ring.Get(), track.Get(), 2.5f);
    DrawArc(d2dFactory_.Get(), dc, center, radius, cpu * 2.0f * kPi, loadBrush.Get(), 2.5f);

    D2D1_RECT_F ringRect = D2D1::RectF(center.x - radius, center.y - radius,
                                       center.x + radius, center.y + radius);
    DrawText(dc, capsule.primaryText, labelFormat_.Get(), ringRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const float x0 = center.x + radius + pad * 0.8f;
    D2D1_RECT_F labelRect = D2D1::RectF(x0, rect.top + pad * 0.5f, rect.right - pad,
                                        rect.top + h * 0.36f);
    DrawText(dc, L"CPU", labelFormat_.Get(), labelRect, label.Get(),
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    D2D1_RECT_F chart =
        D2D1::RectF(x0, rect.top + h * 0.40f, rect.right - pad, rect.bottom - pad * 0.7f);
    if (capsule.series.size() >= 2 && chart.right > chart.left + 4.0f) {
        const float stepX = (chart.right - chart.left) / (capsule.series.size() - 1);
        for (size_t i = 1; i < capsule.series.size(); ++i) {
            auto pointAt = [&](size_t idx) {
                const float norm = std::clamp(capsule.series[idx] / 100.0f, 0.0f, 1.0f);
                return D2D1::Point2F(chart.left + stepX * idx,
                                     chart.bottom - norm * (chart.bottom - chart.top));
            };
            dc->DrawLine(pointAt(i - 1), pointAt(i), loadBrush.Get(), 1.5f);
        }
    }
}

// Weather: vector condition glyph (sun / cloud / rain / snow) + large
// temperature with the condition text underneath.
void Renderer::DrawWeatherCapsule(ID2D1DeviceContext* dc,
                                  const CapsuleRenderData& capsule,
                                  const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    auto sun = Brush(dc, D2D1::ColorF(1.0f, 0.80f, 0.40f, 0.95f));
    auto cloud = Brush(dc, D2D1::ColorF(0.85f, 0.88f, 0.94f, 0.90f));
    auto rain = Brush(dc, D2D1::ColorF(0.45f, 0.68f, 1.0f, 0.95f));
    if (!text || !label || !sun || !cloud || !rain) return;

    const float h = rect.bottom - rect.top;
    const float pad = h * 0.14f;
    const D2D1_POINT_2F iconC =
        D2D1::Point2F(rect.left + pad + h * 0.26f, rect.top + h * 0.5f);
    const std::wstring& cond = capsule.secondaryText;
    const bool isSunny = cond.find(L"晴") != std::wstring::npos;
    const bool isRain = cond.find(L"雨") != std::wstring::npos ||
                        cond.find(L"雷") != std::wstring::npos;
    const bool isSnow = cond.find(L"雪") != std::wstring::npos;

    if (isSunny) {
        const float r = h * 0.14f;
        dc->FillEllipse(D2D1::Ellipse(iconC, r, r), sun.Get());
        for (int i = 0; i < 8; ++i) {
            const float a = i * kPi / 4.0f;
            dc->DrawLine(
                D2D1::Point2F(iconC.x + std::cos(a) * (r + 3.0f),
                              iconC.y + std::sin(a) * (r + 3.0f)),
                D2D1::Point2F(iconC.x + std::cos(a) * (r + 7.0f),
                              iconC.y + std::sin(a) * (r + 7.0f)),
                sun.Get(), 1.8f);
        }
    } else {
        // Cloud: two lobes over a flat-bottomed base.
        const float s = h * 0.16f;
        const float cy = iconC.y - (isRain || isSnow ? s * 0.35f : 0.0f);
        dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(iconC.x - s * 0.55f, cy), s * 0.75f,
                                      s * 0.75f),
                        cloud.Get());
        dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(iconC.x + s * 0.35f, cy - s * 0.30f),
                                      s * 0.90f, s * 0.90f),
                        cloud.Get());
        D2D1_ROUNDED_RECT base = D2D1::RoundedRect(
            D2D1::RectF(iconC.x - s * 1.30f, cy - s * 0.10f, iconC.x + s * 1.25f,
                        cy + s * 0.75f),
            s * 0.4f, s * 0.4f);
        dc->FillRoundedRectangle(base, cloud.Get());
        if (isRain) {
            for (int i = -1; i <= 1; ++i) {
                const float x = iconC.x + i * s * 0.75f;
                dc->DrawLine(D2D1::Point2F(x, cy + s * 1.05f),
                             D2D1::Point2F(x - s * 0.25f, cy + s * 1.65f), rain.Get(),
                             1.8f);
            }
        } else if (isSnow) {
            for (int i = -1; i <= 1; ++i) {
                const float x = iconC.x + i * s * 0.75f;
                dc->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(x, cy + s * 1.35f), 1.8f, 1.8f),
                    cloud.Get());
            }
        }
    }

    const float x0 = rect.left + pad + h * 0.58f;
    D2D1_RECT_F tempRect = D2D1::RectF(x0, rect.top, rect.right - pad * 0.5f,
                                       rect.top + h * 0.66f);
    D2D1_RECT_F condRect = D2D1::RectF(x0, rect.top + h * 0.62f, rect.right - pad * 0.5f,
                                       rect.bottom - h * 0.06f);
    DrawText(dc, capsule.primaryText, bigFormat_.Get(), tempRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    DrawText(dc, cond, labelFormat_.Get(), condRect, label.Get(),
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// Net speed: two color-coded rows — download (blue, down arrow) over upload
// (green, up arrow).
void Renderer::DrawNetSpeedCapsule(ID2D1DeviceContext* dc,
                                   const CapsuleRenderData& capsule,
                                   const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto down = Brush(dc, D2D1::ColorF(0.42f, 0.66f, 1.0f, 0.95f));
    auto up = Brush(dc, D2D1::ColorF(0.45f, 0.84f, 0.63f, 0.95f));
    if (!text || !down || !up) return;

    const float h = rect.bottom - rect.top;
    const float pad = h * 0.14f;
    const float rowH = (h - pad * 1.6f) / 2.0f;
    const float arrowX = rect.left + pad + 5.0f;
    const float textX = arrowX + 10.0f;

    const float y1 = rect.top + pad * 0.8f + rowH * 0.5f;
    const float y2 = y1 + rowH;
    DrawArrow(d2dFactory_.Get(), dc, D2D1::Point2F(arrowX, y1), 8.0f, false, down.Get());
    DrawArrow(d2dFactory_.Get(), dc, D2D1::Point2F(arrowX, y2), 8.0f, true, up.Get());

    D2D1_RECT_F downRect =
        D2D1::RectF(textX, y1 - rowH * 0.5f, rect.right - pad * 0.5f, y1 + rowH * 0.5f);
    D2D1_RECT_F upRect =
        D2D1::RectF(textX, y2 - rowH * 0.5f, rect.right - pad * 0.5f, y2 + rowH * 0.5f);
    DrawText(dc, capsule.primaryText, valueFormat_.Get(), downRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DrawText(dc, capsule.secondaryText, valueFormat_.Get(), upRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// Fallback for unknown capsule ids (future plugins): small label over value.
void Renderer::DrawGenericCapsule(ID2D1DeviceContext* dc,
                                  const CapsuleRenderData& capsule,
                                  const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    if (!text || !label) return;
    const float h = rect.bottom - rect.top;
    D2D1_RECT_F labelRect = rect;
    labelRect.bottom = rect.top + h * 0.38f;
    D2D1_RECT_F valueRect = rect;
    valueRect.top = labelRect.bottom;
    DrawText(dc, capsule.secondaryText, labelFormat_.Get(), labelRect, label.Get(),
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DrawText(dc, capsule.primaryText, valueFormat_.Get(), valueRect, text.Get(),
             DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void Renderer::Commit() {
    if (dcompDevice_) dcompDevice_->Commit();
}

} // namespace cwin
