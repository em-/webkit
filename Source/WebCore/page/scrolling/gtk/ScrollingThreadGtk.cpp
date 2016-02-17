/*
 * Copyright (C) 2012 Collabora Ltd. All rights reserved.
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
#include "ScrollingThread.h"

#if ENABLE(ASYNC_SCROLLING)

#include <wtf/RunLoop.h>

namespace WebCore {

class ScrollingRunLoop {
    WTF_MAKE_NONCOPYABLE(ScrollingRunLoop);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScrollingRunLoop()
        : m_runLoop(RunLoop::current())
    {
    }

    void callOnScrollingRunLoop(std::function<void()> function)
    {
        if (&m_runLoop == &RunLoop::current()) {
            function();
            return;
        }

        m_runLoop.dispatch(WTFMove(function));
    }

    RunLoop& runLoop() { return m_runLoop; }
private:
    RunLoop& m_runLoop;
};

ScrollingThread::ScrollingThread()
    : m_threadIdentifier(0)
    , m_threadRunLoop(nullptr)
{
}

ScrollingThread::~ScrollingThread()
{
    terminateScrollingThread();
}

void ScrollingThread::initializeRunLoop()
{
    {
        LockHolder locker(m_initializeRunLoopMutex);
        m_threadRunLoop = std::make_unique<ScrollingRunLoop>();

        m_initializeRunLoopConditionVariable.notifyAll();
    }

    ASSERT(isCurrentThread());
    m_threadRunLoop->runLoop().run();

    {
        LockHolder locker(m_terminateRunLoopMutex);
        m_threadRunLoop = nullptr;
        m_terminateRunLoopCondition.notifyOne();
    }

    detachThread(m_threadIdentifier);
}

void ScrollingThread::terminateScrollingThread()
{
    LockHolder locker(m_terminateRunLoopMutex);
    m_threadRunLoop->runLoop().stop();
    m_terminateRunLoopCondition.wait(m_terminateRunLoopMutex);
}

void ScrollingThread::dispatch(std::function<void ()> function)
{
    auto& scrollingThread = ScrollingThread::singleton();
    scrollingThread.createThreadIfNeeded();

    {
        LockHolder locker(singleton().m_functionsMutex);
        scrollingThread.m_threadRunLoop->callOnScrollingRunLoop(function);
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
