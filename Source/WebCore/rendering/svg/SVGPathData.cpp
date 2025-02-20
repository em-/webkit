/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGPathData.h"

#include "Path.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SVGCircleElement.h"
#include "SVGEllipseElement.h"
#include "SVGLengthContext.h"
#include "SVGLineElement.h"
#include "SVGNames.h"
#include "SVGPathElement.h"
#include "SVGPathUtilities.h"
#include "SVGPolygonElement.h"
#include "SVGPolylineElement.h"
#include "SVGRectElement.h"
#include <wtf/HashMap.h>

namespace WebCore {

static void updatePathFromCircleElement(SVGElement* element, Path& path)
{
    ASSERT(is<SVGCircleElement>(element));

    SVGLengthContext lengthContext(element);
    RenderElement* renderer = element->renderer();
    if (!renderer)
        return;
    auto& style = renderer->style();
    float r = lengthContext.valueForLength(style.svgStyle().r());
    if (r > 0) {
        float cx = lengthContext.valueForLength(style.svgStyle().cx(), LengthModeWidth);
        float cy = lengthContext.valueForLength(style.svgStyle().cy(), LengthModeHeight);
        path.addEllipse(FloatRect(cx - r, cy - r, r * 2, r * 2));
    }
}

static void updatePathFromEllipseElement(SVGElement* element, Path& path)
{
    RenderElement* renderer = element->renderer();
    if (!renderer)
        return;
    auto& style = renderer->style();
    SVGLengthContext lengthContext(element);
    float rx = lengthContext.valueForLength(style.svgStyle().rx(), LengthModeWidth);
    if (rx <= 0)
        return;
    float ry = lengthContext.valueForLength(style.svgStyle().ry(), LengthModeHeight);
    if (ry <= 0)
        return;
    float cx = lengthContext.valueForLength(style.svgStyle().cx(), LengthModeWidth);
    float cy = lengthContext.valueForLength(style.svgStyle().cy(), LengthModeHeight);
    path.addEllipse(FloatRect(cx - rx, cy - ry, rx * 2, ry * 2));
}

static void updatePathFromLineElement(SVGElement* element, Path& path)
{
    SVGLineElement* line = downcast<SVGLineElement>(element);

    SVGLengthContext lengthContext(element);
    path.moveTo(FloatPoint(line->x1().value(lengthContext), line->y1().value(lengthContext)));
    path.addLineTo(FloatPoint(line->x2().value(lengthContext), line->y2().value(lengthContext)));
}

static void updatePathFromPathElement(SVGElement* element, Path& path)
{
    buildPathFromByteStream(downcast<SVGPathElement>(element)->pathByteStream(), path);
}

static void updatePathFromPolygonElement(SVGElement* element, Path& path)
{
    SVGPointList& points = downcast<SVGPolygonElement>(element)->animatedPoints()->values();
    if (points.isEmpty())
        return;

    path.moveTo(points.first());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i));

    path.closeSubpath();
}

static void updatePathFromPolylineElement(SVGElement* element, Path& path)
{
    SVGPointList& points = downcast<SVGPolylineElement>(element)->animatedPoints()->values();
    if (points.isEmpty())
        return;

    path.moveTo(points.first());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i));
}

static void updatePathFromRectElement(SVGElement* element, Path& path)
{
    RenderElement* renderer = element->renderer();
    if (!renderer)
        return;

    auto& style = renderer->style();
    SVGLengthContext lengthContext(element);
    float width = lengthContext.valueForLength(style.width(), LengthModeWidth);
    if (width <= 0)
        return;
    float height = lengthContext.valueForLength(style.height(), LengthModeHeight);
    if (height <= 0)
        return;
    float x = lengthContext.valueForLength(style.svgStyle().x(), LengthModeWidth);
    float y = lengthContext.valueForLength(style.svgStyle().y(), LengthModeHeight);
    float rx = lengthContext.valueForLength(style.svgStyle().rx(), LengthModeWidth);
    float ry = lengthContext.valueForLength(style.svgStyle().ry(), LengthModeHeight);
    bool hasRx = rx > 0;
    bool hasRy = ry > 0;
    if (hasRx || hasRy) {
        if (!hasRx)
            rx = ry;
        else if (!hasRy)
            ry = rx;
        // FIXME: We currently enforce using beziers here, as at least on CoreGraphics/Lion, as
        // the native method uses a different line dash origin, causing svg/custom/dashOrigin.svg to fail.
        // See bug https://bugs.webkit.org/show_bug.cgi?id=79932 which tracks this issue.
        path.addRoundedRect(FloatRect(x, y, width, height), FloatSize(rx, ry), Path::PreferBezierRoundedRect);
        return;
    }

    path.addRect(FloatRect(x, y, width, height));
}

void updatePathFromGraphicsElement(SVGElement* element, Path& path)
{
    ASSERT(element);
    ASSERT(path.isEmpty());

    typedef void (*PathUpdateFunction)(SVGElement*, Path&);
    static HashMap<AtomicStringImpl*, PathUpdateFunction>* map = 0;
    if (!map) {
        map = new HashMap<AtomicStringImpl*, PathUpdateFunction>;
        map->set(SVGNames::circleTag.localName().impl(), &updatePathFromCircleElement);
        map->set(SVGNames::ellipseTag.localName().impl(), &updatePathFromEllipseElement);
        map->set(SVGNames::lineTag.localName().impl(), &updatePathFromLineElement);
        map->set(SVGNames::pathTag.localName().impl(), &updatePathFromPathElement);
        map->set(SVGNames::polygonTag.localName().impl(), &updatePathFromPolygonElement);
        map->set(SVGNames::polylineTag.localName().impl(), &updatePathFromPolylineElement);
        map->set(SVGNames::rectTag.localName().impl(), &updatePathFromRectElement);
    }

    if (PathUpdateFunction pathUpdateFunction = map->get(element->localName().impl()))
        (*pathUpdateFunction)(element, path);
}

} // namespace WebCore
