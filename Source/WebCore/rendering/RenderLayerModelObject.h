/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#ifndef RenderLayerModelObject_h
#define RenderLayerModelObject_h

#include "RenderElement.h"

namespace WebCore {

class RenderLayer;

class RenderLayerModelObject : public RenderElement {
public:
    virtual ~RenderLayerModelObject();

    // Called by RenderObject::willBeDestroyed() and is the only way layers should ever be destroyed
    void destroyLayer();

    bool hasSelfPaintingLayer() const;
    RenderLayer* layer() const { return m_layer.get(); }

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void updateFromStyle() { }

    virtual bool requiresLayer() const = 0;

    // Returns true if the background is painted opaque in the given rect.
    // The query rect is given in local coordinate system.
    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const { return false; }

    virtual bool isScrollableOrRubberbandableBox() const { return false; }

protected:
    RenderLayerModelObject(Element&, RenderStyle&&, BaseTypeFlags);
    RenderLayerModelObject(Document&, RenderStyle&&, BaseTypeFlags);

    void createLayer();

private:
    std::unique_ptr<RenderLayer> m_layer;

    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
    static bool s_hadLayer;
    static bool s_hadTransform;
    static bool s_layerWasSelfPainting;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderLayerModelObject, isRenderLayerModelObject())

#endif // RenderLayerModelObject_h
