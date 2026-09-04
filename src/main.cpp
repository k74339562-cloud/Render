#include <android_native_app_glue.h>
#include <android/input.h>
#include "vulkan_renderer.h"

struct AppState {
    VulkanRenderer renderer;
    bool animating = false;
    float lastX = 0, lastY = 0;
    bool dragging = false;
};

static int32_t onInput(struct android_app* app, AInputEvent* event) {
    auto* s = (AppState*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        if (action == AMOTION_EVENT_ACTION_DOWN) {
            s->lastX = x; s->lastY = y;
            s->dragging = true;
            return 1;
        } else if (action == AMOTION_EVENT_ACTION_MOVE && s->dragging) {
            float dx = x - s->lastX;
            float dy = y - s->lastY;
            s->renderer.camera.onRotate(dx, dy);
            s->lastX = x; s->lastY = y;
            return 1;
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            s->dragging = false;
            return 1;
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
