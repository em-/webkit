/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#ifndef HTMLMeterElement_h
#define HTMLMeterElement_h

#if ENABLE(METER_ELEMENT)
#include "LabelableElement.h"

namespace WebCore {

class MeterValueElement;
class RenderMeter;

class HTMLMeterElement final : public LabelableElement {
public:
    static Ref<HTMLMeterElement> create(const QualifiedName&, Document&);

    enum GaugeRegion {
        GaugeRegionOptimum,
        GaugeRegionSuboptimal,
        GaugeRegionEvenLessGood
    };

    double min() const;
    void setMin(double, ExceptionCode&);

    double max() const;
    void setMax(double, ExceptionCode&);

    double value() const;
    void setValue(double, ExceptionCode&);

    double low() const;
    void setLow(double, ExceptionCode&);

    double high() const;
    void setHigh(double, ExceptionCode&);

    double optimum() const;
    void setOptimum(double, ExceptionCode&);

    double valueRatio() const;
    GaugeRegion gaugeRegion() const;

    bool canContainRangeEndPoint() const override { return false; }

private:
    HTMLMeterElement(const QualifiedName&, Document&);
    virtual ~HTMLMeterElement();

    RenderMeter* renderMeter() const;

    bool supportLabels() const override { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;

    void didElementStateChange();
    void didAddUserAgentShadowRoot(ShadowRoot*) override;
    bool canHaveUserAgentShadowRoot() const final { return true; }

    RefPtr<MeterValueElement> m_value;
};

} // namespace

#endif
#endif
