#ifndef DIRECT2DWINDOW_H
#define DIRECT2DWINDOW_H
#include <QPaintDeviceWindow>
#include <QSharedPointer>
#include "direct2ddevicecontext.h"
#include "direct2dengine.h"
#include "directcontext.h"

class Direct2DWindow : public QWindow, public QPaintDevice, public IDirect2DDeviceContext
{
private:
	HWND m_hwnd;
	ComPtr<IDXGISwapChain1> m_swapChain;
	bool m_deviceInitialized;
	QScopedPointer<Direct2DPaintEngine> engine;

protected:
	int metric(PaintDeviceMetric metric) const override;
	void setupSwapChain();
	void resizeSwapChain(const QSize& size);
	void recreateTarget() override;
	void present();
	void resizeEvent(QResizeEvent*) override;
	bool event(QEvent* event) override;

public:
	Direct2DWindow(QWindow* parent);
	~Direct2DWindow();
	QPaintEngine* paintEngine() const override;
	void onPaint();
	bool init();
	void flush();

	// QObject interface
};

#endif // DIRECT2DWINDOW_H
