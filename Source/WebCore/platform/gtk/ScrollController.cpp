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
#include "ScrollController.h"

#if ENABLE(ASYNC_SCROLLING)

#include "NotImplemented.h"
#include "PlatformWheelEvent.h"
#include "Scrollbar.h"
#include "WheelEventTestTrigger.h"

namespace WebCore {

ScrollController::ScrollController(ScrollControllerClient& client)
    : m_client(client)
{
}

bool ScrollController::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    Scrollbar* horizontalScrollbar = m_client.horizontalScrollbar();
    Scrollbar* verticalScrollbar = m_client.verticalScrollbar();

    float deltaX = horizontalScrollbar ? -wheelEvent.deltaX() : 0;
    float deltaY = verticalScrollbar ? -wheelEvent.deltaY() : 0;

    // Slightly prefer scrolling vertically by applying the = case to deltaY
    if (std::abs(deltaY) >= std::abs(deltaX))
        deltaX = 0;
    else
        deltaY = 0;

    if (deltaX || deltaY) {
        if (deltaY) {
            deltaY *= verticalScrollbar->pixelStep();
            m_client.scrollBy(FloatSize(0, deltaY));
        }
        if (deltaX) {
            deltaX *= horizontalScrollbar->pixelStep();
            m_client.scrollBy(FloatSize(deltaX, 0));
        }
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
