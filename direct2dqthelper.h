#ifndef DIRECT2DQTHELPER_H
#define DIRECT2DQTHELPER_H

#include "qcolor.h"
#include "qpoint.h"
#include "qtransform.h"
#include <d2d1_1helper.h>

inline D2D1::ColorF toD2DColorF(const QColor& c)
{
	return D2D1::ColorF(c.redF(), c.greenF(), c.blueF(), c.alphaF());
}


inline D2D1_POINT_2F tod2dPoint2f(const QPointF& qpoint)
{
	return D2D1::Point2F(qpoint.x(), qpoint.y());
}

inline D2D1_MATRIX_3X2_F toD2dMatrix3x2F(const QTransform& transform)
{
	return D2D1::Matrix3x2F(transform.m11(), transform.m12(),
		transform.m21(), transform.m22(),
		transform.m31(), transform.m32());
}

inline D2D1_RECT_F toD2dRectF(const QRectF& qrect)
{
	return D2D1::RectF(qrect.x(), qrect.y(), qrect.x() + qrect.width(), qrect.y() + qrect.height());
}

inline D2D1_RECT_F toD2dRectF(const FLOAT x, const FLOAT y, const FLOAT width, const FLOAT height)
{
	return D2D1::RectF(x, y, x + width, y + height);
}

template<class Interface>
inline void
SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != nullptr)
	{
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = nullptr;
	}
}

#endif // DIRECT2DQTHELPER_H
