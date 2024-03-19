#include "directcontext.h"
#include "qlogging.h"
#include  <dxgi1_5.h>
bool DirectContext::init()
{
	HRESULT hr;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,
		__uuidof(ID2D1FACTORY),
		NULL,
		reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
	if (FAILED(hr)) {
		qWarning("%s: Could not create D2D1CreateFactory: %#lx", __FUNCTION__, hr);
		return false;
	}

	D3D_FEATURE_LEVEL level;
	D3D_FEATURE_LEVEL feature[] = { D3D_FEATURE_LEVEL_11_0,
								   D3D_FEATURE_LEVEL_11_1,
								   D3D_FEATURE_LEVEL_12_0 };
	D3D_DRIVER_TYPE typeAttempts[] = { D3D_DRIVER_TYPE_HARDWARE };
	const int ntypes = int(sizeof(typeAttempts) / sizeof(typeAttempts[0]));

	for (int i = 0; i < ntypes; i++) {
		hr = D3D11CreateDevice(nullptr,
			typeAttempts[i],
			nullptr,
			D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			feature,
			_countof(feature),
			D3D11_SDK_VERSION,
			reinterpret_cast<ID3D11Device**>(m_d3dDevice.GetAddressOf()),
			&level,
			reinterpret_cast<ID3D11DeviceContext**>(
				m_d3ddevicecontext.GetAddressOf()));

		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr)) {
		qWarning("%s: Could not create Direct3D Device: %#lx", __FUNCTION__, hr);
		return false;
	}

	ComPtr<IDXGIDevice1> dxgiDevice;
	ComPtr<IDXGIAdapter> dxgiAdapter;

	hr = m_d3dDevice.As(&dxgiDevice);
	if (FAILED(hr)) {
		qWarning("%s: DXGI Device interface query failed on D3D Device: %#lx", __FUNCTION__, hr);
		return false;
	}

	dxgiDevice->SetMaximumFrameLatency(1);

	hr = dxgiDevice->GetAdapter(&dxgiAdapter);
	if (FAILED(hr)) {
		qWarning("%s: Failed to probe DXGI Device for parent DXGI Adapter: %#lx", __FUNCTION__, hr);
		return false;
	}

	hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory));
	if (FAILED(hr)) {
		qWarning("%s: Failed to probe DXGI Adapter for parent DXGI Factory: %#lx", __FUNCTION__, hr);
		return false;
	}

	hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
	if (FAILED(hr)) {
		qWarning("%s: Could not create D2D Device: %#lx", __FUNCTION__, hr);
		return false;
	}

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(ID2D1WRITEFACTORY),
		static_cast<IUnknown**>(&m_dwriteFactory));
	if (FAILED(hr)) {
		qWarning("%s: Could not create DirectWrite factory: %#lx", __FUNCTION__, hr);
		return false;
	}

	hr = m_dwriteFactory->GetGdiInterop(m_dwriteInterop.GetAddressOf());
	if (FAILED(hr)) {
		qWarning("%s: Could not create DirectWrite GDI Interop: %#lx", __FUNCTION__, hr);
		return false;
	}
	return true;
}

DirectContext::DirectContext() {}
