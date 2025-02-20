/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#ifndef WebInspectorClient_h
#define WebInspectorClient_h

#include <WebCore/InspectorClient.h>
#include <WebCore/PageOverlay.h>
#include <wtf/HashSet.h>

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;
class IntRect;
class PageOverlay;
}

namespace WebKit {

class WebPage;
class RepaintIndicatorLayerClient;

class WebInspectorClient : public WebCore::InspectorClient, private WebCore::PageOverlay::Client {
friend class RepaintIndicatorLayerClient;
public:
    WebInspectorClient(WebPage*);
    virtual ~WebInspectorClient();

private:
    // WebCore::InspectorClient
    void inspectedPageDestroyed() override;

    Inspector::FrontendChannel* openLocalFrontend(WebCore::InspectorController*) override;
    void bringFrontendToFront() override;
    void didResizeMainFrame(WebCore::Frame*) override;

    void highlight() override;
    void hideHighlight() override;

#if PLATFORM(IOS)
    void showInspectorIndication() override;
    void hideInspectorIndication() override;

    void didSetSearchingForNode(bool) override;
#endif

    void elementSelectionChanged(bool) override;

    bool overridesShowPaintRects() const override { return true; }
    void showPaintRect(const WebCore::FloatRect&) override;

    // PageOverlay::Client
    void pageOverlayDestroyed(WebCore::PageOverlay&) override;
    void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect&) override;
    bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;

    void animationEndedForLayer(const WebCore::GraphicsLayer*);

    WebPage* m_page;
    WebCore::PageOverlay* m_highlightOverlay;
    
    RefPtr<WebCore::PageOverlay> m_paintRectOverlay;
    std::unique_ptr<RepaintIndicatorLayerClient> m_paintIndicatorLayerClient;
    HashSet<WebCore::GraphicsLayer*> m_paintRectLayers; // Ideally this would be HashSet<std::unique_ptr<GraphicsLayer>> but that doesn't work yet. webkit.org/b/136166
};

} // namespace WebKit

#endif // WebInspectorClient_h
