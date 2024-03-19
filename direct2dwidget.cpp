#include "direct2dwidget.h"
#include <QResizeEvent>
#include "qpainter"
#include <qglobal.h>

Direct2DWidget::Direct2DWidget(QWidget* parent)
	: QWidget(parent)

{
	setMinimumHeight(200);
	setMinimumWidth(200);
	setAttribute(Qt::WA_NativeWindow);
	engine.reset();
	m_hwnd = reinterpret_cast<HWND>(winId());
	m_context = nullptr;
	m_swapChain = nullptr;
	m_deviceInitialized = false;
	setAttribute(Qt::WA_NoSystemBackground);
	setAutoFillBackground(true);
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_OpaquePaintEvent);

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
}

Direct2DWidget::~Direct2DWidget()
{

}

QPaintEngine* Direct2DWidget::paintEngine() const
{
	return engine.get();
}

void Direct2DWidget::resizeEvent(QResizeEvent* event)
{
	int width = std::round(devicePixelRatioF() * event->size().width());
	int height = std::round(devicePixelRatioF() * event->size().height());
	QSize size{ width, height };
	resizeSwapChain(size);
}

void Direct2DWidget::resizeSwapChain(const QSize& size)
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

void Direct2DWidget::flush()
{
	m_context->Flush();
}

void Direct2DWidget::recreateTarget()
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

void Direct2DWidget::present()
{
	std::ignore = m_swapChain->Present(0, 0);
}

bool Direct2DWidget::event(QEvent* event)
{
	if (event->type() == QEvent::Paint) {
		if (m_deviceInitialized) {
			bool result = QWidget::event(event);
			present();
			return result;
		}
		qWarning("%s: HwndRenderTarget not initialized!", __FUNCTION__);
		return false;
	}
	return QWidget::event(event);
}

bool Direct2DWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
	return QWidget::nativeEvent(eventType, message, result);
}

void Direct2DWidget::setupSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};

	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

	HRESULT hr = DirectContext::instance().dxgiFactory()->CreateSwapChainForHwnd(
		DirectContext::instance().d3dDevice(), // [in]   IUnknown *pDevice
		m_hwnd, // [in]   HWND hWnd
		&desc, // [in]   const DXGI_SWAP_CHAIN_DESC1 *pDesc
		nullptr, // [in]   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
		nullptr, // [in]   IDXGIOutput *pRestrictToOutput
		m_swapChain.ReleaseAndGetAddressOf()); // [out]  IDXGISwapChain1 **ppSwapChain

	if (FAILED(hr)) {
		qWarning("%s: Could not create swap chain: %#lx", __FUNCTION__, hr);
		assert(false);
	}
}

bool Direct2DWidget::init()
{
	return true;
}
