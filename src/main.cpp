#include <android_native_app_glue.h>
#include <android/input.h>
#include <cmath>
#include "vulkan_renderer.h"

enum NavState { NAV_IDLE, NAV_ORBIT, NAV_ZOOM_PAN };

struct AppState {
    VulkanRenderer renderer;
    NavState state = NAV_IDLE;
    bool animating = false;

    float lastX = 0, lastY = 0;
    float lastMidX = 0, lastMidY = 0;
    float lastDist = 0;
};

static float calcDist(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

static int32_t onInput(struct android_app* app, AInputEvent* event) {
    auto* s = (AppState*)app->userData;

    // 1. منع الخروج بزر الرجوع (Back Button Lock)
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        if (keyCode == AKEYCODE_BACK) {
            // اعتراض الحدث واستهلاكه؛ لا يمكن الخروج إلا بحذف التطبيق من الخلفية
            return 1;
        }
    }

    // 2. التحكم في الإيماءات واللمس الحر
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event);
        int masked = action & AMOTION_EVENT_ACTION_MASK;
        size_t count = AMotionEvent_getPointerCount(event);

        if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_CANCEL) {
            s->state = NAV_IDLE;
            return 1;
        }

        // حركة بإصبع واحد: دوران حر سلس (Free Orbit)
        if (count == 1) {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);

            if (masked == AMOTION_EVENT_ACTION_DOWN || s->state != NAV_ORBIT) {
                s->lastX = x; s->lastY = y;
                s->state = NAV_ORBIT;
                return 1;
            } else if (masked == AMOTION_EVENT_ACTION_MOVE && s->state == NAV_ORBIT) {
                float dx = x - s->lastX;
                float dy = y - s->lastY;
                s->renderer.camera.onOrbit(dx, dy);
                s->lastX = x; s->lastY = y;
                return 1;
            }
        } 
        // حركة بإصبعين: تحريك متوازن (Pan) + تقريب سلس (Zoom)
        else if (count >= 2) {
            float x0 = AMotionEvent_getX(event, 0), y0 = AMotionEvent_getY(event, 0);
            float x1 = AMotionEvent_getX(event, 1), y1 = AMotionEvent_getY(event, 1);
            float dist = calcDist(x0, y0, x1, y1);
            float midX = (x0 + x1) * 0.5f, midY = (y0 + y1) * 0.5f;

            if (masked == AMOTION_EVENT_ACTION_POINTER_DOWN || s->state != NAV_ZOOM_PAN) {
                s->lastDist = dist;
                s->lastMidX = midX; s->lastMidY = midY;
                s->state = NAV_ZOOM_PAN;
                return 1;
            } else if (masked == AMOTION_EVENT_ACTION_MOVE && s->state == NAV_ZOOM_PAN) {
                // تقريب
                float deltaDist = dist - s->lastDist;
                s->renderer.camera.onZoom(deltaDist);
                s->lastDist = dist;

                // تحريك في مستوى الكاميرا (Pan)
                float dMidX = midX - s->lastMidX;
                float dMidY = midY - s->lastMidY;
                s->renderer.camera.onPan(dMidX, dMidY);
                s->lastMidX = midX; s->lastMidY = midY;
                return 1;
            } else if (masked == AMOTION_EVENT_ACTION_POINTER_UP) {
                int upIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                int remainingIndex = (upIndex == 0) ? 1 : 0;
                s->lastX = AMotionEvent_getX(event, remainingIndex);
                s->lastY = AMotionEvent_getY(event, remainingIndex);
                s->state = NAV_ORBIT;
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
        int ident, events;
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
