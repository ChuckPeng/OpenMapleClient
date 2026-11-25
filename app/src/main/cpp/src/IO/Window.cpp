//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "Window.h"
#include "../Configuration.h"
#include "../Constants.h"
#include "../Graphics/GraphicsGL.h"
#include "../Timer.h"
#include "UI.h"
#include "stb/stb_image.h"
#include "GLES3/gl32.h"
#include "gl.h"
#include "Char.h"
#include "MapPortals.h"
#include "Singleton.h"
#include "Stage.h"
#include "Util/Log.h"

#include <utility>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <android/log.h>
#include <jni.h>

#ifdef __ANDROID__
namespace {
std::mutex g_jni_mutex;
JavaVM *g_jvm = nullptr;
jobject g_maple_activity = nullptr;
jmethodID g_set_ime_visible_mid = nullptr;
}  // namespace
#endif

namespace ms {
    Window::Window() :
            display(nullptr),
            fullscreen_(true),
            opacity_(1.0f),
            opc_step_(0.0f),
            scale_(1.0),
            offset_x_(0.0),
            offset_y_(0.0),
            width_(Constants::Constants::get().get_viewwidth()),
            height_(Constants::Constants::get().get_viewheight()) {
    }

    Window::~Window() {
        display = nullptr;
//        glfmTerminate();
    }

    void error_callback(int no, const char *description) {
        MS_LOG_WARN << "GLFM error [" << no << "]: " << description;
    }

    bool key_callback(GLFMDisplay *display, GLFMKeyCode keyCode, GLFMKeyAction action,
                      int modifiers) {
        MS_LOG_DEBUG << "key_callback " << keyCode << " action " << action;
        UI::get().send_key(keyCode, action != GLFMKeyActionReleased);
        return true;
    }

    void char_callback(GLFMDisplay *display, const char *string, int modifiers) {
        MS_LOG_DEBUG << "char_callback " << string;
        // Determine the length of the wide string (Unicode) to allocate memory
        size_t length = mbstowcs(nullptr, string, 0);
        if (length == (size_t) -1) {
            // Error handling for invalid input string
            return;
        }

        // Allocate memory for the wide string
        wchar_t *wide_string = new wchar_t[length + 1]; // +1 for null terminator

        // Convert the input string to wide string (Unicode)
        mbstowcs(wide_string, string, length);
        wide_string[length] = L'\0'; // Null-terminate the wide string

        // Convert each wide character to uint32_t and use it as needed
        for (size_t i = 0; i < length; ++i) {
            uint32_t unicode_value = static_cast<uint32_t>(wide_string[i]);
            // Now you have the uint32_t representation of the Unicode character
            // You can use it as needed
            UI::get().send_key(unicode_value);
        }
        delete[] wide_string;
    }

    std::chrono::time_point<std::chrono::steady_clock> start =
            ContinuousTimer::get().start();

    std::chrono::time_point<std::chrono::steady_clock> click_start_time;

    bool sent = false;

    bool mousekey_callback(GLFMDisplay *display, int touch, GLFMTouchPhase phase,
                           double x, double y) {
        const double scale = Window::get().get_scale();
        Point<double_t> relative_pos =
            Point<double_t>((x - Window::get().get_offset_x()) / scale,
                            (y - Window::get().get_offset_y()) / scale);
        UI::get().set_touch_phase(touch, TouchInfo(relative_pos, phase));
        const bool send_cursor = UI::get().should_send_cursor();
        MS_LOG_DEBUG << "touch id " << touch << " phase " << static_cast<int>(phase)
                     << " at " << x << "," << y << " -> game "
                     << relative_pos.x() << "," << relative_pos.y()
                     << " send_cursor=" << send_cursor;
        if (send_cursor) {
            Window::get().move_cursor(x, y);
            switch (phase) {
                case GLFMTouchPhaseHover:
                    break;
                case GLFMTouchPhaseBegan: {
                    click_start_time = std::chrono::steady_clock::now();
                    UI::get().send_cursor(false);
                    break;
                }
                case GLFMTouchPhaseMoved: {
                    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - click_start_time).count();
                    if (diff_ms > 500) {
                        if (!sent)
                            UI::get().send_cursor(true);
                    } else {
                        UI::get().send_cursor(false);
                    }
                    break;
                }
                case GLFMTouchPhaseEnded: {
                    if (sent)
                        sent = false;
                    else {
                        UI::get().send_cursor(true);
                    }
                    auto diff_ms = ContinuousTimer::get().stop(start) / 1000;
                    start = ContinuousTimer::get().start();

                    if (diff_ms > 10 && diff_ms < 200) {
                        UI::get().doubleclick();
                    }
                    UI::get().send_cursor(false);
                    break;
                }
                case GLFMTouchPhaseCancelled:
                    break;
            }
        }

        return true;
    }

    void Window::move_cursor(double x, double y) {
        const double scale = Window::get().get_scale();
        auto xpos = static_cast<int16_t>(
            (x - Window::get().get_offset_x()) / scale);
        auto ypos = static_cast<int16_t>(
            (y - Window::get().get_offset_y()) / scale);
        Point<int16_t> pos = Point<int16_t>(xpos, ypos);
        UI::get().send_cursor(pos);
    }

    void focus_callback(GLFMDisplay *display, bool focused) {
        UI::get().send_focus(focused);
    }

    void keyboard_visibility_callback(GLFMDisplay *display,
                                      bool visible,
                                      double x,
                                      double y,
                                      double width,
                                      double height) {
        MS_LOG_INFO << "Keyboard visibility changed: visible=" << visible
                    << " frame=" << width << "x" << height;
        UI::get().on_keyboard_visibility_changed(visible);
    }

    void scroll_callback(GLFMDisplay *display, double xoffset, double yoffset) {
        UI::get().send_scroll(yoffset);
    }

    void close_callback(GLFMDisplay *display) {
        MS_LOG_INFO << "Surface destroyed";
    }

    void onDraw(GLFMDisplay *display) {
        // Preserve the aspect ratio of the virtual canvas: scale it uniformly
        // and letterbox the remainder of the screen.
        int display_width, display_height;
        glfmGetDisplaySize(display, &display_width, &display_height);

        static int last_display_width = -1;
        static int last_display_height = -1;
        if (display_width != last_display_width
            || display_height != last_display_height) {
            last_display_width = display_width;
            last_display_height = display_height;
            MS_LOG_INFO << "Display size changed to " << display_width << "x"
                        << display_height;
        }

        const double view_width =
            Constants::Constants::get().get_viewwidth();
        const double view_height =
            Constants::Constants::get().get_viewheight();
        const double scale =
            std::min(display_width / view_width, display_height / view_height);
        const int content_width =
            static_cast<int>(std::lround(view_width * scale));
        const int content_height =
            static_cast<int>(std::lround(view_height * scale));
        const int viewport_x = (display_width - content_width) / 2;
        const int viewport_y = (display_height - content_height) / 2;

        glViewport(viewport_x, viewport_y, content_width, content_height);
        Window::get().set_view_transform(
            scale,
            viewport_x,
            display_height - content_height - viewport_y);

        if (ms::program_ == 0) {
            program_ = glCreateProgram();
            GraphicsGL::get().init(display);
            GraphicsGL::get().reinit();
            Char::init();
            DamageNumber::init();
            MapPortals::init();
            Stage::get().init();
            UI::get().init();
            Timer::get().start();

            MS_LOG_INFO << "Display " << display_width << "x"
                        << display_height << ", game view " << content_width
                        << "x" << content_height << " at (" << viewport_x
                        << "," << viewport_y << "), virtual canvas "
                        << view_width << "x" << view_height;
        }
        const int64_t timestep = Constants::TIMESTEP * 1000;
        int64_t accumulator = timestep;
        if (game.is_running()) {
            int64_t elapsed = Timer::get().stop();

            // Update game with constant timestep as many times as possible.
            for (accumulator += elapsed; accumulator >= timestep;
                 accumulator -= timestep) {
                game.update();
            }
            float alpha = static_cast<float>(accumulator) / timestep;
            game.draw(alpha);
        }
    }

    Error Window::init(GLFMDisplay *pApp) {
        display = pApp;
        glfmSetDisplayConfig(display,
                             GLFMRenderingAPIOpenGLES32,
                             GLFMColorFormatRGBA8888,
                             GLFMDepthFormatNone,
                             GLFMStencilFormatNone,
                             GLFMMultisampleNone);
        glfmSetMultitouchEnabled(display, true);
        return init_window();
    }

    Error Window::init_window() {
        glViewport(0, 0, width_, height_);

        glfmSetMouseCursor(display, GLFMMouseCursorDefault);
        glfmSetKeyFunc(display, key_callback);
        glfmSetCharFunc(display, char_callback);
        glfmSetRenderFunc(display, onDraw);
        glfmSetTouchFunc(display, mousekey_callback);
//        glfmSetMouseWheelFunc(display, cursor_callback);
        glfmSetAppFocusFunc(display, focus_callback);
        glfmSetKeyboardVisibilityChangedFunc(display,
                                             keyboard_visibility_callback);
//        glfwSetScrollCallback(pApp, scroll_callback);
        glfmSetSurfaceDestroyedFunc(display, close_callback);

        return Error::Code::NONE;
    }

    bool Window::not_closed() const {
//        return glfwWindowShouldClose(glwnd_) == 0;
        return true;
    }

    void Window::update() {
        update_opc();
    }

    void Window::update_opc() {
        if (opc_step_ != 0.0f) {
            opacity_ += opc_step_;

            if (opacity_ >= 1.0f) {
                opacity_ = 1.0f;
                opc_step_ = 0.0f;
            } else if (opacity_ <= 0.0f) {
                opacity_ = 0.0f;
                opc_step_ = -opc_step_;

                fade_procedure_();
            }
        }
    }

    void Window::check_events() {
        int16_t max_width = Configuration::get().get_max_width();
        int16_t max_height = Configuration::get().get_max_height();
        int16_t new_width = Constants::Constants::get().get_viewwidth();
        int16_t new_height = Constants::Constants::get().get_viewheight();

        if (width_ != new_width || height_ != new_height) {
            width_ = new_width;
            height_ = new_height;

            if (new_width >= max_width || new_height >= max_height) {
                fullscreen_ = true;
            }

//            init_window();
        }

//        glfwPollEvents();
    }

    void Window::begin() const {
        GraphicsGL::get().clear_scene();
    }

    void Window::end() const {
        GraphicsGL::get().flush(opacity_);
        glfmSwapBuffers(display);
    }

    void Window::fadeout(float step, std::function<void()> fadeproc) {
        opc_step_ = -step;
        fade_procedure_ = std::move(fadeproc);
    }

    void Window::set_clipboard(const std::string &text) const {
//        glfwSetClipboardString(glwnd_, text.c_str());
    }

    std::string Window::get_clipboard() const {
//        const char *text = glfwGetClipboardString(glwnd_);

//        return text != nullptr ? text : "";
        return "";
    }

    void Window::toggle_fullscreen() {
        int16_t max_width = Configuration::get().get_max_width();
        int16_t max_height = Configuration::get().get_max_height();

        if (width_ < max_width && height_ < max_height) {
            fullscreen_ = !fullscreen_;
            Setting<Fullscreen>::get().save(fullscreen_);

//            init_window();
//            glfwPollEvents();
        }
    }

    double Window::get_scale() {
        return scale_;
    }

    double Window::get_offset_x() {
        return offset_x_;
    }

    double Window::get_offset_y() {
        return offset_y_;
    }

    void Window::set_view_transform(double scale,
                                    double offset_x,
                                    double offset_y) {
        scale_ = scale;
        offset_x_ = offset_x;
        offset_y_ = offset_y;
    }

    GLFMDisplay *Window::get_display() {
        return display;
    }

    void Window::set_keyboard_visible(bool visible) {
#ifdef __ANDROID__
        {
            std::lock_guard<std::mutex> lock(g_jni_mutex);
            if (g_jvm && g_maple_activity && g_set_ime_visible_mid) {
                JNIEnv *env = nullptr;
                bool detach = false;
                jint res = g_jvm->GetEnv(reinterpret_cast<void **>(&env),
                                         JNI_VERSION_1_6);
                if (res == JNI_EDETACHED) {
                    if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                        detach = true;
                    }
                }
                if (env) {
                    env->CallVoidMethod(g_maple_activity,
                                        g_set_ime_visible_mid,
                                        visible ? JNI_TRUE : JNI_FALSE);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                    }
                    if (detach) {
                        g_jvm->DetachCurrentThread();
                    }
                    return;
                }
            }
        }
#endif
        if (display) {
            glfmSetKeyboardVisible(display, visible);
        }
    }
}  // namespace ms

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL
Java_openMapleClient_MapleActivity_nativeRegisterActivity(JNIEnv *env,
                                                          jobject thiz) {
    std::lock_guard<std::mutex> lock(g_jni_mutex);
    env->GetJavaVM(&g_jvm);
    if (g_maple_activity) {
        env->DeleteGlobalRef(g_maple_activity);
        g_maple_activity = nullptr;
    }
    g_maple_activity = env->NewGlobalRef(thiz);
    jclass cls = env->GetObjectClass(thiz);
    g_set_ime_visible_mid =
            env->GetMethodID(cls, "setImeVisibleFromNative", "(Z)V");
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        g_set_ime_visible_mid = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_openMapleClient_MapleActivity_nativeUnregisterActivity(
        JNIEnv *env,
        jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_jni_mutex);
    if (g_maple_activity) {
        env->DeleteGlobalRef(g_maple_activity);
        g_maple_activity = nullptr;
    }
    g_set_ime_visible_mid = nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_openMapleClient_MapleActivity_nativeOnImeVisibilityChanged(
        JNIEnv * /*env*/,
        jobject /*thiz*/,
        jboolean visible) {
    const bool is_visible = (visible == JNI_TRUE);
    MS_LOG_INFO << "Android IME visibility changed: visible=" << is_visible;
    if (!is_visible) {
        ms::UI::get().notify_ime_hidden_from_platform();
    }
}
#endif