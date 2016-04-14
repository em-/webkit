/*
 * Copyright (C) 2013 Igalia S.L.
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

#include "config.h"
#include "WaylandCompositor.h"

#if USE(NESTED_COMPOSITOR)

#include <gdk/gdkwayland.h>
#include <WebCore/EGLHelper.h>
#include <WebCore/GLContextEGL.h>
#include <WebCore/GLPlatformContext.h>
#include <WebCore/PlatformDisplayWayland.h>
#include <WebCore/WaylandEventSource.h>
#include <WebCore/WebKitGtkWaylandServerProtocol.h>

using namespace WebCore;

namespace WebKit {

struct NestedDisplay {
    struct wl_display* childDisplay;      // Nested display
    std::unique_ptr<GLContextEGL> eglCtx; // EGL context
    struct wl_global* wlGlobal;           // Wayland display global
    struct wl_global* webkitgtkGlobal;    // Wayland webkitgtk interface global
    GSource* eventSource;                 // Display event source
};

struct NestedBuffer {
    struct wl_resource* resource;
    struct wl_signal destroySignal;
    struct wl_listener destroyListener;
    uint32_t busyCount;
};

struct NestedBufferReference {
    struct NestedBuffer* buffer;
    struct wl_listener destroyListener;
};

struct NestedSurface {
    WaylandCompositor* compositor;            // Nested compositor instance
    struct NestedBuffer* buffer;              // Last attached buffer (pending buffer)
    GLuint texture;                           // GL texture for the surface
    EGLImageKHR image;                        // EGL Image for texture
    RefPtr<cairo_surface_t> cairoSurface;     // Cairo surface from GL texture
    struct wl_list frameCallbackList;         // Pending frame callback list
    GtkWidget* widget;                        // widget associated with this surface
    struct wl_listener bufferDestroyListener; // Pending buffer destroy listener
    struct NestedBufferReference bufferRef;   // Current buffer
    struct wl_list link;
};

// List of pending frame callbacks on a nested surface
struct NestedFrameCallback {
    struct wl_resource* resource;
    struct wl_list link;
};

static RefPtr<cairo_surface_t> getImageSurfaceFromTexture(GLuint texture, int x, int y, int width, int height)
{
    RefPtr<cairo_surface_t> newSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    unsigned char* data = cairo_image_surface_get_data(newSurface.get());

    GLuint fb;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);

    // Convert to BGRA.
    int totalBytes = width * height * 4;
    for (int i = 0; i < totalBytes; i += 4)
        std::swap(data[i], data[i + 2]);

    cairo_surface_mark_dirty(newSurface.get());
    return newSurface;
}

void WaylandCompositor::nestedBufferReferenceHandleDestroy(struct wl_listener* listener, void* data)
{
    struct NestedBufferReference* ref = 0;
    ref = wl_container_of(listener, ref, destroyListener);
    ref->buffer = 0;
}

void WaylandCompositor::nestedBufferReference(struct NestedBufferReference* ref, struct NestedBuffer* buffer)
{
    if (buffer == ref->buffer)
        return;

    if (ref->buffer) {
        ref->buffer->busyCount--;
        if (ref->buffer->busyCount == 0)
            wl_resource_queue_event(ref->buffer->resource, WL_BUFFER_RELEASE);
        wl_list_remove(&ref->destroyListener.link);
    }

    if (buffer) {
        buffer->busyCount++;
        wl_signal_add(&buffer->destroySignal, &ref->destroyListener);
        ref->destroyListener.notify = nestedBufferReferenceHandleDestroy;
    }

    ref->buffer = buffer;
}

void WaylandCompositor::surfaceHandlePendingBufferDestroy(struct wl_listener* listener, void* data)
{
    struct NestedSurface* surface = 0;
    surface = wl_container_of(listener, surface, bufferDestroyListener);
    surface->buffer = 0;
}

void WaylandCompositor::nestedBufferDestroyHandler(struct wl_listener* listener, void* data)
{
    struct NestedBuffer* buffer = 0;
    buffer = wl_container_of(listener, buffer, destroyListener);
    wl_signal_emit(&buffer->destroySignal, buffer);
    g_free(buffer);
}

struct NestedBuffer* WaylandCompositor::nestedBufferFromResource(struct wl_resource* resource)
{
    struct NestedBuffer* buffer;
    struct wl_listener* listener;

    listener = wl_resource_get_destroy_listener(resource, nestedBufferDestroyHandler);
    if (listener)
        return wl_container_of(listener, (struct NestedBuffer*)0, destroyListener);

    buffer = g_new0(struct NestedBuffer, 1);
    buffer->resource = resource;
    wl_signal_init(&buffer->destroySignal);
    buffer->destroyListener.notify = nestedBufferDestroyHandler;
    wl_resource_add_destroy_listener(resource, &buffer->destroyListener);

    return buffer;
}

bool WaylandCompositor::initEGL(struct NestedDisplay* d)
{
    auto& upstreamDisplay = PlatformDisplay::sharedDisplay();
    d->eglCtx = downcast<PlatformDisplayWayland>(upstreamDisplay).createSharingGLContext();
    d->eglCtx->makeContextCurrent();

    EGLHelper::resolveEGLBindings();

    return true;
}

struct NestedDisplay* WaylandCompositor::createNestedDisplay()
{
    struct NestedDisplay* display = g_new0(struct NestedDisplay, 1);

    if (!initEGL(display)) {
        destroyNestedDisplay(display);
        return nullptr;
    }

    return display;
}

void WaylandCompositor::destroyNestedDisplay(struct NestedDisplay* display)
{
    if (display->childDisplay)
        EGLHelper::unbindWaylandDisplay(display->childDisplay);
    if (display->eventSource)
        g_source_remove(g_source_get_id(display->eventSource));
    if (display->wlGlobal)
        wl_global_destroy(display->wlGlobal);
    if (display->webkitgtkGlobal)
        wl_global_destroy(display->webkitgtkGlobal);
    if (display->childDisplay)
        wl_display_destroy(display->childDisplay);
    display->eglCtx.reset();
    g_free(display);
}

void WaylandCompositor::destroyNestedFrameCallback(struct wl_resource* resource)
{
    struct NestedFrameCallback* callback = static_cast<NestedFrameCallback*>(wl_resource_get_user_data(resource));
    wl_list_remove(&callback->link);
    g_free(callback);
}

void WaylandCompositor::surfaceDestroy(struct wl_client* client, struct wl_resource* resource)
{
    wl_resource_destroy(resource);
}

void WaylandCompositor::surfaceAttach(struct wl_client* client, struct wl_resource* resource, struct wl_resource* bufferResource, int32_t sx, int32_t sy)
{
    struct NestedSurface* surface = static_cast<struct NestedSurface*>(wl_resource_get_user_data(resource));

    EGLint format;
    if (!EGLHelper::queryWaylandBuffer(bufferResource, EGL_TEXTURE_FORMAT, &format))
        return;
    if (format != EGL_TEXTURE_RGB && format != EGL_TEXTURE_RGBA)
        return;

    // Remove references to any previous pending buffer for this surface
    if (surface->buffer) {
        surface->buffer = 0;
        wl_list_remove(&surface->bufferDestroyListener.link);
    }

    // Make the new buffer the current pending buffer
    if (bufferResource) {
        surface->buffer = nestedBufferFromResource(bufferResource);
        wl_signal_add(&surface->buffer->destroySignal, &surface->bufferDestroyListener);
    }
}

void WaylandCompositor::surfaceDamage(struct wl_client* client, struct wl_resource* resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
    // FIXME: Try to use damage information to improve painting
}

void WaylandCompositor::surfaceDamageBuffer(struct wl_client* client, struct wl_resource* resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
}

void WaylandCompositor::surfaceFrame(struct wl_client* client, struct wl_resource* resource, uint32_t id)
{
    struct NestedSurface* surface = static_cast<struct NestedSurface*>(wl_resource_get_user_data(resource));
    if (!surface)
        return;

    struct NestedFrameCallback* callback = g_new0(struct NestedFrameCallback, 1);
    callback->resource = wl_resource_create(client, &wl_callback_interface, 1, id);
    wl_resource_set_implementation(callback->resource, 0, callback, destroyNestedFrameCallback);
    wl_list_insert(surface->frameCallbackList.prev, &callback->link);
}

void WaylandCompositor::surfaceSetOpaqueRegion(struct wl_client* client, struct wl_resource* resource, struct wl_resource* regionResource)
{

}

void WaylandCompositor::surfaceSetInputRegion(struct wl_client* client, struct wl_resource* resource, struct wl_resource* regionResource)
{

}

void WaylandCompositor::surfaceCommit(struct wl_client* client, struct wl_resource* resource)
{
    struct NestedSurface* surface = static_cast<struct NestedSurface*>(wl_resource_get_user_data(resource));
    if (!surface)
        return;

    WaylandCompositor* compositor = surface->compositor;

    compositor->m_display->eglCtx->makeContextCurrent();

    // Destroy any existing EGLImage for this surface
    if (surface->image != EGL_NO_IMAGE_KHR)
        EGLHelper::destroyEGLImage(surface->image);

    // Make the pending buffer current
    nestedBufferReference(&surface->bufferRef, surface->buffer);

    // Create a new EGLImage from the last buffer attached to this surface
    EGLHelper::createEGLImage(&surface->image, EGL_WAYLAND_BUFFER_WL, surface->buffer->resource, 0);
    if (surface->image == EGL_NO_IMAGE_KHR)
        return;

    // Bind the surface texture to the new EGLImage
    glBindTexture(GL_TEXTURE_2D, surface->texture);
    EGLHelper::imageTargetTexture2DOES(surface->image);

    // Create a new cairo surface associated with the surface texture
    int width, height;
    EGLHelper::queryWaylandBuffer(surface->buffer->resource, EGL_WIDTH, &width);
    EGLHelper::queryWaylandBuffer(surface->buffer->resource, EGL_HEIGHT, &height);
    surface->cairoSurface = getImageSurfaceFromTexture(surface->texture, 0, 0, width, height);

    // We are done with this buffer
    if (surface->buffer) {
        wl_list_remove(&surface->bufferDestroyListener.link);
        surface->buffer = 0;
    }

    // Redraw the widget backed by this surface
    if (surface->widget)
        gtk_widget_queue_draw(surface->widget);

    // Process frame callbacks for this surface so the client can render a new frame
    struct NestedFrameCallback *nc, *next;
    wl_list_for_each_safe(nc, next, &surface->frameCallbackList, link) {
        wl_callback_send_done(nc->resource, 0);
        wl_resource_destroy(nc->resource);
    }
    wl_list_init(&surface->frameCallbackList);
    wl_display_flush_clients(surface->compositor->m_display->childDisplay);
}

void WaylandCompositor::surfaceSetBufferTransform(struct wl_client* client, struct wl_resource* resource, int32_t transform)
{
}

void WaylandCompositor::surfaceSetBufferScale(struct wl_client* client, struct wl_resource* resource, int32_t scale)
{
}

static const struct wl_surface_interface surfaceInterface = {
    WaylandCompositor::surfaceDestroy,
    WaylandCompositor::surfaceAttach,
    WaylandCompositor::surfaceDamage,
    WaylandCompositor::surfaceFrame,
    WaylandCompositor::surfaceSetOpaqueRegion,
    WaylandCompositor::surfaceSetInputRegion,
    WaylandCompositor::surfaceCommit,
    WaylandCompositor::surfaceSetBufferTransform,
    WaylandCompositor::surfaceSetBufferScale,
    WaylandCompositor::surfaceDamageBuffer
};

void WaylandCompositor::doDestroyNestedSurface(struct NestedSurface* surface)
{
    // Destroy pending frame callbacks
    struct NestedFrameCallback *cb, *next;
    wl_list_for_each_safe(cb, next, &surface->frameCallbackList, link)
        wl_resource_destroy(cb->resource);
    wl_list_init(&surface->frameCallbackList);

    if (surface->image != EGL_NO_IMAGE_KHR) {
        EGLHelper::destroyEGLImage(surface->image);
        surface->image = EGL_NO_IMAGE_KHR;
    }

    surface->cairoSurface = nullptr;
    nestedBufferReference(&surface->bufferRef, nullptr);
    glDeleteTextures(1, &surface->texture);

    // Unlink this source from the compositor's surface list
    wl_list_remove(&surface->link);

    g_free(surface);
}

void WaylandCompositor::destroyNestedSurface(struct wl_resource* resource)
{
    struct NestedSurface* surface = static_cast<struct NestedSurface*>(wl_resource_get_user_data(resource));
    if (surface)
        doDestroyNestedSurface(surface);
}

void WaylandCompositor::createSurface(struct wl_client* client, struct wl_resource* resource, uint32_t id)
{
    WaylandCompositor* compositor = static_cast<WaylandCompositor*>(wl_resource_get_user_data(resource));

    compositor->m_display->eglCtx->makeContextCurrent();

    struct NestedSurface* surface = g_new0(struct NestedSurface, 1);
    surface->compositor = compositor;

    wl_list_init(&surface->frameCallbackList);
    surface->bufferDestroyListener.notify = surfaceHandlePendingBufferDestroy;

    glGenTextures (1, &surface->texture);
    glBindTexture (GL_TEXTURE_2D, surface->texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    struct wl_resource* surfaceResource = wl_resource_create(client, &wl_surface_interface, 1, id);
    wl_resource_set_implementation(surfaceResource, &surfaceInterface, surface, destroyNestedSurface);

    wl_list_insert(compositor->m_surfaces.prev, &surface->link);
}

void WaylandCompositor::createRegion(struct wl_client* client, struct wl_resource* resource, uint32_t id)
{

}

static const struct wl_compositor_interface compositorInterface = {
    WaylandCompositor::createSurface,
    WaylandCompositor::createRegion
};

void WaylandCompositor::compositorBind(wl_client* client, void* data, uint32_t version, uint32_t id)
{
    WaylandCompositor* compositor = static_cast<WaylandCompositor*>(data);
    struct wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, std::min(static_cast<int>(version), 3), id);
    wl_resource_set_implementation(resource, &compositorInterface, compositor, 0);
}

void WaylandCompositor::webkitgtkSetSurfaceForWidget(struct wl_client* client, struct wl_resource* resource, struct wl_resource* surfaceResource, uint32_t id)
{
    WaylandCompositor* compositor = static_cast<WaylandCompositor*>(wl_resource_get_user_data(resource));
    GtkWidget* widget = getWidgetById(compositor, id);
    if (!widget)
        return;
    struct NestedSurface* surface = static_cast<struct NestedSurface*>(wl_resource_get_user_data(surfaceResource));
    setSurfaceForWidget(compositor, widget, surface);
}

static const struct wl_webkitgtk_interface webkitgtkInterface = {
    WaylandCompositor::webkitgtkSetSurfaceForWidget,
};


void WaylandCompositor::webkitgtkBind(struct wl_client* client, void* data, uint32_t version, uint32_t id)
{
    WaylandCompositor* compositor = static_cast<WaylandCompositor*>(data);;
    struct wl_resource* resource = wl_resource_create(client, &wl_webkitgtk_interface, 1, id);
    wl_resource_set_implementation(resource, &webkitgtkInterface, compositor, 0);
}

bool WaylandCompositor::initialize()
{
    m_display = createNestedDisplay();
    if (!m_display)
        return false;

    m_display->childDisplay = wl_display_create();
    if (!m_display->childDisplay) {
        g_warning("Nested Wayland compositor could not create display object.");
        return false;
    }

    if (wl_display_add_socket(m_display->childDisplay, "webkitgtk-wayland-compositor-socket") != 0) {
        g_warning("Nested Wayland compositor could not create display socket");
        return false;
    }

    if (wl_display_init_shm (m_display->childDisplay) != 0) {
        g_warning("Nested Wayland compositor could not register shm global");
        return false;
    }

    m_display->wlGlobal = wl_global_create(m_display->childDisplay, &wl_compositor_interface, wl_compositor_interface.version, this, compositorBind);
    if (!m_display->wlGlobal) {
        g_warning("Nested Wayland compositor could not register display global");
        return false;
    }

    m_display->webkitgtkGlobal = wl_global_create(m_display->childDisplay, &wl_webkitgtk_interface, 1, this, webkitgtkBind);
    if (!m_display->webkitgtkGlobal) {
        g_warning("Nested Wayland compositor could not register webkitgtk global");
        return false;
    }

    if (!EGLHelper::bindWaylandDisplay(m_display->childDisplay)) {
        g_warning("Nested Wayland compositor could not bind nested display");
        return false;
    }

    // Handle our display events through GLib's main loop
    m_display->eventSource = WaylandEventSource::createDisplayEventSource(m_display->childDisplay);

    return true;
}

WaylandCompositor* WaylandCompositor::m_instance = 0;

WaylandCompositor* WaylandCompositor::instance()
{
    if (m_instance)
        return m_instance;

    WaylandCompositor* compositor = new WaylandCompositor();
    if (!compositor->initialize()) {
        delete compositor;
        return 0;
    }

    m_instance = compositor;
    return m_instance;
}

WaylandCompositor::WaylandCompositor()
    : m_display(0)
{
    wl_list_init(&m_surfaces);
}

WaylandCompositor::~WaylandCompositor()
{
    struct NestedSurface *surface, *next;
    wl_list_for_each_safe(surface, next, &m_surfaces, link)
        doDestroyNestedSurface(surface);
    wl_list_init(&m_surfaces);

    if (m_display)
        destroyNestedDisplay(m_display);
}

void WaylandCompositor::addWidget(GtkWidget* widget)
{
    static int nextWidgetId = 0;
    g_object_set_data(G_OBJECT(widget), "wayland-compositor-widget-id", GINT_TO_POINTER(++nextWidgetId));
    setSurfaceForWidget(this, widget, nullptr);
}

void WaylandCompositor::removeWidget(GtkWidget* widget)
{
    struct NestedSurface* surface = getSurfaceForWidget(this, widget);
    if (surface)
        surface->widget = 0;

    int widgetId = getWidgetId(widget);
    if (widgetId) {
        m_widgetHashMap.remove(widget);
        g_object_steal_data(G_OBJECT(widget), "wayland-compositor-widget-id");
    }
}

int WaylandCompositor::getWidgetId(GtkWidget* widget)
{
    return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "wayland-compositor-widget-id"));
}

struct NestedSurface* WaylandCompositor::getSurfaceForWidget(WaylandCompositor* compositor, GtkWidget* widget)
{
    return compositor->m_widgetHashMap.get(widget);
}

void WaylandCompositor::setSurfaceForWidget(WaylandCompositor* compositor, GtkWidget* widget, struct NestedSurface* surface)
{
    // Associate the new surface with the widget, the client is responsible
    // for destroying any previous surface created for this widget
    compositor->m_widgetHashMap.set(widget, surface);
    if (surface)
        surface->widget = widget;
}

GtkWidget* WaylandCompositor::getWidgetById(WaylandCompositor* compositor, int id)
{
    for (HashMap<GtkWidget*, struct NestedSurface*>::iterator it = compositor->m_widgetHashMap.begin(); it != compositor->m_widgetHashMap.end(); ++it) {
        GtkWidget* widget = it->key;
        if (id == GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "wayland-compositor-widget-id")))
            return widget;
    }
    return 0;
}

cairo_surface_t* WaylandCompositor::cairoSurfaceForWidget(GtkWidget* widget)
{
    struct NestedSurface* surface = getSurfaceForWidget(this, widget);
    return surface ? surface->cairoSurface.get() : 0;
}

EGLImageKHR WaylandCompositor::eglImageForWidget(GtkWidget* widget)
{
    struct NestedSurface* surface = getSurfaceForWidget(this, widget);
    return surface ? surface->image : EGL_NO_IMAGE_KHR;
}

} // namespace WebCore

#endif // USE(NESTED_COMPOSITOR)
