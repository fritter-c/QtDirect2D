#ifndef DIRECT2DDEVICECONTEXT_H
#define DIRECT2DDEVICECONTEXT_H

#ifdef __MINGW64__
#include <d2d1_1.h>
#define ID2D1DEVICECONTEXT  ID2D1DeviceContext
#else

#include <d2d1_3.h>
#define ID2D1DEVICECONTEXT ID2D1DeviceContext6
#endif
#include <wrl.h>
using Microsoft::WRL::ComPtr;
class IDirect2DDeviceContext
{
protected:
	ComPtr<ID2D1DEVICECONTEXT> m_context;
	virtual void recreateTarget() = 0;
public:
	inline void begin() { m_context->BeginDraw(); };
	inline bool end()
	{
		if (m_context->EndDraw() == static_cast<HRESULT>(D2DERR_RECREATE_TARGET)) {
			recreateTarget();
			return false;
		}
		return true;
	};
	inline ID2D1DEVICECONTEXT* dc() { return m_context.Get(); }

};
#endif // DIRECT2DDEVICECONTEXT_H
