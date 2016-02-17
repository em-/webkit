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

#ifndef ScrollingTreeFrameScrollingNodeGtk_h
#define ScrollingTreeFrameScrollingNodeGtk_h

#if ENABLE(ASYNC_SCROLLING)

#include "GraphicsLayer.h"
#include "ScrollController.h"
#include "ScrollbarThemeGtk.h"
#include "ScrollingTreeFrameScrollingNode.h"

namespace WebCore {

class CoordinatedGraphicsLayerClient;

class ScrollingTreeFrameScrollingNodeGtk : public ScrollingTreeFrameScrollingNode, private ScrollControllerClient {
public:
    WEBCORE_EXPORT static Ref<ScrollingTreeFrameScrollingNode> create(ScrollingTree&, ScrollingNodeID);
    virtual ~ScrollingTreeFrameScrollingNodeGtk();

private:
    ScrollingTreeFrameScrollingNodeGtk(ScrollingTree&, ScrollingNodeID);

    // ScrollingTreeNode member functions.
    virtual void updateBeforeChildren(const ScrollingStateNode&) override;
    virtual void updateAfterChildren(const ScrollingStateNode&) override;
    virtual void handleWheelEvent(const PlatformWheelEvent&) override;

    // ScrollcontrollerClient member functions.
    virtual void scrollBy(const FloatSize&) override;
    virtual Scrollbar* verticalScrollbar() override { return m_verticalScrollbar; }
    virtual Scrollbar* horizontalScrollbar() override { return m_horizontalScrollbar; }

    FloatPoint scrollPosition() const override;
    void setScrollPosition(const FloatPoint&) override;
    void setScrollPositionWithoutContentEdgeConstraints(const FloatPoint&) override;

    void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) override;

    void setScrollLayerPosition(const FloatPoint&) override;

    FloatPoint minimumScrollPosition() const override;
    FloatPoint maximumScrollPosition() const override;

    void updateMainFramePinState(const FloatPoint& scrollPosition);

    bool isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent&, ScrollEventAxis);

    void logExposedUnfilledArea();

    void setLayerPosition(GraphicsLayer::PlatformLayerID, const FloatPoint&);

    ScrollController m_scrollController;

    GraphicsLayer::PlatformLayerID m_scrollLayer;
    GraphicsLayer::PlatformLayerID m_scrolledContentsLayer;
    GraphicsLayer::PlatformLayerID m_counterScrollingLayer;
    GraphicsLayer::PlatformLayerID m_insetClipLayer;
    GraphicsLayer::PlatformLayerID m_contentShadowLayer;
    GraphicsLayer::PlatformLayerID m_headerLayer;
    GraphicsLayer::PlatformLayerID m_footerLayer;

    FloatPoint m_scrollLayerPosition;

    CoordinatedGraphicsLayerClient* m_compositingCoordinator;

    Scrollbar* m_verticalScrollbar;
    Scrollbar* m_horizontalScrollbar;

    FloatPoint m_probableMainThreadScrollPosition;
    bool m_lastScrollHadUnfilledPixels { false };
    bool m_hadFirstUpdate { false };
    bool m_expectsWheelEventTestTrigger { false };
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTreeFrameScrollingNodeGtk_h
