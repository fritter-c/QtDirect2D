#ifndef DIRECT2DBITMAP_H
#define DIRECT2DBITMAP_H

#include <QObject>
#include <QPaintDevice>
#include "src/direct2d/direct2dengine.h"
#include <windows.h>
#include <wrl/client.h>
#include "direct2ddevicecontext.h"

using Microsoft::WRL::ComPtr;

class Direct2DBitmap : public QPaintDevice, public IDirect2DDeviceContext
{
private:
	ComPtr<ID2D1Bitmap1> m_bitmap;
	QScopedPointer<Direct2DPaintEngine> engine;
	FLOAT m_dpiX;
	FLOAT m_dpiY;
	UINT32 m_width;
	UINT32 m_height;
	bool m_initiated;
protected:
	int metric(PaintDeviceMetric metric) const override;
	void recreateTarget() override;
	bool ensureInit();
public:
	inline ID2D1Bitmap1* bitmap() { return m_bitmap.Get(); }
	D2D1_BITMAP_PROPERTIES1 bitmapProperties() const
	{
		return D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpiX,
			m_dpiY);
	}
	Direct2DBitmap();
	~Direct2DBitmap() {};
	bool resize(UINT32 width, UINT32 height);
	bool changeDpi(FLOAT dpiX, FLOAT dpiY);
	bool init(UINT32 width, UINT32 height, FLOAT dpiX = 96.0f, FLOAT dpiY = 96.0f);
	void fillRect(const QRect& rect, QColor& color);
	void fillRect(const QRect& rect, D2D1::ColorF color = D2D1::ColorF::White);
	void flush(QColor color = Qt::white);
	QPaintEngine* paintEngine() const override;
};

#endif // DIRECT2DBITMAP_H
