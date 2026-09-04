#include <android_native_app_glue.h>
#include <android/input.h>
#include <cmath>
#include "vulkan_renderer.h"

struct AppState {
    VulkanRenderer renderer;
    bool animating = false;
    float lastX = 0, lastY = 0;
    float lastPinchDist = 0.0f;
    bool isPinching = false;
    bool isDragging = false;
};

static float getDist(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

static int32_t onInput(struct android_app* app, AInputEvent* event) {
    auto* s = (AppState*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event);
        int actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        size_t pointerCount = AMotionEvent_getPointerCount(event);

        if (pointerCount == 1) {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);

            if (actionMasked == AMOTION_EVENT_ACTION_DOWN) {
                s->lastX = x; s->lastY = y;
                s->isDragging = true;
                s->isPinching = false;
                return 1;
            } else if (actionMasked == AMOTION_EVENT_ACTION_MOVE && s->isDragging && !s->isPinching) {
                float dx = x - s->lastX;
                float dy = y - s->lastY;
                s->renderer.camera.onRotate(dx, dy);
                s->lastX = x; s->lastY = y;
                return 1;
            } else if (actionMasked == AMOTION_EVENT_ACTION_UP) {
                s->isDragging = false;
                return 1;
            }
        } else if (pointerCount >= 2) {
            float x0 = AMotionEvent_getX(event, 0);
            float y0 = AMotionEvent_getY(event, 0);
            float x1 = AMotionEvent_getX(event, 1);
            float y1 = AMotionEvent_getY(event, 1);
            float curDist = getDist(x0, y0, x1, y1);

            if (actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                s->lastPinchDist = curDist;
                s->lastX = (x0 + x1) * 0.5f;
                s->lastY = (y0 + y1) * 0.5f;
                s->isPinching = true;
                return 1;
            } else if (actionMasked == AMOTION_EVENT_ACTION_MOVE && s->isPinching) {
                // تكبير وتصغير سلس بإصبعين
                float deltaDist = curDist - s->lastPinchDist;
                s->renderer.camera.onZoom(deltaDist * 1.5f);
                s->lastPinchDist = curDist;

                // تحريك الكاميرا (Pan) بإصبعين
                float midX = (x0 + x1) * 0.5f;
                float midY = (y0 + y1) * 0.5f;
                s->renderer.camera.onPan(midX - s->lastX, midY - s->lastY);
                s->lastX = midX; s->lastY = midY;
                return 1;
            } else if (actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
                s->isPinching = false;
                return 1;
            }
        }
    }
    return 0;
}

static void onCmd(struct android_app* app, int32_t cmd) {
    auto* s = (AppState*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                s->renderer.init(app->window);
                s->animating = true;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            s->renderer.cleanup();
            s->animating = false;
            break;
        case APP_CMD_GAINED_FOCUS:
            s->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            s->animating = false;
            break;
    }
}

void android_main(struct android_app* app) {
    AppState state;
    app->userData = &state;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;

    while (true) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollOnce(state.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested != 0) {
                state.renderer.cleanup();
                return;
            }
        }

        if (state.animating) {
            state.renderer.renderFrame();
        }
    }
}
