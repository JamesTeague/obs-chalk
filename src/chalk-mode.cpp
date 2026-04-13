// chalk-mode.cpp
//
// Installs a Qt event filter on the OBS preview widget to intercept mouse and
// tablet events when chalk mode is active. Coordinates are mapped from preview
// widget space to OBS scene space using the same formula as OBSBasicPreview.
//
// Thread context: all functions run on the Qt main thread (same as OBS source
// interaction callbacks). mark_list.mutex protects against the render thread.
//
// ENABLE_FRONTEND_API must be ON (set in CMakeLists.txt).

#include "chalk-mode.hpp"
#include "chalk-dock.hpp"
#include "chalk-source.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/vec2.h>

#include <QMainWindow>
#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QWidget>
#include <QGuiApplication>

#ifdef _WIN32
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// Coordinate mapping
// ---------------------------------------------------------------------------

static constexpr int CHALK_PREVIEW_EDGE = 10; // PREVIEW_EDGE_SIZE from OBSBasic.hpp

// Map a preview widget position (logical pixels) to OBS scene coordinates.
// Reimplements GetScaleAndCenterPos + PREVIEW_EDGE_SIZE offset from OBSBasic.
// TODO: fixed-scale mode (GetCenterPosFromFixedScale + scroll offsets)
static vec2 preview_widget_to_scene(QWidget *preview, float event_x, float event_y)
{
    float dpr = static_cast<float>(preview->devicePixelRatioF());

    // Physical pixel dimensions of the preview widget
    int phys_w = static_cast<int>(preview->width()  * dpr);
    int phys_h = static_cast<int>(preview->height() * dpr);

    // OBS canvas dimensions
    obs_video_info ovi = {};
    obs_get_video_info(&ovi);

    // Letterbox offset and scale — reimplemented from display-helpers.hpp
    // (cannot include that header from a plugin without pulling in internal OBS headers)
    int   off_x, off_y;
    float scale;
    {
        int inner_w = phys_w - CHALK_PREVIEW_EDGE * 2;
        int inner_h = phys_h - CHALK_PREVIEW_EDGE * 2;

        double wAspect = static_cast<double>(inner_w) / static_cast<double>(inner_h);
        double bAspect = static_cast<double>(ovi.base_width) /
                         static_cast<double>(ovi.base_height);

        if (wAspect > bAspect) {
            scale = static_cast<float>(inner_h) / static_cast<float>(ovi.base_height);
        } else {
            scale = static_cast<float>(inner_w) / static_cast<float>(ovi.base_width);
        }

        int scaled_w = static_cast<int>(ovi.base_width  * scale);
        int scaled_h = static_cast<int>(ovi.base_height * scale);
        off_x = (inner_w - scaled_w) / 2 + CHALK_PREVIEW_EDGE;
        off_y = (inner_h - scaled_h) / 2 + CHALK_PREVIEW_EDGE;
    }

    // event_x/y are in logical pixels; convert to physical, then to scene
    float scene_x = (event_x * dpr - static_cast<float>(off_x)) / scale;
    float scene_y = (event_y * dpr - static_cast<float>(off_y)) / scale;

    vec2 result;
    vec2_set(&result, scene_x, scene_y);
    return result;
}

// ---------------------------------------------------------------------------
// File-scope state
// ---------------------------------------------------------------------------

static QWidget            *s_preview              = nullptr;
static obs_hotkey_id       s_chalk_mode_hotkey     = OBS_INVALID_HOTKEY_ID;
static bool                s_chalk_mode_active     = false;
static ChalkDock          *s_dock                 = nullptr;

// Forward declare filter class
class ChalkEventFilter;
static ChalkEventFilter   *s_filter               = nullptr;

#ifdef _WIN32
class ChalkNativeFilter;
static ChalkNativeFilter  *s_native_filter         = nullptr;
#endif

// ---------------------------------------------------------------------------
// Cursor helper
// ---------------------------------------------------------------------------

static void chalk_update_cursor()
{
    if (s_chalk_mode_active)
        QGuiApplication::setOverrideCursor(Qt::CrossCursor);
    else
        QGuiApplication::restoreOverrideCursor();
}

// ---------------------------------------------------------------------------
// Public accessor — read by ChalkDock::refresh() on Qt main thread
// ---------------------------------------------------------------------------

bool chalk_mode_is_active()
{
    return s_chalk_mode_active;
}

// ---------------------------------------------------------------------------
// Global hotkey callback
// NOTE: Registered in chalk_mode_install (OBS_FRONTEND_EVENT_FINISHED_LOADING).
// If saved bindings are lost on restart, move registration to obs_module_load.
// See research Pitfall 6.
// ---------------------------------------------------------------------------

static void chalk_mode_hotkey_cb(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    s_chalk_mode_active = !s_chalk_mode_active;
    chalk_update_cursor();
}

// ---------------------------------------------------------------------------
// ChalkNativeFilter — Win32 mouse message interception (Windows only)
//
// Qt eventFilter does not receive mouse events on Windows because
// OBSQTDisplay sets WA_NativeWindow, creating a separate HWND that
// dispatches messages directly to the widget. This native event filter
// intercepts WM_LBUTTONDOWN/MOVE/UP before Qt processes them.
// ---------------------------------------------------------------------------

#ifdef _WIN32
class ChalkNativeFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &eventType,
                           void *message,
                           qintptr *result) override
    {
        if (eventType != "windows_generic_MSG")
            return false;

        auto *msg = static_cast<MSG *>(message);

        // Early exit for non-mouse messages
        if (msg->message != WM_LBUTTONDOWN &&
            msg->message != WM_MOUSEMOVE &&
            msg->message != WM_LBUTTONUP)
            return false;

        if (!s_chalk_mode_active || !s_preview)
            return false;

        // HWND matching fails on Windows — the display surface creates
        // unrelated HWNDs. Instead, convert event coords to screen space
        // and check if they fall within the preview widget's bounds.
        POINT screenPt;
        screenPt.x = static_cast<short>(LOWORD(msg->lParam));
        screenPt.y = static_cast<short>(HIWORD(msg->lParam));
        ClientToScreen(msg->hwnd, &screenPt);

        QPoint previewOrigin = s_preview->mapToGlobal(QPoint(0, 0));
        int pw = s_preview->width();
        int ph = s_preview->height();
        if (pw < 1 || ph < 1) return false;
        float rel_x = static_cast<float>(screenPt.x - previewOrigin.x());
        float rel_y = static_cast<float>(screenPt.y - previewOrigin.y());

        if (rel_x < 0 || rel_y < 0 || rel_x >= pw || rel_y >= ph)
            return false;

        // rel_x/rel_y are in logical pixels relative to preview widget
        ChalkSource *ctx = chalk_find_source();
        if (!ctx) return false;

        switch (msg->message) {
        case WM_LBUTTONDOWN: {
            vec2 pos = preview_widget_to_scene(s_preview, rel_x, rel_y);
            chalk_input_begin(ctx, pos.x, pos.y);
            if (result) *result = 0;
            return true;
        }
        case WM_MOUSEMOVE: {
            if (!(msg->wParam & MK_LBUTTON))
                return false;
            vec2 pos = preview_widget_to_scene(s_preview, rel_x, rel_y);
            chalk_input_move(ctx, pos.x, pos.y);
            if (result) *result = 0;
            return true;
        }
        case WM_LBUTTONUP: {
            chalk_input_end(ctx);
            if (result) *result = 0;
            return true;
        }
        default:
            return false;
        }
    }
};
#endif

// ---------------------------------------------------------------------------
// ChalkEventFilter — Q_OBJECT class defined in .cpp (requires #include "chalk-mode.moc")
// ---------------------------------------------------------------------------

class ChalkEventFilter : public QObject {
    Q_OBJECT
public:
    explicit ChalkEventFilter(QObject *parent = nullptr)
        : QObject(parent) {}

protected:
    bool eventFilter(QObject * /*obj*/, QEvent *event) override
    {
        if (!s_chalk_mode_active)
            return false; // pass through to normal OBS behavior

        switch (event->type()) {
            case QEvent::TabletPress:
            case QEvent::TabletMove:
            case QEvent::TabletRelease:
                return handle_tablet(static_cast<QTabletEvent *>(event));

            case QEvent::MouseButtonPress: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    vec2 pos = preview_widget_to_scene(
                        s_preview,
                        static_cast<float>(me->position().x()),
                        static_cast<float>(me->position().y()));
                    ChalkSource *ctx = chalk_find_source();
                    // Mouse has no pressure — uses default 1.0f (full width)
                    if (ctx) chalk_input_begin(ctx, pos.x, pos.y);
                    return true;
                }
                return false;
            }

            case QEvent::MouseMove: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->buttons() & Qt::LeftButton) {
                    vec2 pos = preview_widget_to_scene(
                        s_preview,
                        static_cast<float>(me->position().x()),
                        static_cast<float>(me->position().y()));
                    ChalkSource *ctx = chalk_find_source();
                    // Mouse has no pressure — uses default 1.0f (full width)
                    if (ctx) chalk_input_move(ctx, pos.x, pos.y);
                    return true;
                }
                return false;
            }

            case QEvent::MouseButtonRelease: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    ChalkSource *ctx = chalk_find_source();
                    if (ctx) chalk_input_end(ctx);
                    return true;
                }
                return false;
            }

            default:
                return false;
        }
    }

private:
    bool handle_tablet(QTabletEvent *e)
    {
        // Extract position (use position(), not deprecated pos())
        float wx = static_cast<float>(e->position().x());
        float wy = static_cast<float>(e->position().y());

        // Extract pressure (0.0-1.0). QTabletEvent::pressure() returns qreal.
        float pressure = static_cast<float>(e->pressure());

        vec2 pos = preview_widget_to_scene(s_preview, wx, wy);
        ChalkSource *ctx = chalk_find_source();

        if (ctx) {
            switch (e->type()) {
                case QEvent::TabletPress:
                    // Suppress ghost strokes: some tablets report hover as pressure-0 press.
                    // Treat pressure < 0.01 as no-op so invisible strokes are not started.
                    if (pressure >= 0.01f) {
                        chalk_input_begin(ctx, pos.x, pos.y, pressure);
                    }
                    break;
                case QEvent::TabletMove:
                    chalk_input_move(ctx, pos.x, pos.y, pressure);
                    break;
                case QEvent::TabletRelease:
                    chalk_input_end(ctx);
                    break;
                default:
                    break;
            }
        }

        // Return true to prevent Qt from synthesizing mouse events from tablet input
        // (see research Pitfall 3 — double dispatch)
        return true;
    }
};

// MOC include — required when Q_OBJECT class is defined in a .cpp file
#include "chalk-mode.moc"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void chalk_mode_install()
{
    QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main) {
        blog(LOG_WARNING, "obs-chalk: chalk_mode_install — could not get main window");
        return;
    }

    // OBS has multiple widgets named "preview". The real one is OBSBasicPreview
    // (visible, large). findChild returns the first match which may be a hidden
    // OBSQTDisplay stub. Find all and pick the visible one with the largest area.
    QList<QWidget *> candidates = main->findChildren<QWidget *>("preview");
    for (QWidget *w : candidates) {
        if (w->isVisible() && w->width() > 100 && w->height() > 100) {
            s_preview = w;
            break;
        }
    }
    if (!s_preview && !candidates.isEmpty())
        s_preview = candidates.last(); // fallback
    if (!s_preview) {
        blog(LOG_WARNING,
             "obs-chalk: could not find preview widget; "
             "event filter not installed (source interaction callbacks still work)");
        return;
    }

    s_filter = new ChalkEventFilter(s_preview);
    s_preview->installEventFilter(s_filter);

#ifdef _WIN32
    s_native_filter = new ChalkNativeFilter();
    QCoreApplication::instance()->installNativeEventFilter(s_native_filter);
    blog(LOG_INFO, "obs-chalk: native event filter installed (geometry-based matching)");
#endif

    s_chalk_mode_hotkey = obs_hotkey_register_frontend(
        "chalk.chalk_mode",
        "Chalk: Toggle Chalk Mode",
        chalk_mode_hotkey_cb,
        nullptr);

    blog(LOG_INFO, "obs-chalk: chalk mode installed on preview widget "
         "(class=%s, geo=(%d,%d)+%dx%d)",
         s_preview->metaObject()->className(),
         s_preview->mapToGlobal(QPoint(0, 0)).x(),
         s_preview->mapToGlobal(QPoint(0, 0)).y(),
         s_preview->width(), s_preview->height());

    s_dock = new ChalkDock();
    obs_frontend_add_dock_by_id("chalk-status",
                                 obs_module_text("ChalkStatus"),
                                 s_dock);
    blog(LOG_INFO, "obs-chalk: chalk dock installed");
}

void chalk_mode_shutdown()
{
    if (s_dock) {
        obs_frontend_remove_dock("chalk-status");
        s_dock = nullptr; // OBS owns widget — do NOT delete
    }

#ifdef _WIN32
    if (s_native_filter) {
        QCoreApplication::instance()->removeNativeEventFilter(s_native_filter);
        delete s_native_filter;
        s_native_filter = nullptr;
    }
#endif

    if (s_preview && s_filter) {
        s_preview->removeEventFilter(s_filter);
        // s_filter is parented to s_preview — Qt will delete it when preview is destroyed.
        // Explicitly nulling here to prevent dangling use.
        s_filter  = nullptr;
        s_preview = nullptr;
    }

    if (s_chalk_mode_hotkey != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(s_chalk_mode_hotkey);
        s_chalk_mode_hotkey = OBS_INVALID_HOTKEY_ID;
    }

    s_chalk_mode_active = false;
}
