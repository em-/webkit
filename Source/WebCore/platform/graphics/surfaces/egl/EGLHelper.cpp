/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#include "EGLHelper.h"

#if USE(EGL)

#include "PlatformDisplay.h"
#include <opengl/GLPlatformContext.h>


namespace WebCore {

#if PLATFORM(WAYLAND) && !defined(EGL_WL_bind_wayland_display)
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
#endif

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 0;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 0;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC eglImageTargetTexture2DOES = 0;

#if PLATFORM(WAYLAND)
static PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplayWL = 0;
static PFNEGLUNBINDWAYLANDDISPLAYWL eglUnbindWaylandDisplayWL = 0;
static PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL = 0;
#endif

EGLDisplay EGLHelper::eglDisplay()
{
    return PlatformDisplay::sharedDisplay().eglDisplay();
}

EGLDisplay EGLHelper::currentDisplay()
{
    EGLDisplay display = eglGetCurrentDisplay();

    if (display == EGL_NO_DISPLAY)
        display = EGLHelper::eglDisplay();

    return display;
}

void EGLHelper::resolveEGLBindings()
{
    static bool initialized = false;

    if (initialized)
        return;

    initialized = true;

    EGLDisplay display = currentDisplay();

    if (display == EGL_NO_DISPLAY)
        return;

    if (!GLPlatformContext::supportsEGLExtension(display, "EGL_KHR_image_base"))
        return;

    if (!GLPlatformContext::supportsGLExtension("GL_OES_EGL_image"))
        return;

    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11 && !GLPlatformContext::supportsEGLExtension(display, "EGL_KHR_image_pixmap"))
        return;

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
        if (!GLPlatformContext::supportsEGLExtension(display, "EGL_WL_bind_wayland_display"))
            return;

        eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWL) eglGetProcAddress("eglBindWaylandDisplayWL");
        eglUnbindWaylandDisplayWL = (PFNEGLUNBINDWAYLANDDISPLAYWL) eglGetProcAddress("eglUnbindWaylandDisplayWL");
        eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWL) eglGetProcAddress("eglQueryWaylandBufferWL");
    }
#endif

    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
    eglImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
}

void EGLHelper::createEGLImage(EGLImageKHR* image, GLenum target, const EGLClientBuffer clientBuffer, const EGLint attributes[])
{
    EGLDisplay display = currentDisplay();

    if (display == EGL_NO_DISPLAY)
        return;

    EGLImageKHR tempHandle = EGL_NO_IMAGE_KHR;

    if (eglCreateImageKHR && eglImageTargetTexture2DOES && eglDestroyImageKHR)
        tempHandle = eglCreateImageKHR(display, EGL_NO_CONTEXT, target, clientBuffer, attributes);

    *image = tempHandle;
}

void EGLHelper::destroyEGLImage(const EGLImageKHR image)
{
    EGLDisplay display = currentDisplay();

    if (display == EGL_NO_DISPLAY)
        return;

    if (eglDestroyImageKHR)
        eglDestroyImageKHR(display, image);
}

void EGLHelper::imageTargetTexture2DOES(const EGLImageKHR image)
{
    eglImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
}

#if PLATFORM(WAYLAND)
bool EGLHelper::bindWaylandDisplay(struct wl_display *wlDisplay)
{
    EGLDisplay eglDisplay = currentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || !eglBindWaylandDisplayWL)
        return false;
    return eglBindWaylandDisplayWL(eglDisplay, wlDisplay);
}

bool EGLHelper::unbindWaylandDisplay(struct wl_display *wlDisplay)
{
    EGLDisplay eglDisplay = currentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY || !eglUnbindWaylandDisplayWL)
        return false;
    return eglUnbindWaylandDisplayWL(eglDisplay, wlDisplay);
}

bool EGLHelper::queryWaylandBuffer(struct wl_resource *buffer, EGLint attribute, EGLint *value)
{
    EGLDisplay display = currentDisplay();
    if (display == EGL_NO_DISPLAY || !eglQueryWaylandBufferWL)
        return false;
    return eglQueryWaylandBufferWL(display, buffer, attribute, value);
}
#endif // PLATFORM(WAYLAND)

}
#endif
