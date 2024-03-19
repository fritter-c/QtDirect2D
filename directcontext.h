#ifndef DIRECTCONTEXT_H
#define DIRECTCONTEXT_H
#ifdef __MINGW64__
#include <d2d1_1.h>
#define ID2D1DEVICE ID2D1Device
#define ID2D1FACTORY ID2D1Factory1
#define ID2D1WRITEFACTORY IDWriteFactory
#else

#include <d2d1_3.h>
#define ID2D1DEVICE ID2D1Device7
#define ID2D1FACTORY ID2D1Factory8
#define ID2D1WRITEFACTORY IDWriteFactory8
#endif
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>
#include <dwrite_3.h>
using Microsoft::WRL::ComPtr;
class DirectContext
{
private:
	ComPtr<ID2D1DEVICE> m_d2dDevice;
	ComPtr<ID3D11Device5> m_d3dDevice;
	ComPtr<ID2D1FACTORY> m_d2dFactory;
	ComPtr<ID3D11DeviceContext3> m_d3ddevicecontext;
	ComPtr<ID2D1WRITEFACTORY> m_dwriteFactory;
	ComPtr<IDXGIFactory7> m_dxgiFactory;
	ComPtr<IDWriteGdiInterop> m_dwriteInterop;
public:
	bool init();
	DirectContext();
	~DirectContext() = default;

	static DirectContext& instance()
	{
		static DirectContext _instance;
		return _instance;
	}
	inline ID3D11Device5* d3dDevice() const { return m_d3dDevice.Get(); }
	inline ID2D1DEVICE* d2dDevice() const { return m_d2dDevice.Get(); }
	inline ID2D1FACTORY* d2dFactory() const { return m_d2dFactory.Get(); }
	inline ID2D1WRITEFACTORY* dwriteFactory() const { return m_dwriteFactory.Get(); }
	inline ID3D11DeviceContext3* d3dDeviceContext() const { return m_d3ddevicecontext.Get(); }
	inline IDXGIFactory7* dxgiFactory() const { return m_dxgiFactory.Get(); }
	inline IDWriteGdiInterop* IDWriteGdiInterop() const { return m_dwriteInterop.Get(); }
};

[[maybe_unused]] static inline ID2D1FACTORY* factory()
{
	return DirectContext::instance().d2dFactory();
}
#endif // DIRECTCONTEXT_H
