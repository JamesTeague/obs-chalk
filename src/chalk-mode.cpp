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

// Forward declare filter class
class ChalkEventFilter;
static ChalkEventFilter   *s_filter               = nullptr;

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

    s_preview = main->findChild<QWidget *>("preview");
    if (!s_preview) {
        blog(LOG_WARNING,
             "obs-chalk: chalk_mode_install — could not find preview widget; "
             "event filter not installed (source interaction callbacks still work)");
        return;
    }

    s_filter = new ChalkEventFilter(s_preview);
    s_preview->installEventFilter(s_filter);

    // Register global chalk mode hotkey.
    // NOTE: Registering here (FINISHED_LOADING) rather than obs_module_load means
    // saved bindings from scene collection will NOT be restored on restart.
    // Move to obs_module_load if binding persistence is required (see research Pitfall 6).
    s_chalk_mode_hotkey = obs_hotkey_register_frontend(
        "chalk.chalk_mode",
        "Chalk: Toggle Chalk Mode",
        chalk_mode_hotkey_cb,
        nullptr);

    blog(LOG_INFO, "obs-chalk: chalk mode installed on preview widget");
}

void chalk_mode_shutdown()
{
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
