#pragma once
#include <QtCore/qscopedpointer.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <qglobal.h>
#include <wrl.h>
#include "qpaintengine.h"
#include "qpainter.h"
#include "src/direct2d/direct2ddevicecontext.h"
#include "src/direct2d/direct2dqthelper.h"
#include "QHash"

namespace std {
	template<>
	struct hash<QFont>
	{
		inline size_t operator()(const QFont& f) const { return qHash(f); }
	};
}

using Microsoft::WRL::ComPtr;
static const qreal PIXEL_SNAP = 0.5;
class Direct2DPaintEngine final : public QPaintEngine
{
private:
	IDirect2DDeviceContext* d;
	ComPtr<ID2D1Bitmap> QPixmapToD2D1Bitmap(const QPixmap& pixmap);
	void updateBrush(const QBrush& brush, bool force = false);
	void updatePen(const QPen& pen, bool force = false);
	void initBrushAndPen();
	ComPtr<ID2D1Brush> toD2dBrush(const QBrush& newBrush);
	void updateCompositionMode(QPainter::CompositionMode mode);
	void updateBrushOrigin(const QPointF& brushOrigin);
	void negateCurrentBrushOrigin();
	void applyBrushOrigin(const QPointF& origin);
	QPointF currentBrushOrigin;
	ComPtr<ID2D1Bitmap> fromImage(QImage& image);
	std::unordered_map<QFont, ComPtr<IDWriteFontFace>> fontCache;
	ComPtr<IDWriteFontFace> getFont();
	struct brush
	{
		QBrush qbrush;
		ComPtr<ID2D1Brush> brush;
		ID2D1Brush** getReset() { return brush.ReleaseAndGetAddressOf(); }
	};
	struct pen
	{
		QPen qpen;
		ComPtr<ID2D1Brush> brush;
		ComPtr<ID2D1StrokeStyle1> strokeStyle;
		void reset()
		{
			brush.Reset();
			strokeStyle.Reset();
		}
	};

	inline void adjustLine(QPointF* p1, QPointF* p2)
	{
		if (isLinePositivelySloped(*p1, *p2)) {
			p1->ry() -= qreal(1.0);
			p2->ry() -= qreal(1.0);
		}
	}

	inline D2D1_POINT_2F adjusted(const QPointF& point)
	{
		static const QPointF adjustment(PIXEL_SNAP, PIXEL_SNAP);

		return tod2dPoint2f(point + adjustment);
	}

	inline bool isLinePositivelySloped(const QPointF& p1, const QPointF& p2)
	{
		if (p2.x() > p1.x())
			return p2.y() < p1.y();

		if (p1.x() > p2.x())
			return p1.y() < p2.y();

		return false;
	}

	brush m_brush;
	pen m_pen;

	inline D2D1_INTERPOLATION_MODE interpolationMode() const
	{
		return (state->renderHints() & QPainter::SmoothPixmapTransform)
			? D2D1_INTERPOLATION_MODE_LINEAR
			: D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
	}
	inline D2D1_ANTIALIAS_MODE antialiasMode() const
	{
		return (state->renderHints() & QPainter::Antialiasing) ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
			: D2D1_ANTIALIAS_MODE_ALIASED;
	}

public:
	Direct2DPaintEngine(IDirect2DDeviceContext* rt,
		QPaintEngine::PaintEngineFeatures caps = PaintEngineFeatures());
	~Direct2DPaintEngine();
	// Herdado por meio de QPaintEngine
	bool begin(QPaintDevice* pdev) override;
	bool end() override;
	void updateState(const QPaintEngineState& sstate) override;
	void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
	QPaintEngine::Type type() const override;
	virtual void drawEllipse(const QRectF& rect) override;
	virtual void drawEllipse(const QRect& rect) override;
	virtual void drawImage(const QRectF& rectangle,
		const QImage& image,
		const QRectF& sr,
		Qt::ImageConversionFlags flags = Qt::AutoColor) override;
	virtual void drawLines(const QLineF* lines, int lineCount) override;
	virtual void drawLines(const QLine* lines, int lineCount) override;
	virtual void drawPath(const QPainterPath& path) override;
	virtual void drawPoints(const QPointF* points, int pointCount) override;
	virtual void drawPoints(const QPoint* points, int pointCount) override;
	virtual void drawPolygon(const QPointF* points,
		int pointCount,
		QPaintEngine::PolygonDrawMode mode) override;
	virtual void drawPolygon(const QPoint* points,
		int pointCount,
		QPaintEngine::PolygonDrawMode mode) override;
	virtual void drawRects(const QRectF* rects, int rectCount) override;
	virtual void drawRects(const QRect* rects, int rectCount) override;
	virtual void drawTextItem(const QPointF& p, const QTextItem& textItem) override;
	virtual void drawTiledPixmap(const QRectF& rect,
		const QPixmap& pixmap,
		const QPointF& p) override;
	void drawD2DBitmap(ID2D1Bitmap* bitmap,
		const D2D1_RECT_F& dest,
		FLOAT opacity,
		D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
		const D2D1_RECT_F* src);
	void drawLinePath(const QPointF* path, const size_t count);
};
