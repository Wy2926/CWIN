#include "Renderer.h"

#include <d3d11.h>
#include <dcomp.h>

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace cwin {

namespace {
constexpr float kPi = 3.14159265f;
// Translucent Win11-style pill drawn inside the DComp tree (stands in for a
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
        // Dynamic-island style: one capsule at a time inside a fully rounded
        // pill whose radius is half its height (fixed proportions).
        const float pillRadius = static_cast<float>(height) * 0.5f - 0.5f;
        D2D1_ROUNDED_RECT strip = D2D1::RoundedRect(
            D2D1::RectF(0.5f, 0.5f, static_cast<float>(width) - 0.5f,
                        static_cast<float>(height) - 0.5f),
            pillRadius, pillRadius);
        if (stripFill) d2dContext_->FillRoundedRectangle(strip, stripFill.Get());
        if (stripBorder) d2dContext_->DrawRoundedRectangle(strip, stripBorder.Get(), 1.0f);

        D2D1_RECT_F rect =
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        DrawCapsule(d2dContext_.Get(), capsules.front(), rect);
    }

    hr = d2dContext_->EndDraw();
    d2dContext_->SetTarget(nullptr);
    surface_->EndDraw();
    return hr;
}

ComPtr<IDWriteTextFormat> Renderer::MakeFormat(float fontPx, DWRITE_FONT_WEIGHT weight) {
    ComPtr<IDWriteTextFormat> format;
    dwriteFactory_->CreateTextFormat(L"Segoe UI Variable Display", nullptr, weight,
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                     fontPx, L"", format.GetAddressOf());
    return format;
}

void Renderer::DrawText(ID2D1DeviceContext* dc, const std::wstring& text, float fontPx,
                        DWRITE_FONT_WEIGHT weight, const D2D1_RECT_F& rect,
                        ID2D1Brush* brush, DWRITE_TEXT_ALIGNMENT h,
                        DWRITE_PARAGRAPH_ALIGNMENT v) {
    if (text.empty() || !brush) return;
    auto format = MakeFormat(fontPx, weight);
    if (!format) return;
    format->SetTextAlignment(h);
    format->SetParagraphAlignment(v);
    dc->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format.Get(), rect,
                  brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
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

namespace {
// Shared proportional geometry for the wide pill: a left icon slot + a content
// column. All values are fractions of the pill height so proportions are fixed.
struct PillLayout {
    float h;
    float pad;
    float iconR;
    D2D1_POINT_2F iconC;
    float contentL;
    float contentR;
    float bigPx;    // primary line
    float valuePx;  // value line
    float labelPx;  // secondary line
};

PillLayout Layout(const D2D1_RECT_F& rect) {
    PillLayout p;
    p.h = rect.bottom - rect.top;
    p.pad = p.h * 0.24f;
    p.iconR = p.h * 0.26f;
    p.iconC = D2D1::Point2F(rect.left + p.pad + p.iconR, rect.top + p.h * 0.5f);
    p.contentL = p.iconC.x + p.iconR + p.h * 0.16f;
    p.contentR = rect.right - p.h * 0.20f;
    p.bigPx = p.h * 0.40f;
    p.valuePx = p.h * 0.33f;
    p.labelPx = p.h * 0.24f;
    return p;
}
}  // namespace

// Two-line content column: bold primary over a muted secondary, vertically
// centered together. Used by clock / weather for a consistent rhythm.
static void DrawTwoLine(Renderer* self, ID2D1DeviceContext* dc, const PillLayout& p,
                        const D2D1_RECT_F& rect, const std::wstring& primary,
                        const std::wstring& secondary, ID2D1Brush* text,
                        ID2D1Brush* label);

// Clock: live clock-face glyph + large time over weekday/date.
void Renderer::DrawClockCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                                const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    auto face = Brush(dc, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.55f));
    auto accent = Brush(dc, D2D1::ColorF(0.42f, 0.66f, 1.0f, 0.95f));
    if (!text || !label || !face || !accent) return;
    const PillLayout p = Layout(rect);

    ComPtr<ID2D1EllipseGeometry> ring;
    d2dFactory_->CreateEllipseGeometry(D2D1::Ellipse(p.iconC, p.iconR, p.iconR),
                                       ring.GetAddressOf());
    if (ring) dc->DrawGeometry(ring.Get(), face.Get(), p.h * 0.045f);

    int hh = 0, mm = 0;
    swscanf_s(capsule.primaryText.c_str(), L"%d:%d", &hh, &mm);
    const float minAngle = mm * 6.0f * kPi / 180.0f - kPi / 2.0f;
    const float hourAngle = ((hh % 12) + mm / 60.0f) * 30.0f * kPi / 180.0f - kPi / 2.0f;
    dc->DrawLine(p.iconC,
                 D2D1::Point2F(p.iconC.x + std::cos(hourAngle) * p.iconR * 0.5f,
                               p.iconC.y + std::sin(hourAngle) * p.iconR * 0.5f),
                 text.Get(), p.h * 0.05f);
    dc->DrawLine(p.iconC,
                 D2D1::Point2F(p.iconC.x + std::cos(minAngle) * p.iconR * 0.78f,
                               p.iconC.y + std::sin(minAngle) * p.iconR * 0.78f),
                 accent.Get(), p.h * 0.04f);

    DrawTwoLine(this, dc, p, rect, capsule.primaryText, capsule.secondaryText, text.Get(),
                label.Get());
}

// CPU: load ring (green/amber/red) with % inside; label + fixed-scale
// CPU-history sparkline fill on the right.
void Renderer::DrawCpuCapsule(ID2D1DeviceContext* dc, const CapsuleRenderData& capsule,
                              const D2D1_RECT_F& rect) {
    const PillLayout p = Layout(rect);
    float cpu = capsule.series.empty() ? 0.0f : capsule.series.back() / 100.0f;
    cpu = std::clamp(cpu, 0.0f, 1.0f);

    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    auto track = Brush(dc, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f));
    auto loadBrush = Brush(dc, LoadColor(cpu));
    if (!text || !label || !track || !loadBrush) return;

    ComPtr<ID2D1EllipseGeometry> ring;
    d2dFactory_->CreateEllipseGeometry(D2D1::Ellipse(p.iconC, p.iconR, p.iconR),
                                       ring.GetAddressOf());
    if (ring) dc->DrawGeometry(ring.Get(), track.Get(), p.h * 0.06f);
    DrawArc(d2dFactory_.Get(), dc, p.iconC, p.iconR, cpu * 2.0f * kPi, loadBrush.Get(),
            p.h * 0.06f);

    D2D1_RECT_F ringRect = D2D1::RectF(p.iconC.x - p.iconR, p.iconC.y - p.iconR,
                                       p.iconC.x + p.iconR, p.iconC.y + p.iconR);
    DrawText(dc, capsule.primaryText, p.labelPx, DWRITE_FONT_WEIGHT_SEMI_BOLD, ringRect,
             text.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F labelRect = D2D1::RectF(p.contentL, rect.top + p.h * 0.16f, p.contentR,
                                        rect.top + p.h * 0.48f);
    DrawText(dc, L"CPU 使用率", p.labelPx, DWRITE_FONT_WEIGHT_NORMAL, labelRect,
             label.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    D2D1_RECT_F chart = D2D1::RectF(p.contentL, rect.top + p.h * 0.52f, p.contentR,
                                    rect.bottom - p.h * 0.20f);
    if (capsule.series.size() >= 2 && chart.right > chart.left + 4.0f) {
        const float stepX = (chart.right - chart.left) / (capsule.series.size() - 1);
        auto pointAt = [&](size_t idx) {
            const float norm = std::clamp(capsule.series[idx] / 100.0f, 0.0f, 1.0f);
            return D2D1::Point2F(chart.left + stepX * idx,
                                 chart.bottom - norm * (chart.bottom - chart.top));
        };
        // Filled area under the CPU line for a fuller look.
        ComPtr<ID2D1PathGeometry> area;
        d2dFactory_->CreatePathGeometry(area.GetAddressOf());
        ComPtr<ID2D1GeometrySink> sink;
        if (area && SUCCEEDED(area->Open(sink.GetAddressOf()))) {
            sink->BeginFigure(D2D1::Point2F(chart.left, chart.bottom),
                              D2D1_FIGURE_BEGIN_FILLED);
            for (size_t i = 0; i < capsule.series.size(); ++i) sink->AddLine(pointAt(i));
            sink->AddLine(D2D1::Point2F(chart.right, chart.bottom));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            auto fillBrush = Brush(dc, D2D1::ColorF(LoadColor(cpu).r, LoadColor(cpu).g,
                                                    LoadColor(cpu).b, 0.20f));
            if (fillBrush) dc->FillGeometry(area.Get(), fillBrush.Get());
        }
        for (size_t i = 1; i < capsule.series.size(); ++i)
            dc->DrawLine(pointAt(i - 1), pointAt(i), loadBrush.Get(), p.h * 0.04f);
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
    auto cloud = Brush(dc, D2D1::ColorF(0.85f, 0.88f, 0.94f, 0.92f));
    auto rain = Brush(dc, D2D1::ColorF(0.45f, 0.68f, 1.0f, 0.95f));
    if (!text || !label || !sun || !cloud || !rain) return;

    const PillLayout p = Layout(rect);
    const D2D1_POINT_2F iconC = p.iconC;
    const std::wstring& cond = capsule.secondaryText;
    const bool isSunny = cond.find(L"晴") != std::wstring::npos;
    const bool isRain = cond.find(L"雨") != std::wstring::npos ||
                        cond.find(L"雷") != std::wstring::npos;
    const bool isSnow = cond.find(L"雪") != std::wstring::npos;

    if (isSunny) {
        const float r = p.h * 0.15f;
        dc->FillEllipse(D2D1::Ellipse(iconC, r, r), sun.Get());
        for (int i = 0; i < 8; ++i) {
            const float a = i * kPi / 4.0f;
            dc->DrawLine(D2D1::Point2F(iconC.x + std::cos(a) * (r + p.h * 0.06f),
                                       iconC.y + std::sin(a) * (r + p.h * 0.06f)),
                         D2D1::Point2F(iconC.x + std::cos(a) * (r + p.h * 0.15f),
                                       iconC.y + std::sin(a) * (r + p.h * 0.15f)),
                         sun.Get(), p.h * 0.045f);
        }
    } else {
        const float s = p.h * 0.17f;
        const float cy = iconC.y - (isRain || isSnow ? s * 0.35f : 0.0f);
        dc->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(iconC.x - s * 0.55f, cy), s * 0.75f, s * 0.75f),
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
                             p.h * 0.045f);
            }
        } else if (isSnow) {
            for (int i = -1; i <= 1; ++i) {
                const float x = iconC.x + i * s * 0.75f;
                dc->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(x, cy + s * 1.35f), p.h * 0.04f,
                                  p.h * 0.04f),
                    cloud.Get());
            }
        }
    }

    DrawTwoLine(this, dc, p, rect, capsule.primaryText, cond, text.Get(), label.Get());
}

// Net speed: two color-coded rows — download (blue ▼) over upload (green ▲).
void Renderer::DrawNetSpeedCapsule(ID2D1DeviceContext* dc,
                                   const CapsuleRenderData& capsule,
                                   const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto down = Brush(dc, D2D1::ColorF(0.42f, 0.66f, 1.0f, 0.95f));
    auto up = Brush(dc, D2D1::ColorF(0.45f, 0.84f, 0.63f, 0.95f));
    if (!text || !down || !up) return;

    const PillLayout p = Layout(rect);
    const float rowY1 = rect.top + p.h * 0.30f;
    const float rowY2 = rect.top + p.h * 0.70f;
    const float arrowX = rect.left + p.pad + p.h * 0.10f;
    const float textL = arrowX + p.h * 0.22f;
    const float arrow = p.h * 0.20f;

    DrawArrow(d2dFactory_.Get(), dc, D2D1::Point2F(arrowX, rowY1), arrow, false,
              down.Get());
    DrawArrow(d2dFactory_.Get(), dc, D2D1::Point2F(arrowX, rowY2), arrow, true, up.Get());

    const float half = p.h * 0.22f;
    D2D1_RECT_F downRect =
        D2D1::RectF(textL, rowY1 - half, p.contentR, rowY1 + half);
    D2D1_RECT_F upRect = D2D1::RectF(textL, rowY2 - half, p.contentR, rowY2 + half);
    DrawText(dc, capsule.primaryText, p.valuePx, DWRITE_FONT_WEIGHT_SEMI_BOLD, downRect,
             text.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DrawText(dc, capsule.secondaryText, p.valuePx, DWRITE_FONT_WEIGHT_SEMI_BOLD, upRect,
             text.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// Fallback for unknown capsule ids (future plugins): centered value + label.
void Renderer::DrawGenericCapsule(ID2D1DeviceContext* dc,
                                  const CapsuleRenderData& capsule,
                                  const D2D1_RECT_F& rect) {
    auto text = Brush(dc, kTextColor);
    auto label = Brush(dc, kLabelColor);
    if (!text || !label) return;
    const float h = rect.bottom - rect.top;
    D2D1_RECT_F valueRect = rect;
    valueRect.bottom = rect.top + h * 0.62f;
    D2D1_RECT_F labelRect = rect;
    labelRect.top = rect.top + h * 0.56f;
    DrawText(dc, capsule.primaryText, h * 0.34f, DWRITE_FONT_WEIGHT_SEMI_BOLD, valueRect,
             text.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    DrawText(dc, capsule.secondaryText, h * 0.24f, DWRITE_FONT_WEIGHT_NORMAL, labelRect,
             label.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

static void DrawTwoLine(Renderer* self, ID2D1DeviceContext* dc, const PillLayout& p,
                        const D2D1_RECT_F& rect, const std::wstring& primary,
                        const std::wstring& secondary, ID2D1Brush* text,
                        ID2D1Brush* label) {
    D2D1_RECT_F line1 = D2D1::RectF(p.contentL, rect.top + p.h * 0.14f, p.contentR,
                                    rect.top + p.h * 0.60f);
    D2D1_RECT_F line2 = D2D1::RectF(p.contentL, rect.top + p.h * 0.56f, p.contentR,
                                    rect.bottom - p.h * 0.12f);
    self->DrawText(dc, primary, p.bigPx, DWRITE_FONT_WEIGHT_SEMI_BOLD, line1, text,
                   DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    self->DrawText(dc, secondary, p.labelPx, DWRITE_FONT_WEIGHT_NORMAL, line2, label,
                   DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void Renderer::Commit() {
    if (dcompDevice_) dcompDevice_->Commit();
}

} // namespace cwin
