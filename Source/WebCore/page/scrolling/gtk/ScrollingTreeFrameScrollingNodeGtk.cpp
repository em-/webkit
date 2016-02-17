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
#include "ScrollingTreeFrameScrollingNodeGtk.h"

#if ENABLE(ASYNC_SCROLLING)

#include "FrameView.h"
#include "LayoutSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PlatformWheelEvent.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include "Settings.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/StringBuilder.h>

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsLayer.h"
#endif

namespace WebCore {

static void logThreadedScrollingMode(unsigned synchronousScrollingReasons);

Ref<ScrollingTreeFrameScrollingNode> ScrollingTreeFrameScrollingNodeGtk::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeFrameScrollingNodeGtk(scrollingTree, nodeID));
}

ScrollingTreeFrameScrollingNodeGtk::ScrollingTreeFrameScrollingNodeGtk(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeFrameScrollingNode(scrollingTree, nodeID)
    , m_scrollController(*this)
    , m_scrollLayer(0)
    , m_scrolledContentsLayer(0)
    , m_counterScrollingLayer(0)
    , m_insetClipLayer(0)
    , m_contentShadowLayer(0)
    , m_headerLayer(0)
    , m_footerLayer(0)
    , m_compositingCoordinator(nullptr)
    , m_verticalScrollbar(nullptr)
    , m_horizontalScrollbar(nullptr)
{
}

ScrollingTreeFrameScrollingNodeGtk::~ScrollingTreeFrameScrollingNodeGtk()
{
}

void ScrollingTreeFrameScrollingNodeGtk::setLayerPosition(GraphicsLayer::PlatformLayerID id, const FloatPoint& p)
{
#if USE(COORDINATED_GRAPHICS)
    ASSERT(m_compositingCoordinator);
    m_compositingCoordinator->commitLayerPosition(id, p);
#else
    UNUSED_PARAM(id);
    UNUSED_PARAM(p);
#endif
}

void ScrollingTreeFrameScrollingNodeGtk::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateFrameScrollingNode>(stateNode);

#if USE(COORDINATED_GRAPHICS)
    CoordinatedGraphicsLayerClient* compositingCoordinator = stateNode.compositingCoordinator();
    if (compositingCoordinator && compositingCoordinator != m_compositingCoordinator)
        m_compositingCoordinator = compositingCoordinator;
#endif

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer)) {
        m_scrollLayer = scrollingStateNode.layer();
#if USE(COORDINATED_GRAPHICS)
        m_scrollLayerPosition = scrollingStateNode.layerPosition();
#endif
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::CounterScrollingLayer))
        m_counterScrollingLayer = scrollingStateNode.counterScrollingLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::InsetClipLayer))
        m_insetClipLayer = scrollingStateNode.insetClipLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ContentShadowLayer))
        m_contentShadowLayer = scrollingStateNode.contentShadowLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderLayer))
        m_headerLayer = scrollingStateNode.headerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterLayer))
        m_footerLayer = scrollingStateNode.footerLayer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::PainterForScrollbar)) {
        m_verticalScrollbar = scrollingStateNode.verticalScrollbar();
        m_horizontalScrollbar = scrollingStateNode.horizontalScrollbar();
    }

    bool logScrollingMode = !m_hadFirstUpdate;
    if (scrollingStateNode.hasChangedProperty(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling)) {
        if (shouldUpdateScrollLayerPositionSynchronously()) {
            // We're transitioning to the slow "update scroll layer position on the main thread" mode.
            // Initialize the probable main thread scroll position with the current scroll layer position.
            if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
                m_probableMainThreadScrollPosition = scrollingStateNode.requestedScrollPosition();
            else {
                const FloatPoint& scrollLayerPosition = m_scrollLayerPosition;
                m_probableMainThreadScrollPosition = FloatPoint(-scrollLayerPosition.x(), -scrollLayerPosition.y());
            }
        }

        logScrollingMode = true;
    }

    if (logScrollingMode) {
        if (scrollingTree().scrollingPerformanceLoggingEnabled())
            logThreadedScrollingMode(synchronousScrollingReasons());
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ExpectsWheelEventTestTrigger))
        m_expectsWheelEventTestTrigger = scrollingStateNode.expectsWheelEventTestTrigger();

    m_hadFirstUpdate = true;
}

void ScrollingTreeFrameScrollingNodeGtk::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeFrameScrollingNode::updateAfterChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateScrollingNode>(stateNode);

    // Update the scroll position after child nodes have been updated, because they need to have updated their constraints before any scrolling happens.
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        setScrollPosition(scrollingStateNode.requestedScrollPosition());

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        updateMainFramePinState(scrollPosition());
}

void ScrollingTreeFrameScrollingNodeGtk::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return;

    m_scrollController.handleWheelEvent(wheelEvent);
}

bool ScrollingTreeFrameScrollingNodeGtk::isAlreadyPinnedInDirectionOfGesture(const PlatformWheelEvent& wheelEvent, ScrollEventAxis axis)
{
    switch (axis) {
    case ScrollEventAxis::Vertical:
        return (wheelEvent.deltaY() > 0 && scrollPosition().y() <= minimumScrollPosition().y()) || (wheelEvent.deltaY() < 0 && scrollPosition().y() >= maximumScrollPosition().y());
    case ScrollEventAxis::Horizontal:
        return (wheelEvent.deltaX() > 0 && scrollPosition().x() <= minimumScrollPosition().x()) || (wheelEvent.deltaX() < 0 && scrollPosition().x() >= maximumScrollPosition().x());
    }

    ASSERT_NOT_REACHED();
    return false;
}

void ScrollingTreeFrameScrollingNodeGtk::scrollBy(const FloatSize& offset)
{
    ScrollingTreeFrameScrollingNode::scrollBy(offset);
}

FloatPoint ScrollingTreeFrameScrollingNodeGtk::scrollPosition() const
{
    if (shouldUpdateScrollLayerPositionSynchronously())
        return m_probableMainThreadScrollPosition;

    return -m_scrollLayerPosition;
}

void ScrollingTreeFrameScrollingNodeGtk::setScrollPosition(const FloatPoint& scrollPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNodeMac::setScrollPosition " << scrollPosition << " scrollPosition(): " << this->scrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition());

    // Scroll deltas can be non-integral with some input devices, so scrollPosition may not be integral.
    // FIXME: when we support half-pixel scroll positions on Retina displays, this will need to round to half pixels.
    FloatPoint roundedPosition(roundf(scrollPosition.x()), roundf(scrollPosition.y()));

    ScrollingTreeFrameScrollingNode::setScrollPosition(roundedPosition);

    if (scrollingTree().scrollingPerformanceLoggingEnabled())
        logExposedUnfilledArea();
}

void ScrollingTreeFrameScrollingNodeGtk::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    updateMainFramePinState(scrollPosition);

    if (shouldUpdateScrollLayerPositionSynchronously()) {
        m_probableMainThreadScrollPosition = scrollPosition;
        scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, SetScrollingLayerPosition);
        return;
    }

    setScrollLayerPosition(scrollPosition);
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition);
}

void ScrollingTreeFrameScrollingNodeGtk::setScrollLayerPosition(const FloatPoint& position)
{
    ASSERT(!shouldUpdateScrollLayerPositionSynchronously());

    m_scrollLayerPosition = -position;
    setLayerPosition(m_scrollLayer, m_scrollLayerPosition);

    ScrollBehaviorForFixedElements behaviorForFixed = scrollBehaviorForFixedElements();
    FloatRect viewportRect(position, scrollableAreaSize());

    FloatPoint scrollPositionForFixedChildren = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(position), scrollOrigin(), frameScaleFactor(),
        fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight());

    if (m_counterScrollingLayer)
        setLayerPosition(m_counterScrollingLayer, FloatPoint(scrollPositionForFixedChildren));

    float topContentInset = this->topContentInset();
    if (m_insetClipLayer && m_scrolledContentsLayer && topContentInset) {
        setLayerPosition(m_insetClipLayer, FloatPoint(0, FrameView::yPositionForInsetClipLayer(position, topContentInset)));

        const FloatPoint scrolledContentsLayerPosition = FrameView::positionForRootContentLayer(position, scrollOrigin(), topContentInset, headerHeight());
        setLayerPosition(m_scrolledContentsLayer, scrolledContentsLayerPosition);
        if (m_contentShadowLayer)
            setLayerPosition(m_contentShadowLayer, scrolledContentsLayerPosition);
    }

    if (m_headerLayer || m_footerLayer) {
        // Generally the banners should have the same horizontal-position computation as a fixed element. However,
        // the banners are not affected by the frameScaleFactor(), so if there is currently a non-1 frameScaleFactor()
        // then we should recompute scrollPositionForFixedChildren for the banner with a scale factor of 1.
        float horizontalScrollOffsetForBanner = scrollPositionForFixedChildren.x();
        if (frameScaleFactor() != 1)
            horizontalScrollOffsetForBanner = FrameView::scrollPositionForFixedPosition(enclosingLayoutRect(viewportRect), LayoutSize(totalContentsSize()), LayoutPoint(position), scrollOrigin(), 1, fixedElementsLayoutRelativeToFrame(), behaviorForFixed, headerHeight(), footerHeight()).x();

        if (m_headerLayer)
            setLayerPosition(m_headerLayer, FloatPoint(horizontalScrollOffsetForBanner, FrameView::yPositionForHeaderLayer(position, topContentInset)));

        if (m_footerLayer) {
            setLayerPosition(m_footerLayer, FloatPoint(horizontalScrollOffsetForBanner,
                FrameView::yPositionForFooterLayer(position, topContentInset, totalContentsSize().height(), footerHeight())));
        }
    }

    if (!m_children)
        return;

    viewportRect.setLocation(scrollPositionForFixedChildren);

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, viewportRect, FloatSize());
}

void ScrollingTreeFrameScrollingNodeGtk::updateLayersAfterViewportChange(const FloatRect&, double)
{
    ASSERT_NOT_REACHED();
}

FloatPoint ScrollingTreeFrameScrollingNodeGtk::minimumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(), toFloatSize(scrollOrigin()));
    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToBottom)
        position.setY(maximumScrollPosition().y());

    return position;
}

FloatPoint ScrollingTreeFrameScrollingNodeGtk::maximumScrollPosition() const
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(FloatPoint(totalContentsSizeForRubberBand() - scrollableAreaSize()), toFloatSize(scrollOrigin()));
    position = position.expandedTo(FloatPoint());

    if (scrollingTree().rootNode() == this && scrollingTree().scrollPinningBehavior() == PinToTop)
        position.setY(minimumScrollPosition().y());

    return position;
}

void ScrollingTreeFrameScrollingNodeGtk::updateMainFramePinState(const FloatPoint& scrollPosition)
{
    bool pinnedToTheLeft = scrollPosition.x() <= minimumScrollPosition().x();
    bool pinnedToTheRight = scrollPosition.x() >= maximumScrollPosition().x();
    bool pinnedToTheTop = scrollPosition.y() <= minimumScrollPosition().y();
    bool pinnedToTheBottom = scrollPosition.y() >= maximumScrollPosition().y();

    scrollingTree().setMainFramePinState(pinnedToTheLeft, pinnedToTheRight, pinnedToTheTop, pinnedToTheBottom);
}

void ScrollingTreeFrameScrollingNodeGtk::logExposedUnfilledArea()
{
    notImplemented();
}

static void logThreadedScrollingMode(unsigned synchronousScrollingReasons)
{
    if (synchronousScrollingReasons) {
        StringBuilder reasonsDescription;

        if (synchronousScrollingReasons & ScrollingCoordinator::ForcedOnMainThread)
            reasonsDescription.appendLiteral("forced,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasSlowRepaintObjects)
            reasonsDescription.appendLiteral("slow-repaint objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
            reasonsDescription.appendLiteral("viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
            reasonsDescription.appendLiteral("non-layer viewport-constrained objects,");
        if (synchronousScrollingReasons & ScrollingCoordinator::IsImageDocument)
            reasonsDescription.appendLiteral("image document,");

        // Strip the trailing comma.
        String reasonsDescriptionTrimmed = reasonsDescription.toString().left(reasonsDescription.length() - 1);

        WTFLogAlways("SCROLLING: Switching to main-thread scrolling mode. Time: %f Reason(s): %s\n", WTF::monotonicallyIncreasingTime(), reasonsDescriptionTrimmed.ascii().data());
    } else
        WTFLogAlways("SCROLLING: Switching to threaded scrolling mode. Time: %f\n", WTF::monotonicallyIncreasingTime());
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
