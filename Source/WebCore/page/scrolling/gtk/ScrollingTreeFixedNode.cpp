/*
 * Copyright (C) 2015 Collabora Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingTreeFixedNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateFixedNode.h"
#include "ScrollingTree.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsLayer.h"
#endif

namespace WebCore {

Ref<ScrollingTreeFixedNode> ScrollingTreeFixedNode::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFixedNode(scrollingTree, nodeID));
}

ScrollingTreeFixedNode::ScrollingTreeFixedNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, FixedNode, nodeID)
    , m_compositingCoordinator(nullptr)
{
    scrollingTree.fixedOrStickyNodeAdded();
}

ScrollingTreeFixedNode::~ScrollingTreeFixedNode()
{
    scrollingTree().fixedOrStickyNodeRemoved();
}

void ScrollingTreeFixedNode::setLayerPosition(GraphicsLayer::PlatformLayerID id, const FloatPoint& p)
{
#if USE(COORDINATED_GRAPHICS)
    ASSERT(m_compositingCoordinator);
    m_compositingCoordinator->commitLayerPosition(id, p);
#else
    UNUSED_PARAM(id);
    UNUSED_PARAM(p);
#endif
}

void ScrollingTreeFixedNode::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateFixedNode& fixedStateNode = downcast<ScrollingStateFixedNode>(stateNode);

#if USE(COORDINATED_GRAPHICS)
    CoordinatedGraphicsLayerClient* compositingCoordinator = fixedStateNode.compositingCoordinator();
    if (compositingCoordinator && compositingCoordinator != m_compositingCoordinator)
        m_compositingCoordinator = compositingCoordinator;
#endif

    if (fixedStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_layer = fixedStateNode.layer();

    if (stateNode.hasChangedProperty(ScrollingStateFixedNode::ViewportConstraints))
        m_constraints = fixedStateNode.viewportConstraints();
}

void ScrollingTreeFixedNode::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    FloatPoint layerPosition = m_constraints.layerPositionForViewportRect(fixedPositionRect);
    layerPosition -= cumulativeDelta;

    const FloatPoint newPosition = layerPosition - m_constraints.alignmentOffset();
    setLayerPosition(m_layer, newPosition);

    if (!m_children)
        return;

    const FloatSize newDelta = layerPosition - m_constraints.layerPositionAtLastLayout() + cumulativeDelta;
    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, newDelta);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
