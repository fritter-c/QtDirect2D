#include "direct2dbitmap.h"
#include "direct2dwidget.h"
#include "src/direct2d/direct2dqthelper.h"
int Direct2DBitmap::metric(PaintDeviceMetric metric) const
{
	if (!m_context.Get())
		return -1;

	switch (metric) {
	case QPaintDevice::PdmWidth:
		return (int)m_context->GetPixelSize().width;
	case QPaintDevice::PdmHeight:
		return (int)m_context->GetPixelSize().height;
	case QPaintDevice::PdmWidthMM: {
		FLOAT dpix, dpiy;
		m_context->GetDpi(&dpix, &dpiy);
		return (int)((m_context->GetPixelSize().width * 25.4) / dpix);
	}
	case QPaintDevice::PdmHeightMM: {
		FLOAT dpix, dpiy;
		m_context->GetDpi(&dpix, &dpiy);
		return (int)((m_context->GetPixelSize().height * 25.4) / dpiy);
	}
	case QPaintDevice::PdmNumColors: {
		return INT_MAX;
	}
	case QPaintDevice::PdmDepth: {
		return 32;
	}
	case QPaintDevice::PdmPhysicalDpiX:
	case QPaintDevice::PdmDpiX: {
		FLOAT dpix, dpiy;
		m_context->GetDpi(&dpix, &dpiy);
		return qRound(dpix);
	}
	case QPaintDevice::PdmPhysicalDpiY:
	case QPaintDevice::PdmDpiY: {
		FLOAT dpix, dpiy;
		m_context->GetDpi(&dpix, &dpiy);
		return qRound(dpiy);
	}
	case QPaintDevice::PdmDevicePixelRatio: {
		return 1;
	}
	case QPaintDevice::PdmDevicePixelRatioScaled:
		break;
	}
	return -1;
}

Direct2DBitmap::Direct2DBitmap() : m_dpiX(96.0f), m_dpiY(96.0f), m_width(100), m_height(100), m_initiated(false) {}

bool Direct2DBitmap::resize(UINT32 width, UINT32 height)
{
	m_width = width;
	m_height = height;
	if (ensureInit()) {
		m_context->SetTarget(nullptr);
		m_bitmap.Reset();

		D2D1_SIZE_U size = { UINT32(m_width), UINT32(m_height) };

		HRESULT hr = m_context->CreateBitmap(size,
			nullptr,
			0,
			bitmapProperties(),
			m_bitmap.ReleaseAndGetAddressOf());
		if (SUCCEEDED(hr))
			m_context->SetTarget(m_bitmap.Get());
		else
			qWarning("%s: Could not create bitmap: %#lx", __FUNCTION__, hr);

		return SUCCEEDED(hr);
	}
	return false;
}

bool Direct2DBitmap::changeDpi(FLOAT dpiX, FLOAT dpiY)
{
	m_dpiX = dpiX;
	m_dpiY = dpiY;
	if (ensureInit()) {
		m_context->SetTarget(nullptr);
		m_context->SetDpi(m_dpiX, m_dpiY);
		D2D1_SIZE_U size = { UINT32(m_bitmap->GetSize().width), UINT32(m_bitmap->GetSize().height) };
		m_bitmap.Reset();

		HRESULT hr = m_context->CreateBitmap(size,
			nullptr,
			0,
			bitmapProperties(),
			m_bitmap.ReleaseAndGetAddressOf());
		if (SUCCEEDED(hr))
			m_context->SetTarget(m_bitmap.Get());
		else
			qWarning("%s: Could not create bitmap: %#lx", __FUNCTION__, hr);

		return SUCCEEDED(hr);
	}
	return false;
}

bool Direct2DBitmap::init(UINT32 width, UINT32 height, FLOAT dpiX, FLOAT dpiY)
{
	HRESULT hr = DirectContext::instance().d2dDevice()->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
		m_context.ReleaseAndGetAddressOf());
	if (SUCCEEDED(hr)) {
		m_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		m_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
		m_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
		if (SUCCEEDED(hr)) {
			engine.reset(new Direct2DPaintEngine(this));
			D2D1_SIZE_U size = { width, height };
			m_dpiX = dpiX;
			m_dpiY = dpiY;
			hr = m_context->CreateBitmap(size,
				nullptr,
				0,
				bitmapProperties(),
				m_bitmap.ReleaseAndGetAddressOf());

			if (SUCCEEDED(hr))
				m_context->SetTarget(m_bitmap.Get());
			else
				qWarning("%s: Could not create bitmap: %#lx", __FUNCTION__, hr);
		}
	}
	else {
		qWarning("%s: Could not create device context: %#lx", __FUNCTION__, hr);
	}
	m_initiated = SUCCEEDED(hr);
	return m_initiated;
}

void Direct2DBitmap::fillRect(const QRect& rect, QColor& color)
{
	if (ensureInit()) {
		begin();
		ComPtr<ID2D1SolidColorBrush> brush;
		if (SUCCEEDED(m_context->CreateSolidColorBrush(toD2DColorF(color), brush.GetAddressOf())))
			m_context->FillRectangle(toD2dRectF(rect), brush.Get());
		end();
	}
}

void Direct2DBitmap::fillRect(const QRect& rect, D2D1::ColorF color)
{
	if (ensureInit()) {
		begin();
		ComPtr<ID2D1SolidColorBrush> brush;
		if (SUCCEEDED(m_context->CreateSolidColorBrush(color, brush.GetAddressOf())))
			m_context->FillRectangle(toD2dRectF(rect), brush.Get());
		end();
	}
}

void Direct2DBitmap::flush(QColor color)
{
	if (ensureInit()) {
		begin();
		m_context->Clear(toD2DColorF(color));
		end();
	}
}

QPaintEngine* Direct2DBitmap::paintEngine() const
{
	return engine.get();
}

void Direct2DBitmap::recreateTarget()
{
	assert(init(width(), height(), physicalDpiX(), physicalDpiY()));
}

bool Direct2DBitmap::ensureInit()
{
	if (!m_initiated)
		return init(m_width, m_height);
	return m_initiated;
}
