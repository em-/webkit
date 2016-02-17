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
#include "ScrollingCoordinatorGtk.h"

#if ENABLE(ASYNC_SCROLLING)

#include "FrameView.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "ScrollingStateTree.h"
#include "ScrollingThread.h"
#include "ScrollingTreeGtk.h"
#include "TiledBacking.h"

namespace WebCore {

ScrollingCoordinatorGtk::ScrollingCoordinatorGtk(Page* page)
    : AsyncScrollingCoordinator(page)
    , m_scrollingStateTreeCommitterTimer(*this, &ScrollingCoordinatorGtk::commitTreeState)
{
    setScrollingTree(ScrollingTreeGtk::create(this));
}

ScrollingCoordinatorGtk::~ScrollingCoordinatorGtk()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorGtk::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    m_scrollingStateTreeCommitterTimer.stop();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    RefPtr<ThreadedScrollingTree> scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree] {
        scrollingTree->invalidate();
    });
}

void ScrollingCoordinatorGtk::commitTreeStateIfNeeded()
{
    commitTreeState();
    m_scrollingStateTreeCommitterTimer.stop();
}

bool ScrollingCoordinatorGtk::handleWheelEvent(FrameView&, const PlatformWheelEvent& wheelEvent)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (scrollingTree()->willWheelEventStartSwipeGesture(wheelEvent))
        return false;

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingThread::dispatch([threadedScrollingTree, wheelEvent] {
        threadedScrollingTree->handleWheelEvent(wheelEvent);
    });
    return true;
}

void ScrollingCoordinatorGtk::scheduleTreeStateCommit()
{
    ASSERT(scrollingStateTree()->hasChangedProperties() || nonFastScrollableRegionDirty());
    if (m_scrollingStateTreeCommitterTimer.isActive())
        return;

    m_scrollingStateTreeCommitterTimer.startOneShot(0);
}

void ScrollingCoordinatorGtk::commitTreeState()
{
    willCommitTree();
    if (!scrollingStateTree()->hasChangedProperties())
        return;

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingStateTree* unprotectedTreeState = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerIDRepresentation).release();

    ScrollingThread::dispatch([threadedScrollingTree, unprotectedTreeState] {
        std::unique_ptr<ScrollingStateTree> treeState(unprotectedTreeState);
        threadedScrollingTree->commitNewTreeState(WTFMove(treeState));
    });

    updateTiledScrollingIndicator();
}

void ScrollingCoordinatorGtk::updateTiledScrollingIndicator()
{
    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return;

    TiledBacking* tiledBacking = frameView->tiledBacking();
    if (!tiledBacking)
        return;

    ScrollingModeIndication indicatorMode;
    if (shouldUpdateScrollLayerPositionSynchronously())
        indicatorMode = SynchronousScrollingBecauseOfStyleIndication;
    else
        indicatorMode = AsyncScrollingIndication;

    tiledBacking->setScrollingModeIndication(indicatorMode);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
