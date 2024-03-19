#include "direct2dengine.h"
#include <QGlyphRun>
#include <QRawFont>
#include <QVarLengthArray>
#include "direct2dqthelper.h"
#include "directcontext.h"
#include "os.h"
#include "qpainterpath.h"
#include <comdef.h>
#include <dwrite.h>
#include <qglobal.h>
#include <wingdi.h>

ComPtr<ID2D1Bitmap> Direct2DPaintEngine::QPixmapToD2D1Bitmap(const QPixmap& pixmap)
{
	QImage image = pixmap.toImage();
	return fromImage(image);
}

void Direct2DPaintEngine::updateBrush(const QBrush& brush, bool force)
{
	if (force || m_brush.qbrush != brush) {
		m_brush.brush.Reset();
		m_brush.brush = toD2dBrush(brush);
		if (m_brush.brush)
			m_brush.brush->SetOpacity((FLOAT)state->opacity());
		m_brush.qbrush = brush;
	}
}

void Direct2DPaintEngine::updatePen(const QPen& newPen, bool force)
{
	if (force || m_pen.qpen != newPen) {
		m_pen.qpen = newPen;
		m_pen.reset();

		if (newPen.style() == Qt::NoPen)
			return;

		m_pen.brush = toD2dBrush(newPen.brush());
		if (!m_pen.brush)
			return;

		m_pen.brush->SetOpacity(FLOAT(state->opacity()));

		D2D1_STROKE_STYLE_PROPERTIES1 props = {};

		switch (newPen.capStyle()) {
		case Qt::SquareCap:
			props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_SQUARE;
			break;
		case Qt::RoundCap:
			props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_ROUND;
			break;
		case Qt::FlatCap:
		default:
			props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;
			break;
		}

		switch (newPen.joinStyle()) {
		case Qt::BevelJoin:
			props.lineJoin = D2D1_LINE_JOIN_BEVEL;
			break;
		case Qt::RoundJoin:
			props.lineJoin = D2D1_LINE_JOIN_ROUND;
			break;
		case Qt::MiterJoin:
		default:
			props.lineJoin = D2D1_LINE_JOIN_MITER;
			break;
		}

		props.miterLimit = FLOAT(newPen.miterLimit() * qreal(2.0)); // D2D and Qt miter specs differ
		props.dashOffset = FLOAT(newPen.dashOffset());

		if (newPen.widthF() == 0)
			props.transformType = D2D1_STROKE_TRANSFORM_TYPE_HAIRLINE;
		else if (newPen.isCosmetic())
			props.transformType = D2D1_STROKE_TRANSFORM_TYPE_FIXED;
		else
			props.transformType = D2D1_STROKE_TRANSFORM_TYPE_NORMAL;

		switch (newPen.style()) {
		case Qt::SolidLine:
			props.dashStyle = D2D1_DASH_STYLE_SOLID;
			break;

		case Qt::DashLine:
		case Qt::DotLine:
		case Qt::DashDotLine:
		case Qt::DashDotDotLine:
			if (newPen.widthF() <= 1.0)
				props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;

			props.dashStyle = (D2D1_DASH_STYLE)(newPen.style() - 1);

			break;
		default:
			props.dashStyle = D2D1_DASH_STYLE_CUSTOM;
			//TODO
			break;
		}

		HRESULT hr;
		hr = factory()->CreateStrokeStyle(props, nullptr, 0, &m_pen.strokeStyle);

		if (FAILED(hr))
			qWarning("%s: Could not create stroke style: %#lx", __FUNCTION__, hr);
	}
}

void Direct2DPaintEngine::initBrushAndPen()
{
	updatePen(state->pen(), true);
	updateBrush(state->brush(), true);
}

Direct2DPaintEngine::Direct2DPaintEngine(IDirect2DDeviceContext* rt,
	QPaintEngine::PaintEngineFeatures caps)
	: QPaintEngine(caps)
	, d(rt)
{
	QPaintEngine::PaintEngineFeatures unsupported = QPaintEngine::PorterDuff
		| QPaintEngine::BlendModes
		| QPaintEngine::RasterOpModes
		| QPaintEngine::PerspectiveTransform
		| QPaintEngine::PaintOutsidePaintEvent;
	QPaintEngine::PaintEngineFeatures supported = QPaintEngine::AlphaBlend
		| QPaintEngine::Antialiasing
		| QPaintEngine::BrushStroke
		| QPaintEngine::ConicalGradientFill
		| QPaintEngine::ConstantOpacity
		| QPaintEngine::LinearGradientFill
		| QPaintEngine::MaskedBrush
		| QPaintEngine::ObjectBoundingModeGradients
		| QPaintEngine::PainterPaths
		| QPaintEngine::PatternBrush
		| QPaintEngine::PatternTransform
		| QPaintEngine::PixmapTransform
		| QPaintEngine::PrimitiveTransform
		| QPaintEngine::RadialGradientFill;

	gccaps = (supported | ~unsupported);
}

Direct2DPaintEngine::~Direct2DPaintEngine() {}

bool Direct2DPaintEngine::begin(QPaintDevice* pdev)
{
	UNUSED(pdev);
	if (!d || !d->dc())
		return false;
	d->begin();
	d->dc()->SetTransform(D2D1::Matrix3x2F::Identity());
	initBrushAndPen();
	setActive(true);
	return true;
}

bool Direct2DPaintEngine::end()
{
	return d->end();
}

static QList<D2D1_GRADIENT_STOP> qGradientStopsToD2DStops(const QGradientStops& qstops)
{
	QList<D2D1_GRADIENT_STOP> stops(qstops.count());
	for (int i = 0, count = stops.size(); i < count; ++i) {
		stops[i].position = FLOAT(qstops.at(i).first);
		stops[i].color = toD2DColorF(qstops.at(i).second);
	}
	return stops;
}

ComPtr<ID2D1Brush> Direct2DPaintEngine::toD2dBrush(const QBrush& newBrush)
{
	HRESULT hr;
	ComPtr<ID2D1Brush> result{};

	switch (newBrush.style()) {
	case Qt::NoBrush:
		break;

	case Qt::SolidPattern: {
		ComPtr<ID2D1SolidColorBrush> solid;

		hr = d->dc()->CreateSolidColorBrush(toD2DColorF(newBrush.color()), solid.GetAddressOf());
		if (FAILED(hr)) {
			qWarning("%s: Could not create solid color brush: %#lx", __FUNCTION__, hr);
			break;
		}

		hr = solid.As(&result);
		if (FAILED(hr))
			qWarning("%s: Could not convert solid color brush: %#lx", __FUNCTION__, hr);
	} break;

	case Qt::Dense1Pattern:
	case Qt::Dense2Pattern:
	case Qt::Dense3Pattern:
	case Qt::Dense4Pattern:
	case Qt::Dense5Pattern:
	case Qt::Dense6Pattern:
	case Qt::Dense7Pattern:
	case Qt::HorPattern:
	case Qt::VerPattern:
	case Qt::CrossPattern:
	case Qt::BDiagPattern:
	case Qt::FDiagPattern:
	case Qt::DiagCrossPattern: {
		ComPtr<ID2D1BitmapBrush1> bitmapBrush;
		D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = { D2D1_EXTEND_MODE_WRAP,
															   D2D1_EXTEND_MODE_WRAP,
															   interpolationMode() };

		QImage brushImg = newBrush.textureImage();
		brushImg.setColor(0, newBrush.color().rgba());
		brushImg.setColor(1, qRgba(0, 0, 0, 0));

		ComPtr<ID2D1Bitmap> bitmap;
		bitmap = fromImage(brushImg);
		if (!bitmap) {
			qWarning("%s: Could not create Direct2D bitmap from Qt pattern brush image",
				__FUNCTION__);
			break;
		}

		hr = d->dc()->CreateBitmapBrush(bitmap.Get(), bitmapBrushProperties, &bitmapBrush);
		if (FAILED(hr)) {
			qWarning("%s: Could not create Direct2D bitmap brush for Qt pattern brush: %#lx",
				__FUNCTION__,
				hr);
			break;
		}

		hr = bitmapBrush.As(&result);
		if (FAILED(hr))
			qWarning("%s: Could not convert Direct2D bitmap brush for Qt pattern brush: %#lx",
				__FUNCTION__,
				hr);
		break;
	}

	case Qt::LinearGradientPattern: {
		ComPtr<ID2D1LinearGradientBrush> linear;
		const auto* qlinear = static_cast<const QLinearGradient*>(newBrush.gradient());

		D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linearGradientBrushProperties;
		ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

		linearGradientBrushProperties.startPoint = tod2dPoint2f(qlinear->start());
		linearGradientBrushProperties.endPoint = tod2dPoint2f(qlinear->finalStop());

		const QList<D2D1_GRADIENT_STOP> stops = qGradientStopsToD2DStops(qlinear->stops());

		hr = d->dc()->CreateGradientStopCollection(stops.constData(),
			UINT32(stops.size()),
			&gradientStopCollection);
		if (FAILED(hr)) {
			qWarning("%s: Could not create gradient stop collection for linear gradient: %#lx",
				__FUNCTION__,
				hr);
			break;
		}

		hr = d->dc()->CreateLinearGradientBrush(linearGradientBrushProperties,
			gradientStopCollection.Get(),
			&linear);
		if (FAILED(hr)) {
			qWarning("%s: Could not create Direct2D linear gradient brush: %#lx", __FUNCTION__, hr);
			break;
		}

		hr = linear.As(&result);
		if (FAILED(hr)) {
			qWarning("%s: Could not convert Direct2D linear gradient brush: %#lx", __FUNCTION__, hr);
			break;
		}

		break;
	}
	case Qt::RadialGradientPattern: {
		ComPtr<ID2D1RadialGradientBrush> radial;
		const auto* qradial = static_cast<const QRadialGradient*>(newBrush.gradient());

		D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radialGradientBrushProperties;
		ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

		radialGradientBrushProperties.center = tod2dPoint2f(qradial->center());
		radialGradientBrushProperties.gradientOriginOffset = tod2dPoint2f(qradial->focalPoint()
			- qradial->center());
		radialGradientBrushProperties.radiusX = FLOAT(qradial->radius());
		radialGradientBrushProperties.radiusY = FLOAT(qradial->radius());

		const QList<D2D1_GRADIENT_STOP> stops = qGradientStopsToD2DStops(qradial->stops());

		hr = d->dc()->CreateGradientStopCollection(stops.constData(),
			stops.size(),
			&gradientStopCollection);
		if (FAILED(hr)) {
			qWarning("%s: Could not create gradient stop collection for radial gradient: %#lx",
				__FUNCTION__,
				hr);
			break;
		}

		hr = d->dc()->CreateRadialGradientBrush(radialGradientBrushProperties,
			gradientStopCollection.Get(),
			&radial);
		if (FAILED(hr)) {
			qWarning("%s: Could not create Direct2D radial gradient brush: %#lx", __FUNCTION__, hr);
			break;
		}

		radial.As(&result);
		if (FAILED(hr)) {
			qWarning("%s: Could not convert Direct2D radial gradient brush: %#lx", __FUNCTION__, hr);
			break;
		}

		break;
	}
	case Qt::ConicalGradientPattern:
		break;

	case Qt::TexturePattern: {
		ComPtr<ID2D1BitmapBrush1> bitmapBrush;
		D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = { D2D1_EXTEND_MODE_WRAP,
															   D2D1_EXTEND_MODE_WRAP,
															   interpolationMode() };

		QImage i{ newBrush.texture().toImage() };
		ComPtr<ID2D1Bitmap> bitmap = fromImage(i);
		if (!bitmap)
			break;
		hr = d->dc()->CreateBitmapBrush(bitmap.Get(), bitmapBrushProperties, &bitmapBrush);

		if (FAILED(hr)) {
			qWarning("%s: Could not create texture brush: %#lx", __FUNCTION__, hr);
			break;
		}

		hr = bitmapBrush.As(&result);
		if (FAILED(hr))
			qWarning("%s: Could not convert texture brush: %#lx", __FUNCTION__, hr);
	} break;
	}

	if (result && !newBrush.transform().isIdentity())
		result->SetTransform(toD2dMatrix3x2F(newBrush.transform()));

	return result;
}

ComPtr<ID2D1Bitmap> Direct2DPaintEngine::fromImage(QImage& image)
{
	if (image.format() != QImage::Format_ARGB32_Premultiplied)
		image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

	// Create a D2D1Bitmap from the QImage data
	D2D1_SIZE_U size = { static_cast<UINT32>(image.width()), static_cast<UINT32>(image.height()) };
	D2D1_BITMAP_PROPERTIES properties
		= D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
			D2D1_ALPHA_MODE_PREMULTIPLIED),
			image.physicalDpiX(),
			image.physicalDpiY());

	ComPtr<ID2D1Bitmap> bitmap;
	HRESULT hr = d->dc()->CreateBitmap(size,
		image.constBits(),
		image.bytesPerLine(),
		&properties,
		&bitmap);
	if (FAILED(hr)) {
		return nullptr;
	}

	return bitmap;
}

ComPtr<IDWriteFontFace> Direct2DPaintEngine::getFont()
{
	const QFont& font = state->font();
	if (fontCache.contains(font))
		return fontCache[font];

	ComPtr<IDWriteFontFace> fontFace = nullptr;
	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -font.pointSize();
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfWeight = font.weight();
	lf.lfItalic = font.italic();
	lf.lfUnderline = font.underline();
	lf.lfStrikeOut = font.strikeOut();
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_TT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = CLEARTYPE_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	std::wstring fontWName = state->font().family().toStdWString();
	wcscpy_s(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), fontWName.data());
	ComPtr<IDWriteFont> dwriteFont;
	HRESULT hr = DirectContext::instance().IDWriteGdiInterop()->CreateFontFromLOGFONT(&lf,
		&dwriteFont);
	if (FAILED(hr)) {
		_com_error err(hr);
		LPCTSTR errMsg = err.ErrorMessage();
		qDebug("%s: CreateFontFromLOGFONT failed: %#lx = %ls", __FUNCTION__, hr, errMsg);
		wcscpy_s(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), L"Arial");
		hr = DirectContext::instance().IDWriteGdiInterop()->CreateFontFromLOGFONT(&lf,
			&dwriteFont);
		if (FAILED(hr)) {
			_com_error err2(hr);
			LPCTSTR errMsg2 = err2.ErrorMessage();
			qDebug("%s: CreateFontFromLOGFONT failed for Arial: %#lx = %ls",
				__FUNCTION__,
				hr,
				errMsg2);
			return fontFace;
		}
	}

	hr = dwriteFont->CreateFontFace(&fontFace);
	if (FAILED(hr)) {

		qDebug("%s: CreateFontFace failed: %#lx", __FUNCTION__, hr);
		return fontFace;
	}
	fontCache[font] = fontFace;

	return fontFace;
}

void Direct2DPaintEngine::updateCompositionMode(QPainter::CompositionMode mode)
{
	switch (mode) {
	case QPainter::CompositionMode_Source:
		d->dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		break;
	case QPainter::CompositionMode_SourceOver:
		d->dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
		break;

	default:
		d->dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		break;
	}
}

void Direct2DPaintEngine::updateBrushOrigin(const QPointF& brushOrigin)
{
	negateCurrentBrushOrigin();
	applyBrushOrigin(brushOrigin);
}

void Direct2DPaintEngine::negateCurrentBrushOrigin()
{
	if (m_brush.brush && !currentBrushOrigin.isNull()) {
		D2D1_MATRIX_3X2_F transform;
		m_brush.brush->GetTransform(&transform);

		m_brush.brush->SetTransform(*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
			* D2D1::Matrix3x2F::Translation(FLOAT(-currentBrushOrigin.x()),
				FLOAT(-currentBrushOrigin.y())));
	}
}

void Direct2DPaintEngine::applyBrushOrigin(const QPointF& origin)
{
	if (m_brush.brush && !origin.isNull()) {
		D2D1_MATRIX_3X2_F transform;
		m_brush.brush->GetTransform(&transform);

		m_brush.brush->SetTransform(
			*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
			* D2D1::Matrix3x2F::Translation(FLOAT(origin.x()), FLOAT(origin.y())));
	}

	currentBrushOrigin = origin;
}
void Direct2DPaintEngine::updateState(const QPaintEngineState& sstate)
{
	if (sstate.state().testFlag(QPaintEngine::DirtyBrush)) {
		updateBrush(sstate.brush());
	}
	if (sstate.state().testFlag(QPaintEngine::DirtyPen)) {
		updatePen(sstate.pen());
	}
	if (sstate.state().testFlag(QPaintEngine::DirtyOpacity)) {
		updateBrush(sstate.brush());
		updatePen(sstate.pen());
	}
	if (sstate.state().testFlag(QPaintEngine::DirtyCompositionMode)) {
		updateBrush(sstate.brush());
		updatePen(sstate.pen());
		updateCompositionMode(sstate.compositionMode());
	}
	if (sstate.state().testFlag(QPaintEngine::DirtyTransform)) {
		d->dc()->SetTransform(toD2dMatrix3x2F(sstate.transform()));
	}
	if (sstate.state().testFlag(QPaintEngine::DirtyHints)) {
		d->dc()->SetAntialiasMode(antialiasMode());
	}
}
void Direct2DPaintEngine::drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
{
	drawImage(r, pm.toImage(), sr);
}

void Direct2DPaintEngine::drawPoints(const QPointF* points, int pointCount)
{
	if (m_brush.brush) {
		for (int i = 0; i < pointCount; i++) {
			d->dc()->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(points[i].x(), points[i].y()),
				1.0f,
				1.0f),
				m_brush.brush.Get(),
				1.0f);
		}
	}
}

void Direct2DPaintEngine::drawPoints(const QPoint* points, int pointCount)
{
	if (m_pen.brush) {
		for (int i = 0; i < pointCount; i++) {
			d->dc()->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(points[i].x(), points[i].y()),
				1.0f,
				1.0f),
				m_pen.brush.Get(),
				1.0f);
		}
	}
}

void Direct2DPaintEngine::drawPolygon(const QPointF* points,
	int pointCount,
	QPaintEngine::PolygonDrawMode mode)
{
	UNUSED(points);
	UNUSED(pointCount);
	UNUSED(mode);
}

void Direct2DPaintEngine::drawPolygon(const QPoint* points,
	int pointCount,
	QPaintEngine::PolygonDrawMode mode)
{
	UNUSED(points);
	UNUSED(pointCount);
	UNUSED(mode);
}

void Direct2DPaintEngine::drawRects(const QRectF* rects, int rectCount)
{
	for (int i = 0; i < rectCount; ++i) {
		D2D1_RECT_F rect = toD2dRectF(
			rects[i].adjusted(PIXEL_SNAP, PIXEL_SNAP, PIXEL_SNAP, PIXEL_SNAP));
		if (state->brush() != Qt::NoBrush && m_brush.brush)
			d->dc()->FillRectangle(rect, m_brush.brush.Get());
		if (state->pen() != Qt::NoPen && m_pen.brush && m_pen.strokeStyle)
			d->dc()->DrawRectangle(rect,
				m_pen.brush.Get(),
				(FLOAT)m_pen.qpen.widthF(),
				m_pen.strokeStyle.Get());
	}
}

void Direct2DPaintEngine::drawRects(const QRect* rects, int rectCount)
{
	for (int i = 0; i < rectCount; ++i) {
		D2D1_RECT_F rect = toD2dRectF(rects[i]);
		if (state->brush() != Qt::NoBrush && m_brush.brush)
			d->dc()->FillRectangle(rect, m_brush.brush.Get());
		if (state->pen() != Qt::NoPen && m_pen.brush && m_pen.strokeStyle)
			d->dc()->DrawRectangle(rect,
				m_pen.brush.Get(),
				(FLOAT)m_pen.qpen.widthF(),
				m_pen.strokeStyle.Get());
	}
}

void Direct2DPaintEngine::drawTextItem(const QPointF& p, const QTextItem& textItem)
{
	ComPtr<IDWriteFontFace> fontFace = getFont();
	if (fontFace) {
		const QString text = textItem.text();
		QRawFont raw = QRawFont::fromFont(state->font());
		QList<quint32> indexes = raw.glyphIndexesForString(text);
		QList<QPointF> adv = raw.advancesForGlyphIndexes(indexes);
		QVarLengthArray<UINT16> glyphIndices(indexes.size());
		QVarLengthArray<FLOAT> glyphAdvances(indexes.size());
		QVarLengthArray<DWRITE_GLYPH_OFFSET> glyphOffsets(indexes.size());
		if (adv.size() == glyphIndices.size()) {
			for (long long i = 0; i < glyphIndices.size(); i++) {
				glyphIndices[i] = UINT16(indexes[i]);
				glyphAdvances[i] = FLOAT(adv[i].x());
				// nao sei oq eh isso, oq isso faz, se um dia alguem mais instruido ver isso e souber fazer corretamente por favor
				glyphOffsets[i].advanceOffset = 0;
				glyphOffsets[i].ascenderOffset = 0;
			}
			DWRITE_GLYPH_RUN glyphRun;
			glyphRun.fontFace = fontFace.Get();
			glyphRun.fontEmSize = raw.pixelSize();
			glyphRun.glyphCount = indexes.size();
			glyphRun.glyphIndices = glyphIndices.constData();
			glyphRun.glyphAdvances = glyphAdvances.constData();
			glyphRun.glyphOffsets = nullptr;
			glyphRun.isSideways = FALSE;
			glyphRun.bidiLevel = 0;
			d->dc()->DrawGlyphRun(tod2dPoint2f(p),
				&glyphRun,
				m_pen.brush.Get(),
				DWRITE_MEASURING_MODE_NATURAL);
		}
	}
}

void Direct2DPaintEngine::drawTiledPixmap(const QRectF& rect,
	const QPixmap& pixmap,
	const QPointF& p)
{
	if (pixmap.isNull())
		return;

	// Convert QPixmap to ID2D1Bitmap
	ComPtr<ID2D1Bitmap> d2dBitmap = QPixmapToD2D1Bitmap(pixmap);
	if (!d2dBitmap)
		return;

	D2D1_SIZE_F size = d2dBitmap->GetSize();
	D2D1_RECT_F sourceRect = D2D1::RectF(0, 0, size.width, size.height);
	D2D1_RECT_F destRect;

	// Calculate the starting point
	float startX = rect.left() + p.x();
	float startY = rect.top() + p.y();

	// Tile the pixmap across the specified rect
	for (float y = startY; y < rect.bottom(); y += size.height) {
		for (float x = startX; x < rect.right(); x += size.width) {
			destRect = D2D1::RectF(x, y, x + size.width, y + size.height);

			// Adjust the destination rect if it goes outside of the specified rect
			if (destRect.right > rect.right()) {
				destRect.right = rect.right();
			}
			if (destRect.bottom > rect.bottom()) {
				destRect.bottom = rect.bottom();
			}

			// Draw the bitmap
			d->dc()->DrawBitmap(d2dBitmap.Get(),
				destRect,
				1.0f,
				D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				&sourceRect);
		}
	}
}

void Direct2DPaintEngine::drawD2DBitmap(ID2D1Bitmap* bitmap,
	const D2D1_RECT_F& dest,
	FLOAT opacity,
	D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
	const D2D1_RECT_F* src)
{
	d->dc()->DrawBitmap(bitmap, dest, opacity, interpolationMode, src);
}

void Direct2DPaintEngine::drawLinePath(const QPointF* path, const size_t count)
{
	ComPtr<ID2D1PathGeometry> d2dPath;
	if (SUCCEEDED(factory()->CreatePathGeometry(d2dPath.GetAddressOf()))) {
		ID2D1GeometrySink* pSink = NULL;
		if (SUCCEEDED(d2dPath->Open(&pSink))) {
			if (count) {
				pSink->BeginFigure(D2D1::Point2F(path[0].x(), path[0].y()),
					D2D1_FIGURE_BEGIN_FILLED);
				for (size_t i = 1; i < count; i++) {
					pSink->AddLine(D2D1::Point2F(path[i].x(), path[i].y()));
				}
				pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
				std::ignore = pSink->Close();

				if (m_brush.brush && m_brush.qbrush != Qt::NoBrush) {
					d->dc()->FillGeometry(d2dPath.Get(), m_brush.brush.Get());
				}
				if (m_pen.brush && m_pen.strokeStyle) {
					d->dc()->DrawGeometry(d2dPath.Get(),
						m_pen.brush.Get(),
						m_pen.qpen.widthF(),
						m_pen.strokeStyle.Get());
				}
				SafeRelease(&pSink);
			}
		}
	}
}

QPaintEngine::Type Direct2DPaintEngine::type() const
{
	return GTRDirect2D;
}

void Direct2DPaintEngine::drawEllipse(const QRectF& rect)
{
	UNUSED(rect);
}

void Direct2DPaintEngine::drawEllipse(const QRect& rect)
{
	UNUSED(rect);
}

void Direct2DPaintEngine::drawImage(const QRectF& rectangle,
	const QImage& image,
	const QRectF& sr,
	Qt::ImageConversionFlags flags)
{
	(void)flags;
	QImage renderImage = image;
	if (renderImage.format() != QImage::Format_ARGB32_Premultiplied)
		renderImage = renderImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

	FLOAT dpiY = 96.0;
	FLOAT dpiX = 96.0;
	d->dc()->GetDpi(&dpiX, &dpiY);
	int width = sr.width();
	int height = sr.height();
	D2D1_SIZE_U size{ (UINT32)width, (UINT32)height };
	D2D1_BITMAP_PROPERTIES props;
	D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED);
	props.pixelFormat = pixelFormat;
	props.dpiX = dpiX;
	props.dpiY = dpiY;
	ID2D1Bitmap* bitmap;
	if (SUCCEEDED(d->dc()->CreateBitmap(size, props, &bitmap))) {
		UINT32 pitch = image.sizeInBytes() / height;
		if (SUCCEEDED(bitmap->CopyFromMemory(nullptr, image.bits(), pitch))) {
			d->dc()->DrawBitmap(bitmap,
				D2D1::RectF(rectangle.left(),
					rectangle.top(),
					rectangle.right(),
					rectangle.bottom()));
		}
	}
	bitmap->Release();
}

void Direct2DPaintEngine::drawLines(const QLineF* lines, int lineCount)
{
	if (m_pen.brush && m_pen.strokeStyle) {
		for (int i = 0; i < lineCount; ++i) {
			QPointF p1{ lines[i].p1() };
			QPointF p2{ lines[i].p2() };
			adjustLine(&p1, &p2);
			D2D1_POINT_2F dp1 = tod2dPoint2f(p1);
			D2D1_POINT_2F dp2 = tod2dPoint2f(p2);
			d->dc()->DrawLine(dp1,
				dp2,
				m_pen.brush.Get(),
				m_pen.qpen.widthF(),
				m_pen.strokeStyle.Get());
		}
	}
}

void Direct2DPaintEngine::drawLines(const QLine* lines, int lineCount)
{
	if (m_pen.brush && m_pen.strokeStyle) {
		for (int i = 0; i < lineCount; ++i) {
			QPointF p1{ lines[i].p1() };
			QPointF p2{ lines[i].p2() };
			adjustLine(&p1, &p2);
			D2D1_POINT_2F dp1 = tod2dPoint2f(p1);
			D2D1_POINT_2F dp2 = tod2dPoint2f(p2);
			d->dc()->DrawLine(dp1,
				dp2,
				m_pen.brush.Get(),
				m_pen.qpen.widthF(),
				m_pen.strokeStyle.Get());
		}
	}
}

void Direct2DPaintEngine::drawPath(const QPainterPath& path)
{
	ComPtr<ID2D1PathGeometry> d2dPath;
	if (SUCCEEDED(factory()->CreatePathGeometry(d2dPath.GetAddressOf()))) {
		ID2D1GeometrySink* pSink = NULL;
		if (SUCCEEDED(d2dPath->Open(&pSink))) {
			pSink->SetFillMode(path.fillRule() == Qt::WindingFill ? D2D1_FILL_MODE_WINDING
				: D2D1_FILL_MODE_ALTERNATE);

			for (int i = 0; i < path.elementCount(); ++i) {
				const QPainterPath::Element& element = path.elementAt(i);
				switch (element.type) {
				case QPainterPath::MoveToElement:
					if (i) {
						pSink->EndFigure(D2D1_FIGURE_END_OPEN);
					}
					pSink->BeginFigure(D2D1::Point2F(element.x, element.y),
						D2D1_FIGURE_BEGIN_FILLED);
					break;
				case QPainterPath::LineToElement:
					pSink->AddLine(D2D1::Point2F(element.x, element.y));
					break;
				case QPainterPath::CurveToElement: {
					// Cubic Bezier curve requires two more points (CurveToDataElement)
					if (i + 2 < path.elementCount()) {
						const QPainterPath::Element& controlPoint1 = path.elementAt(++i);
						const QPainterPath::Element& controlPoint2 = path.elementAt(++i);
						pSink->AddBezier(
							D2D1::BezierSegment(D2D1::Point2F(controlPoint1.x, controlPoint1.y),
								D2D1::Point2F(element.x, element.y),
								D2D1::Point2F(controlPoint2.x, controlPoint2.y)));
					}
				} break;
				case QPainterPath::CurveToDataElement:
					// This case is handled within CurveToElement
					break;
				}
			}

			pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
			std::ignore = pSink->Close();

			if (m_brush.brush && m_brush.qbrush != Qt::NoBrush) {
				d->dc()->FillGeometry(d2dPath.Get(), m_brush.brush.Get());
			}
			if (m_pen.brush && m_pen.strokeStyle) {
				d->dc()->DrawGeometry(d2dPath.Get(),
					m_pen.brush.Get(),
					m_pen.qpen.widthF(),
					m_pen.strokeStyle.Get());
			}
			SafeRelease(&pSink);
		}
	}
}
