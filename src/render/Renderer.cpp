#include "Renderer.h"

#include <d3d11.h>
#include <dcomp.h>

using Microsoft::WRL::ComPtr;

namespace cwin {

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

HRESULT Renderer::DrawPlaceholder() {
    // TODO: capsule template rendering (text / icon+text / sparkline / progress ring).
    return S_OK;
}

void Renderer::Commit() {
    if (dcompDevice_) dcompDevice_->Commit();
}

} // namespace cwin
