#pragma once
#include <QSharedPointer>
#include "direct2ddevicecontext.h"
#include "direct2dengine.h"
#include "directcontext.h"
#include "qwidget.h"
#include <qglobal.h>

using Microsoft::WRL::ComPtr;

//use this as your base class or as it is
class Direct2DWidget : public QWidget, public IDirect2DDeviceContext
{
public:
	Direct2DWidget(QWidget* parent = nullptr);
	~Direct2DWidget();
	QPaintEngine* paintEngine() const override;
	QScopedPointer<Direct2DPaintEngine> engine;
	bool init();
	void flush();

protected:
	virtual void resizeEvent(QResizeEvent* event) override;
	bool event(QEvent* event) override;
	bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
	void setupSwapChain();
	void resizeSwapChain(const QSize& size);
	HWND m_hwnd;
	ComPtr<IDXGISwapChain1> m_swapChain;
	bool m_deviceInitialized;
	void recreateTarget() override;
	void present();
};
