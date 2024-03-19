#include "direct2dwindow.h"
#include "os.h"
#include "qevent.h"

Direct2DWindow::Direct2DWindow(QWindow* parent)
	: QWindow(parent)
	, m_deviceInitialized(false)
{
	engine.reset();
	m_hwnd = reinterpret_cast<HWND>(winId());
	m_context = nullptr;
	m_swapChain = nullptr;

	setupSwapChain();

	HRESULT hr = DirectContext::instance().d2dDevice()->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
		m_context.ReleaseAndGetAddressOf());
	if (FAILED(hr)) {
		qWarning("%s: Couldn't create Direct2D Device context: %#lx", __FUNCTION__, hr);
		assert(false);
	}
	else {
		engine.reset(new Direct2DPaintEngine(this));
	}
	m_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	m_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	m_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
	m_deviceInitialized = true;
	setMinimumHeight(200);
	setMinimumWidth(200);
	requestUpdate();
}

Direct2DWindow::~Direct2DWindow() {}

QPaintEngine* Direct2DWindow::paintEngine() const
{
	return engine.get();
}

void Direct2DWindow::onPaint()
{
	QPainter p(this);
	p.setPen(Qt::red);
	p.drawLine(0, 0, QWindow::width(), QWindow::height());
	requestUpdate();
}

bool Direct2DWindow::init()
{
	return true;
}

void Direct2DWindow::flush()
{
	m_context->Flush();
}

int Direct2DWindow::metric(PaintDeviceMetric metric) const
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

void Direct2DWindow::setupSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};

	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

	HRESULT hr = DirectContext::instance().dxgiFactory()->CreateSwapChainForHwnd(
		DirectContext::instance().d3dDevice(), // [in]   IUnknown *pDevice
		m_hwnd,                                // [in]   HWND hWnd
		&desc,                                 // [in]   const DXGI_SWAP_CHAIN_DESC1 *pDesc
		nullptr, // [in]   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
		nullptr, // [in]   IDXGIOutput *pRestrictToOutput
		m_swapChain.ReleaseAndGetAddressOf()); // [out]  IDXGISwapChain1 **ppSwapChain

	if (FAILED(hr)) {
		qWarning("%s: Could not create swap chain: %#lx", __FUNCTION__, hr);
		assert(false);
	}
}

void Direct2DWindow::resizeSwapChain(const QSize& size)
{
	if (m_deviceInitialized) {
		m_context->SetTarget(nullptr);
		if (!m_swapChain)
			return;

		HRESULT hr = m_swapChain->ResizeBuffers(0,
			UINT(size.width()),
			UINT(size.height()),
			DXGI_FORMAT_UNKNOWN,
			0);
		if (FAILED(hr))
			qWarning("%s: Could not resize swap chain: %#lx", __FUNCTION__, hr);

		ComPtr<IDXGISurface1> backBufferSurface;
		hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferSurface));
		if (FAILED(hr)) {
			qWarning("%s: Could not query backbuffer for DXGI Surface: %#lx", __FUNCTION__, hr);
			return;
		}
		ComPtr<ID2D1Bitmap1> backBufferBitmap;
		hr = m_context->CreateBitmapFromDxgiSurface(backBufferSurface.Get(),
			nullptr,
			backBufferBitmap.GetAddressOf());
		if (FAILED(hr)) {
			qWarning("%s: Could not create Direct2D Bitmap from DXGI Surface: %#lx",
				__FUNCTION__,
				hr);
			return;
		}
		m_context->SetTarget(backBufferBitmap.Get());
	}
}

void Direct2DWindow::recreateTarget()
{
	if (m_deviceInitialized) {
		setupSwapChain();
		HRESULT hr = DirectContext::instance()
			.d2dDevice()
			->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				m_context.ReleaseAndGetAddressOf());
		m_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		m_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
		m_context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
		if (FAILED(hr)) {
			qWarning("%s: Couldn't create Direct2D Device context: %#lx", __FUNCTION__, hr);
			assert(false);
		}
		else {
			engine.reset(new Direct2DPaintEngine(this));
		}
	}
}

void Direct2DWindow::present()
{
	std::ignore = m_swapChain->Present(0, 0);
}

void Direct2DWindow::resizeEvent(QResizeEvent* event)
{
	int width = std::round(devicePixelRatioF() * event->size().width());
	int height = std::round(devicePixelRatioF() * event->size().height());
	QSize size{ width, height };
	resizeSwapChain(size);
}

bool Direct2DWindow::event(QEvent* event)
{
	if (event->type() == QEvent::UpdateRequest || event->type() == QEvent::Paint) {
		onPaint();
		present();
		return true;
	}
	return QWindow::event(event);
}
