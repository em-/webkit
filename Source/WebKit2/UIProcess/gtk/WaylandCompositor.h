/*
 * Copyright (C) 2013, Igalia S.L.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef  WaylandCompositor_h
#define  WaylandCompositor_h

#include <gtk/gtk.h>

#if USE(NESTED_COMPOSITOR)

#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-egl.h>
#include <opengl/GLDefs.h>
#include <cairo.h>

#include <wtf/HashMap.h>

#include "GLContext.h"
#include "IntSize.h"
#include "RefPtrCairo.h"

namespace WebKit {

struct NestedDisplay;
struct NestedSurface;
struct NestedBuffer;

class WaylandCompositor {
public:
    static WaylandCompositor* instance();
    virtual ~WaylandCompositor();

    // WebKit integration
    void addWidget(GtkWidget*);
    void removeWidget(GtkWidget*);
    int getWidgetId(GtkWidget*);
    cairo_surface_t* cairoSurfaceForWidget(GtkWidget*);
    EGLImageKHR eglImageForWidget(GtkWidget*);

    // Wayland Webkit extension interface
    static void webkitgtkSetSurfaceForWidget(struct wl_client*, struct wl_resource*, struct wl_resource*, uint32_t);

    // Wayland compositor interface
    static void createSurface(struct wl_client*, wl_resource*, uint32_t);
    static void createRegion(struct wl_client*, wl_resource*, uint32_t);

    // Wayland surface interface
    static void surfaceDestroy(struct wl_client*, struct wl_resource*);
    static void surfaceAttach(struct wl_client*, struct wl_resource*, struct wl_resource*, int32_t, int32_t);
    static void surfaceDamage(struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t);
    static void surfaceFrame(struct wl_client*, struct wl_resource*, uint32_t);
    static void surfaceSetOpaqueRegion(struct wl_client*, struct wl_resource*, struct wl_resource*);
    static void surfaceSetInputRegion(struct wl_client*, struct wl_resource*, struct wl_resource*);
    static void surfaceCommit(struct wl_client*, struct wl_resource*);
    static void surfaceSetBufferTransform(struct wl_client*, struct wl_resource*, int32_t);
    static void surfaceSetBufferScale(struct wl_client*, struct wl_resource*, int32_t);
    static void surfaceDamageBuffer(struct wl_client*, struct wl_resource*, int32_t, int32_t, int32_t, int32_t);

private:
    WaylandCompositor();

    // Nested compositor display initialization
    static bool supportsRequiredExtensions(EGLDisplay);
    static bool initEGL(struct NestedDisplay*);
    static void shutdownEGL(struct NestedDisplay*);
    static struct NestedDisplay* createNestedDisplay();
    static void destroyNestedDisplay(struct NestedDisplay*);
    bool initialize();

    // Global binding
    static void webkitgtkBind(struct wl_client*, void*, uint32_t, uint32_t);
    static void compositorBind(struct wl_client*, void*, uint32_t, uint32_t);

    // Widget/Surface mapping
    static struct NestedSurface* getSurfaceForWidget(WaylandCompositor*, GtkWidget*);
    static void setSurfaceForWidget(WaylandCompositor*, GtkWidget*, struct NestedSurface*);
    static GtkWidget* getWidgetById(WaylandCompositor*, int);

    // Surface management
    static void doDestroyNestedSurface(struct NestedSurface*);
    static void destroyNestedSurface(struct wl_resource*);
    static void destroyNestedFrameCallback(struct wl_resource*);

    // Buffer management
    static void nestedBufferDestroyHandler(struct wl_listener*, void*);
    static struct NestedBuffer* nestedBufferFromResource(struct wl_resource*);
    static void surfaceHandlePendingBufferDestroy(struct wl_listener*, void*);
    static void nestedBufferReference(struct NestedBufferReference*, struct NestedBuffer*);
    static void nestedBufferReferenceHandleDestroy(struct wl_listener*, void*);

    // Global instance
    static WaylandCompositor* m_instance;

    struct NestedDisplay* m_display;
    HashMap<GtkWidget*, struct NestedSurface*> m_widgetHashMap;
    struct wl_list m_surfaces;
};

} // namespace WebCore

#endif // USE(NESTED_COMPOSITOR)

#endif // WaylandCompositor_h
