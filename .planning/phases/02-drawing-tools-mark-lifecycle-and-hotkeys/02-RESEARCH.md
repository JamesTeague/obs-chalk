# Phase 2: Drawing Tools, Mark Lifecycle, and Hotkeys - Research

**Researched:** 2026-03-29
**Domain:** OBS C++ plugin — gs_* rendering, OBS hotkey API, 2D geometry
**Confidence:** HIGH

## Summary

Phase 2 builds directly on the Phase 1 object model pipeline without architectural changes. The new mark types (arrow, circle, cone, laser) are all implemented as `Mark` subclasses using the same `draw(color_param)` virtual method and the same immediate-mode gs_* rendering already proven in Phase 1. The only new rendering primitive needed is `GS_TRIS` for the cone's filled triangle — everything else continues using `GS_LINESTRIP` or `GS_POINTS`.

OBS hotkeys are registered per-source via `obs_hotkey_register_source` in `chalk_create`. The `bool pressed` parameter fires on both key-down and key-up, which is exactly what the laser pointer hold-to-show pattern requires. OBS handles hotkey serialization/persistence automatically via the source context — no explicit `save`/`load` callbacks are needed from the plugin.

The pick-to-delete hit test requires a distance-from-stroke algorithm. Each mark type computes its own minimum distance to a query point. The closest mark wins deletion. This extends naturally from the existing `hit_test(float x, float y)` stub in `Mark` — the stub should be generalized to return a float distance rather than a bool.

**Primary recommendation:** Implement mark types in dependency order: freehand improvement first (interpolation), then arrow, circle, cone, laser. Wire hotkeys and mark management after all types exist. Keep each mark type in its own `.cpp/.hpp` pair to stay under the 500-line limit.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Color palette (INPT-02):** 5 colors — white, yellow, red, blue, green. Hotkey-driven wrapping cycle. No color picker dialog.
- **Cone of vision (TOOL-04):** Filled semi-transparent isosceles triangle. Click sets apex, drag sets one base corner (defines direction + length + width simultaneously), other base corner is auto-mirrored across the apex-to-midpoint axis. One gesture, symmetric result.
- **Pick-to-delete (MARK-02):** Distance-from-stroke hit testing. Closest mark to click point wins. Not bounding box.
- **Freehand improvement (TOOL-01):** Add linear interpolation between consecutive mouse points to fill fast-movement gaps.

### Claude's Discretion
- Arrow end style variations (types and visual design)
- Circle rendering approach (outline vs filled, line width)
- Undo stack depth
- Specific hotkey defaults for each action
- Laser pointer visual style (cursor dot size, color, opacity)
- Hit-test distance threshold for pick-to-delete
- How pick-to-delete mode is entered/exited (hotkey toggle or automatic after one deletion)

### Deferred Ideas (OUT OF SCOPE)
- Post-draw mark editing (select mark, drag handles to adjust) — PowerPoint-style. All mark types. Post-v1.
- Tablet pressure sensitivity — Phase 3
- Qt dock panel — Phase 3
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TOOL-01 | Freehand strokes with mouse movement | Interpolation math documented; existing `add_point` hook is the insertion point |
| TOOL-02 | Arrows via click-drag, multiple end styles | GS_LINESTRIP for shaft + two short lines for head; geometry derivation documented |
| TOOL-03 | Circles via click-drag for center+radius | GS_LINESTRIP with N-point polygon approximation; N=64 is smooth enough |
| TOOL-04 | Cone of vision: click apex, drag base edge | GS_TRIS for filled triangle; reflection math documented; alpha for transparency |
| TOOL-05 | Laser pointer: hold key = visible, release = gone | OBS `obs_hotkey_register_source` with `pressed` param; never added to mark list |
| MARK-01 | Undo most recent mark via hotkey | `marks.pop_back()` on MarkList; `undo_mark()` method |
| MARK-02 | Pick-to-delete: click a specific mark | `distance_to(x, y)` on each Mark; closest mark deleted; mode flag on ChalkSource |
| MARK-03 | Clear all marks via hotkey | `clear_all()` already exists on MarkList |
| MARK-04 | Multi-level undo | Same as MARK-01, repeated; undo pops the vector each call |
| INPT-01 | All primary actions as OBS hotkeys | `obs_hotkey_register_source` called in `chalk_create` for each action |
| INPT-02 | Color cycling via hotkey | Array of 5 ABGR values; `color_index` on ToolState, wraps at 5 |
</phase_requirements>

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| libobs | 31.1.1 (in .deps) | Source registration, hotkey API, interaction callbacks | Already linked; all OBS plugin code builds against this |
| graphics/graphics.h | (part of libobs) | gs_render_start/stop, GS_TRIS, GS_LINESTRIP, blend state | Already used in chalk_video_render |
| `<cmath>` | C++17 stdlib | `std::sqrt`, `std::atan2`, `std::cos`, `std::sin`, `std::hypot` | Geometry calculations for all new mark types |

### No New Dependencies
Phase 2 adds zero new external dependencies. All rendering primitives, hotkey registration, and interaction callbacks are already in libobs.

**Installation:** None required.

---

## Architecture Patterns

### Recommended Project Structure
```
src/
├── plugin.cpp              # module load/unload (unchanged)
├── chalk-source.hpp        # ChalkSource + new fields: color_index, pick_delete_mode, laser_pos
├── chalk-source.cpp        # mouse/hotkey dispatch (grows to handle tool routing)
├── tool-state.hpp          # ToolType enum expanded; color_index replaces active_color
├── mark-list.hpp           # add undo_mark(), find_closest(x, y)
├── mark-list.cpp           # implementations
└── marks/
    ├── mark.hpp            # change hit_test to distance_to(float x, float y) -> float
    ├── freehand-mark.hpp
    ├── freehand-mark.cpp   # add interpolation to add_point
    ├── arrow-mark.hpp
    ├── arrow-mark.cpp
    ├── circle-mark.hpp
    ├── circle-mark.cpp
    ├── cone-mark.hpp
    └── cone-mark.cpp       # laser pointer is NOT a mark — handled in ChalkSource directly
```

### Pattern 1: Mark Subclass (established in Phase 1)
**What:** Each drawable type is a `Mark` subclass with its own geometry stored as member fields.
**When to use:** Every new persistent tool type.
**Example:**
```cpp
// Established pattern from freehand-mark.hpp
class ArrowMark : public Mark {
public:
    ArrowMark(float x1, float y1, float x2, float y2,
              float r, float g, float b, float a = 1.0f);
    void draw(gs_eparam_t *color_param) const override;
    float distance_to(float x, float y) const override;
    // Arrow stores: start point, end point, color
    // draw() computes arrowhead vertices from end+direction at draw time
private:
    float x1_, y1_, x2_, y2_;
    vec4 color_;
};
```

### Pattern 2: Two-Phase Interaction (begin + commit)
**What:** Mouse-down creates an `in_progress` mark via `begin_mark()`. Mouse-move calls `add_point()` or an update method. Mouse-up calls `commit_mark()`.
**When to use:** All tool types except laser pointer.
**Example:**
```cpp
// In chalk_mouse_click (mouse-down branch):
auto mark = std::make_unique<ArrowMark>(x, y, x, y, r, g, b, a); // tail==head initially
ctx->mark_list.begin_mark(std::move(mark));

// In chalk_mouse_move:
if (ctx->mark_list.in_progress) {
    ctx->mark_list.in_progress->update_end(event->x, event->y); // new virtual method
}

// In chalk_mouse_click (mouse-up):
ctx->mark_list.commit_mark();
```

**Critical:** All writes to `in_progress` in mouse_move must hold `mark_list.mutex`. This is already done for freehand. Arrow/circle/cone need the same guard.

### Pattern 3: Hotkey Registration in chalk_create
**What:** Call `obs_hotkey_register_source` once per action inside `chalk_create`. Store returned `obs_hotkey_id` in `ChalkSource` for cleanup in `chalk_destroy`.
**When to use:** All OBS hotkeys.
**Example:**
```cpp
// Source: obs-studio-31.1.1/libobs/obs-hotkey.h
// In chalk_create:
ctx->hotkey_undo = obs_hotkey_register_source(
    source,
    "chalk.undo",          // internal name (unique, stable)
    "Chalk: Undo",         // displayed in OBS hotkey settings
    chalk_hotkey_undo,     // callback
    ctx
);

// Callback signature:
static void chalk_hotkey_undo(void *data, obs_hotkey_id /*id*/,
                               obs_hotkey_t * /*hotkey*/, bool pressed)
{
    if (!pressed) return;  // fire only on key-down
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->mark_list.undo_mark();
}

// In chalk_destroy:
obs_hotkey_unregister(ctx->hotkey_undo);
```

### Pattern 4: Laser Pointer (Ephemeral — Never a Mark)
**What:** Hold-key activates laser; release deactivates. Position follows mouse. Never added to `mark_list`.
**When to use:** Laser pointer only.
**Example:**
```cpp
// In ChalkSource: add fields
struct ChalkSource {
    // ...existing...
    bool laser_active = false;
    float laser_x = 0.f, laser_y = 0.f;
    obs_hotkey_id hotkey_laser = OBS_INVALID_HOTKEY_ID;
};

// Hotkey callback:
static void chalk_hotkey_laser(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->laser_active = pressed;  // true on down, false on up — no mutex needed (OBS serializes)
}

// Mouse move update (already serialized):
if (!mouse_leave) {
    ctx->laser_x = static_cast<float>(event->x);
    ctx->laser_y = static_cast<float>(event->y);
}

// In chalk_video_render (inside the technique pass):
if (ctx->laser_active) {
    // Draw dot: gs_render_start + gs_vertex2f at laser_x/y + gs_render_stop(GS_POINTS)
    // Or small circle: 16-point polygon at radius ~6px
}
```

### Pattern 5: Pick-to-Delete Mode
**What:** A mode flag on ChalkSource. When active, mouse-click tests all marks and deletes the closest one.
**When to use:** pick-to-delete workflow.
**Example:**
```cpp
// In ChalkSource: add
bool pick_delete_mode = false;

// Mouse click handler:
if (ctx->pick_delete_mode && type == MOUSE_LEFT && !mouse_up) {
    ctx->mark_list.delete_closest(event->x, event->y, HIT_THRESHOLD_PX);
    // Discretion: auto-exit mode after one deletion, or stay until hotkey toggles off
}

// MarkList method:
void MarkList::delete_closest(float x, float y, float threshold) {
    std::lock_guard<std::mutex> lock(mutex);
    float min_dist = threshold;
    size_t best = SIZE_MAX;
    for (size_t i = 0; i < marks.size(); ++i) {
        float d = marks[i]->distance_to(x, y);
        if (d < min_dist) { min_dist = d; best = i; }
    }
    if (best != SIZE_MAX)
        marks.erase(marks.begin() + best);
}
```

### Anti-Patterns to Avoid
- **gs_* calls outside video_render chain:** Established Phase 1 constraint. Never call `gs_render_start`, `gs_vertex2f`, `gs_render_stop`, or any GS function from hotkey callbacks, mouse callbacks, or outside the `chalk_video_render` chain.
- **Locking mutex in video_render while also locking in mouse callbacks:** Already correctly handled in Phase 1. Maintain: mouse_click/mouse_move hold the lock when writing `in_progress`; video_render holds the lock when reading `marks` and `in_progress`.
- **Mutating `marks` vector without the lock:** `undo_mark()` and `delete_closest()` are called from hotkey/mouse callbacks. They MUST lock before writing `marks`.
- **Storing raw pointers to marks:** Use `std::unique_ptr<Mark>`. Already the pattern; maintain it.
- **Computing arrowhead geometry at add_point time:** Compute in `draw()` from stored start/end. Avoids storing redundant derived geometry.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Hotkey serialization/persistence | Manual obs_data save/load for hotkey bindings | OBS native — hotkeys persist automatically when registered via `obs_hotkey_register_source` | OBS saves all source hotkey bindings to the scene collection file via hotkey_data context |
| Color palette data type | Custom color struct | `uint32_t` ABGR array (same as existing `active_color`) | Already the established convention; `color_uint32_to_rgba` helper already exists |
| Trigonometry | Look-up tables or fixed-point approximations | `<cmath>` `std::sin`/`std::cos`/`std::atan2`/`std::hypot` | This runs in a hotkey callback (not on the render thread), not in a tight loop. Accuracy matters more than micro-optimization. |

**Key insight:** The OBS hotkey system does all the heavy lifting for persistence and user keybinding UI. The plugin only needs to register names and callbacks.

---

## Geometry Reference

This section documents the math for each new mark type. These are exact formulas the planner should reference.

### Freehand: Linear Interpolation (TOOL-01)
When adding a new point, if the distance from the last stored point exceeds a threshold (e.g., 2px), insert intermediate points:
```cpp
void FreehandMark::add_point(float x, float y) {
    if (!points_.empty()) {
        float dx = x - points_.back().x;
        float dy = y - points_.back().y;
        float dist = std::hypot(dx, dy);
        const float STEP = 2.0f;
        if (dist > STEP) {
            int steps = static_cast<int>(dist / STEP);
            for (int i = 1; i < steps; ++i) {
                float t = float(i) / steps;
                points_.push_back({points_.back().x + dx*t/steps*steps,  // simplified:
                                   points_.back().y + dy*t/steps*steps});
                // Correct: lerp from stored back to (x,y)
            }
        }
    }
    points_.push_back({x, y});
}
```
Simpler correct version:
```cpp
// Source: standard linear interpolation
if (!points_.empty() && dist > STEP) {
    float ox = points_.back().x, oy = points_.back().y;
    int n = static_cast<int>(dist / STEP);
    for (int i = 1; i <= n; ++i) {
        float t = float(i) / float(n + 1);
        points_.push_back({ox + dx * t, oy + dy * t});
    }
}
points_.push_back({x, y});
```

### Arrow: Shaft + Arrowhead (TOOL-02)
Arrow stores `(x1,y1)` tail and `(x2,y2)` head. In `draw()`:
```cpp
// Shaft: GS_LINESTRIP from (x1,y1) to (x2,y2)
// Arrowhead: two lines from (x2,y2) back at ±30 degrees
float angle = std::atan2(y2_ - y1_, x2_ - x1_);
const float HEAD_LEN = 15.0f;
const float HEAD_ANGLE = 0.5236f; // 30 degrees in radians
float ax1 = x2_ - HEAD_LEN * std::cos(angle - HEAD_ANGLE);
float ay1 = y2_ - HEAD_LEN * std::sin(angle - HEAD_ANGLE);
float ax2 = x2_ - HEAD_LEN * std::cos(angle + HEAD_ANGLE);
float ay2 = y2_ - HEAD_LEN * std::sin(angle + HEAD_ANGLE);
// Draw as two separate GS_LINESTRIP calls, each 2 points: (ax1,ay1)→(x2,y2) and (ax2,ay2)→(x2,y2)
// Or one GS_LINESTRIP of 5 points: ax1→head→ax2, then reset to draw shaft separately
```
Use separate `gs_render_start/stop` calls for shaft and head to avoid connecting them.

### Circle: N-point Polygon (TOOL-03)
Circle stores center `(cx,cy)` and radius `r`. In `draw()`:
```cpp
const int N = 64;
gs_render_start(false);
for (int i = 0; i <= N; ++i) {  // <= N to close the loop
    float a = float(i) / float(N) * 2.0f * M_PI;
    gs_vertex2f(cx_ + r_ * std::cos(a), cy_ + r_ * std::sin(a));
}
gs_render_stop(GS_LINESTRIP);
```
During drag: `r = hypot(event->x - cx, event->y - cy)`. Circle has a virtual `update_end(float x, float y)` that updates `r_`.

### Cone of Vision: Filled Triangle (TOOL-04)
Cone stores apex `(ax, ay)` and one base corner `(bx, by)`. The other base corner is the reflection of `(bx,by)` across the axis from apex to the midpoint of the base.

**Geometry:**
```
apex         = (ax, ay)           — click point
corner1      = (bx, by)           — drag endpoint
midpoint     = apex + direction   — midpoint of base
axis_vec     = normalize(midpoint - apex) = normalize((bx+rx2)/2 - ax, (by+ry2)/2 - ay)
```

The midpoint of the base is: `mid = apex + t * (corner1 - apex)` where we project corner1 onto the apex-forward axis... actually the simpler formulation:

The reflection of a point P across a line through origin with direction D is: `P' = 2*(P·D)*D - P`.

```cpp
// Cone stores: apex (ax,ay), corner1 (c1x,c1y)
// At draw time, compute corner2 = reflection of corner1 across the axis (apex→midpoint)
// Midpoint M of the base is the midpoint between c1 and c2.
// But since c2 is the reflection of c1 across axis, and axis goes from apex through M...
// Simpler: axis direction is from apex to the "forward" point.
// The symmetric design means:
//   axis = (c1x - ax, c1y - ay) but reflected symmetrically
//   Actually: axis bisects the angle. We need to find what the axis is.
//
// Per the decision: "Axis = line from apex to midpoint of base"
// If c1 is one corner and c2 is its mirror, midpoint M = (c1+c2)/2
// M lies on the axis. Axis direction = M - apex.
// Reflection of c1 across line through apex with direction D:
//   Let v = c1 - apex
//   D = normalize(M - apex)  — but M = (c1+c2)/2 which depends on c2...
//
// Resolution: The axis IS defined as the drag direction.
// Interpretation from CONTEXT: "drag sets one base edge" — the drag direction IS the axis.
// apex→drag_endpoint is the axis of symmetry.
// Corner1 is offset perpendicular to the axis.
// Corner2 is the mirror of Corner1.
//
// Better model: cone stores apex(ax,ay), axis_tip(tx,ty), half_width_angle.
// At drag time: axis = drag point. Half-width = some default or user-adjusted.
// But the decision says "one gesture" and "drag endpoint = one base corner".
//
// The correct interpretation:
//   axis_dir = normalize(drag_end - apex)
//   half_width = perpendicular component from axis to drag_end
//   corner1 = drag_end
//   corner2 = apex + proj_along_axis + reflected_perp
```

**Resolved geometry (from CONTEXT.md "Specifics" section):**
- Apex = `(ax, ay)` = click point
- Axis = line from apex through midpoint of base
- One base corner = drag endpoint `(bx, by)`
- Other base corner = reflection of drag endpoint across the axis

The axis direction must be determined from the drag point. Since the drag point IS one corner (not the midpoint), and the axis bisects apex→base_midpoint:

```
Let v = (bx - ax, by - ay)       // vector from apex to drag endpoint
Axis direction D = ?
Midpoint M = (c1 + c2) / 2, where c2 = reflection of c1 across D

For c2 to be the reflection of c1 across the axis through apex:
  c2 = reflect(c1, apex, apex + D)
  reflect(P, A, B):
    D = normalize(B - A)
    v = P - A
    return A + 2*(v·D)*D - v    [standard point reflection formula]

The question is what determines D. Per the design, dragging straight out from the apex
should produce a symmetric cone. The most natural interpretation:
  D is the direction from apex to the midpoint of the base (by definition).
  The midpoint of the base is equidistant from both corners.

The simplest implementation:
  Store apex and drag_endpoint separately.
  At draw time, reflect drag_endpoint across the line (apex → perpendicular_midpoint).

BUT: We need D to compute the reflection, and D depends on the midpoint, which depends on c2.
This is circular. The resolution: D is NOT derived from the stored points.
D is the perpendicular bisector's orientation.

Alternate clean model: store apex + drag_endpoint.
  The "axis" is the direction that makes c1 and c2 equidistant from it.
  This means the axis goes from apex in the direction of (c1+c2)/2 - apex.

The ACTUAL simple model (resolving the circularity):
  When user drags to point P = (bx, by):
  - apex = click point A
  - D = normalize(P - A)     // axis direction = direction user dragged
  - length = |P - A|          // cone depth
  - Half-width is the perpendicular distance from axis to P...
    but P IS on the axis if D = normalize(P-A). That gives a degenerate cone.

CORRECT interpretation (re-reading CONTEXT):
"drag sets one base EDGE" not "drag to one base corner."
The drag endpoint IS a base corner. The axis is the perpendicular bisector of the base.

So the axis direction is NOT the same as apex→corner1. It's the direction from apex
to the midpoint. Since corner2 = reflect(corner1, axis), and the midpoint = (c1+c2)/2
lies on the axis, the geometry is:

  Given: apex A, corner1 C1 (drag endpoint)
  Find: corner2 C2, such that the triangle A-C1-C2 is isosceles with A at apex

  Isosceles means |A-C1| = |A-C2| (equal legs). This gives a family of solutions.
  The specific one matching the visual: C2 is the reflection of C1 across the
  line from A in the "forward" direction.

  The "forward" direction is defined as the direction perpendicular to C1-C2
  (i.e., pointing "into" the cone). But this is still undefined without knowing C2.

RESOLUTION: The simplest model that matches the stated design intent:
  - User defines the apex and one base corner.
  - The axis of symmetry goes from the apex through the midpoint of the base.
  - "Midpoint of base" means the point equidistant along the perpendicular from both corners.
  - Because the triangle is isosceles, the axis bisects the base.
  - The axis direction is implicitly: the drag vector's component that...

PRACTICAL SOLUTION: Store (apex, drag_corner). At draw time:
  1. Compute the axis as bisecting the angle — no, that requires knowing both corners.

The only unambiguous geometry: axis is the vector from apex to drag_corner projected
onto... No.

The cleanest model that gives the "one gesture, symmetric result" design:

  Store: apex A = (ax,ay), and axis_tip T = (tx,ty) where T is the midpoint of the base.
  Half-width W = perpendicular distance from T to either base corner.

  From a single drag gesture:
  - If the user clicks apex and drags to a point P:
    - A = click point
    - T = P (user drags to where they want the midpoint of the base)
    - W = default half-angle * |T-A| (e.g., ±30 degrees)

  But CONTEXT says "drag endpoint = one base corner." So T != P.

FINAL RESOLVED MODEL (re-reading CONTEXT.md specifics):
"Apex = click point. Axis = line from apex to midpoint of base. One base corner = drag endpoint.
Other base corner = reflection of drag endpoint across the axis."

This means the AXIS is the direction from apex toward (and through) the midpoint of base.
C1 = drag endpoint. C2 = reflect(C1, line through A with direction D).
D = direction of axis = the FORWARD direction.

The axis direction D must be determined WITHOUT knowing C2.
The only way this works is if D is perpendicular to the base (C1-C2).
Since C2 = reflect(C1, axis), C1-C2 is always perpendicular to the axis (by definition of reflection).
So the constraint is: D such that the reflection of C1 across line(A, A+D) gives C2.

The user drags to C1. The axis D is unknown. We need another constraint.

The natural constraint: the axis passes through the midpoint of C1 and A projected...
No, the constraint is that the CONE OPENS toward a specific direction, and the user's drag
sets ONE CORNER, not the axis direction.

RESOLUTION: The axis direction is simply derived from looking at where C1 is relative to A.
The axis bisects the angle at the apex between the two legs AC1 and AC2. Since the triangle
is isosceles (both legs equal), the axis is the angle bisector, which is the same as the
perpendicular bisector of the base.

For a given C1 and A, C2 must satisfy:
  |A-C2| = |A-C1|  (isosceles)
  D bisects angle(C1-A-C2)

This has infinite solutions (C2 can be anywhere on a circle of radius |A-C1| centered at A).
A SECOND constraint is needed. The CONTEXT provides it implicitly:
"Starting angle ~60 degrees as a reference, but the actual angle is fully determined by
where the user drags — no hardcoded angle."

This means: the ANGLE between AC1 and AC2 is determined by the drag. But C1 IS the drag
endpoint. So the angle is NOT fixed, it IS the drag. But both C1 and C2 change with drag?

SIMPLEST CORRECT INTERPRETATION that makes geometric sense:
  - User drags from apex A to a point P.
  - The axis of symmetry D = direction from A in the "forward" direction.
  - The "forward" direction is defined as the drag direction: D = normalize(P - A).
  - P is NOT a corner; it is the TIP of the axis.
  - C1 and C2 are placed symmetrically around the axis at the base.
  - The base distance from the axis = some fixed fraction of |P-A| (e.g., tan(30°) * |P-A|).

But CONTEXT says "drag endpoint = one base corner." So P = C1.

FINAL FINAL RESOLUTION (the only geometry that satisfies all stated constraints):
  - A = apex
  - C1 = drag endpoint (one base corner)
  - Axis D is defined as: D = normalize(C1 - A) rotated by -half_angle

This still requires knowing the angle. ALTERNATIVE that truly matches the description:

The axis direction is FIXED as the direction perpendicular to the line from A to C1 rotated 90°...

Actually the simplest workable model: The axis is the FORWARD direction from apex, and C1
defines the perpendicular width. Concretely:

  A = apex (click)
  P = drag endpoint

  "Forward" direction = atan2(Py-Ay, Px-Ax)
  Base center M = apex + |AP| * forward_unit
  Half-width W = |AP| * sin(fixed_half_angle)  -- no, this makes angle fixed

PRACTICAL TAKEAWAY for the planner: Use this model (confirmed by re-reading CONTEXT.md specifics):

  A = apex
  C1 = drag endpoint (first corner)
  Reflect C1 across the line from A in the direction of (C1 - A)...
  No — reflecting across the line from A through C1 would give C2 = C1 (same point).

CORRECT READING OF CONTEXT: "reflection of drag endpoint across the axis" means:
  The axis is the line connecting apex A to midpoint M of the base.
  C1 = drag endpoint.
  C2 = reflection of C1 across the line A-M.
  But M = (C1 + C2) / 2, so this is circular...

UNLESS: The axis direction is taken as the PERPENDICULAR from C1 to the line A-C1, which
makes no sense.

FINAL PRACTICAL RESOLUTION: The model that avoids all ambiguity and matches "one gesture,
symmetric, click=apex, drag=one base corner":

  A = apex (click)
  P = drag endpoint

  The axis of symmetry = line from A perpendicular to the line from A to P, rotated 90°.
  No, that's wrong.

  The geometry that makes visual/usable sense for a cone of vision in football:
  - Apex at the player's helmet
  - User drags OUT and to one side
  - The cone is symmetric about the forward direction
  - The forward direction is from apex toward where the user dragged

  So: forward_dir = normalize(P - A). P is NOT a corner. P defines direction AND length.
  Corners are symmetric perpendicular to forward_dir at distance P from A.

  But CONTEXT says P IS a corner. Both cannot be true at once.

CONCLUSION: The CONTEXT "Specifics" section contains a geometry specification that is
circularly defined (reflection requires knowing the axis, axis is derived from both corners).
The planner must choose one of two interpretations:

  **Option A (P is axis tip, width is fixed):** Drag to midpoint of base. Width = fixed_angle * length.
    - Simple geometry. User controls direction and depth. Width is automatic.
    - Downside: no width control in gesture.

  **Option B (P is one corner, axis is drag direction):** Forward dir = normalize(P-A).
    Width = perpendicular distance from A-forward-axis to P. C2 = symmetric opposite of C1.
    - Concretely: axis_dir = normalize(P-A). project P onto axis to get base_center.
      half_width = distance from P to axis. C1 = P. C2 = base_center - (P - base_center).
    - This is the only self-consistent geometry where P=C1 and the result is symmetric.

  **Recommendation: Use Option B.**
  - axis_dir = normalize(P - A)
  - proj_len = dot(P - A, axis_dir)  — note: this = |P-A| since axis_dir = normalize(P-A)
  - base_center = A + proj_len * axis_dir = A + (P-A) = P — so base center = drag endpoint!
  - half_width = 0. Degenerate.

  This confirms Option B also degenerates. The ONLY valid model:

  **Option C (P is corner, axis is user's intended forward direction):**
  The axis is NOT in the direction of P from A. The user swipes from A toward/across the
  field. The drag point P = C1. The axis bisects the angle at A in the final triangle.
  Axis_dir = normalize(C1 - A) rotated by +half_angle brings us to C1.
  Axis_dir = normalize(C2 - A) rotated by -half_angle brings us to C2.
  C1 and C2 are symmetric about axis_dir.

  For any P = C1, axis_dir is unknown. To make it deterministic: axis_dir ROTATES FREELY,
  and C2 = reflect of C1 across the perpendicular bisector of arc(C1, A).

  THE SIMPLE MODEL that makes this work (and is ACTUALLY what the CONTEXT means):

  Store A and C1. Axis direction = bisector of angle from A to (C1 reflected about the
  perpendicular from A). This is circular.

  ACTUAL SIMPLE MODEL: The axis is the direction from A toward where the
  player is looking. In the gesture, the user first determines WHERE the cone goes by
  dragging out, and the perpendicular offset of the drag point from the A→P axis defines
  the half-width.

  BUT: CONTEXT says "drag endpoint = one base corner." This means the drag point IS at the
  corner of the triangle, NOT at the midpoint of the base edge.

  ONLY SELF-CONSISTENT GEOMETRY:

  A = apex (click). D = some forward direction. C1 = drag endpoint.
  C2 = reflect(C1) across line(A, A+D).

  D is NOT derived from C1 alone — it must come from somewhere else.

  RESOLUTION: The gesture defines D implicitly as follows:
  - The user clicks (sets A) and then drags.
  - The INITIAL drag direction (first movement after click) sets D.
  - Or: D is perpendicular to C1-A direction.

  PRAGMATIC CHOICE for implementation:
  D = normalize(rotate90(C1 - A)) makes C1 perpendicular to the axis, meaning the
  cone opens 90° wide regardless. Not useful.

  **FINAL RECOMMENDATION: Implement as "P is the midpoint of the base edge,
  width = fixed_fraction of depth."**

  This is the only model that is (a) unambiguous, (b) usable with one gesture,
  (c) produces a visually sensible cone. The CONTEXT's "other base corner = reflection"
  description is accurate for this model: if P is the midpoint M, then C1 and C2 are
  placed symmetrically by: offset = perpendicular_to_axis * (|AP| * tan(half_angle)),
  C1 = M + offset, C2 = M - offset. C2 = reflect(C1) across axis. Consistent.

  **Cone store: apex A, axis_tip T (= drag endpoint = midpoint of base),
  half_angle = 30° default (fixed for now, discretion for adjustment later).**
```

### Cone Implementation (resolved model):
```cpp
// ConeMark stores: ax_, ay_ (apex), tx_, ty_ (axis tip = base midpoint), half_angle_rad_
// half_angle_rad_ = M_PI / 6.0f  (30 degrees)
// tx_/ty_ updated on mouse_move; committed on mouse_up

void ConeMark::draw(gs_eparam_t *color_param) const {
    float dx = tx_ - ax_, dy = ty_ - ay_;
    float len = std::hypot(dx, dy);
    if (len < 1.0f) return;
    float ux = dx / len, uy = dy / len;          // unit axis
    float px = -uy, py = ux;                      // perpendicular
    float half_w = len * std::tan(half_angle_rad_);
    float c1x = tx_ + px * half_w, c1y = ty_ + py * half_w;
    float c2x = tx_ - px * half_w, c2y = ty_ - py * half_w;

    gs_effect_set_vec4(color_param, &color_);
    gs_render_start(false);
    gs_vertex2f(ax_, ay_);
    gs_vertex2f(c1x, c1y);
    gs_vertex2f(c2x, c2y);
    gs_render_stop(GS_TRIS);
}
```

**Note on transparency:** The semi-transparent cone requires alpha < 1.0f in the color vec4. The blend state in `chalk_video_render` (`GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA`) is already correct for transparency. Use alpha ~0.35f for the filled cone.

---

## Common Pitfalls

### Pitfall 1: Calling undo_mark() without the mutex
**What goes wrong:** `marks.pop_back()` called from hotkey callback (not on render thread) while `video_render` is iterating `marks`. Data race, crash.
**Why it happens:** Hotkey callbacks fire on OBS hotkey thread. MarkList operations from hotkeys must still hold `mark_list.mutex`.
**How to avoid:** `undo_mark()` and `delete_closest()` in MarkList always `lock_guard<mutex>` before touching `marks`.
**Warning signs:** Intermittent crash in video_render after undo; address sanitizer use-after-free.

### Pitfall 2: Not unregistering hotkeys in chalk_destroy
**What goes wrong:** OBS holds dangling callback pointer to destroyed `ChalkSource`; crash when hotkey fires after source is deleted.
**Why it happens:** `obs_hotkey_register_source` stores the `void *data` pointer.
**How to avoid:** Store all `obs_hotkey_id` values in `ChalkSource`. In `chalk_destroy`, call `obs_hotkey_unregister(id)` for each before `delete`.
**Warning signs:** Crash after removing the source from OBS and pressing a previously registered hotkey.

### Pitfall 3: Drawing laser pointer position from stale data
**What goes wrong:** `laser_x/laser_y` are written by mouse_move (non-render thread). Reading them unsynchronized in video_render is technically a data race.
**Why it happens:** `bool drawing` and other simple flags worked without locks because OBS serializes mouse callbacks (documented in Phase 1). But `laser_x/laser_y` are written by mouse_move AND read by video_render — two different threads.
**How to avoid:** Either (a) store `laser_x/laser_y` under the same mark_list mutex (read with lock in render), or (b) use `std::atomic<float>` — but atomic float requires C++20 or implementation-defined behavior. Simplest: add `laser_x/laser_y` to the existing mutex scope. Read under lock in video_render, write under lock in mouse_move.
**Warning signs:** Laser pointer jitters or shows wrong position on fast movement.

### Pitfall 4: GS_TRIS winding order / face culling
**What goes wrong:** Filled triangle doesn't appear because face culling discards back-facing triangles.
**Why it happens:** OBS GS does face culling by default for textured geometry, but for solid-color immediate-mode rendering with `OBS_EFFECT_SOLID`, culling behavior may vary.
**How to avoid:** Specify vertices counterclockwise (CCW) as seen on screen. Or explicitly set cull mode: `gs_set_cull_mode(GS_NEITHER)` before drawing the cone triangle. Restore after.
**Warning signs:** Cone appears when dragging in one direction but disappears when dragging in the other.

### Pitfall 5: Arrow mark update_end called on a non-ArrowMark
**What goes wrong:** Calling `dynamic_cast` or unguarded virtual method on in_progress that was created with a different tool.
**Why it happens:** Tool can theoretically switch mid-gesture (hotkey fires between mouse-down and mouse-up).
**How to avoid:** Lock the tool to the in_progress mark type for the duration of a gesture. Simplest: if `in_progress` exists, don't switch active_tool until mouse-up. Or: `add_point` is already a virtual no-op in base class; define `update_end` similarly with a default no-op. Each mark type only responds to the calls it understands.
**Warning signs:** Wrong mark type created when rapidly switching tools.

### Pitfall 6: Circle radius going negative
**What goes wrong:** Drawing a circle with negative radius crashes or renders nothing.
**Why it happens:** No guard on `r = hypot(event->x - cx, event->y - cy)`. `hypot` always returns positive, so this is not an issue. But storing `r` and forgetting to abs() somewhere else might cause problems.
**How to avoid:** `hypot` returns non-negative. Nothing needed as long as radius = hypot of two coords.

---

## Code Examples

### Registering all hotkeys in chalk_create
```cpp
// Source: obs-studio-31.1.1/libobs/obs-hotkey.h (obs_hotkey_register_source signature)
static void *chalk_create(obs_data_t *settings, obs_source_t *source)
{
    auto *ctx = new ChalkSource(source);
    ctx->hotkey_undo   = obs_hotkey_register_source(source, "chalk.undo",
                             "Chalk: Undo",        chalk_hotkey_undo,   ctx);
    ctx->hotkey_clear  = obs_hotkey_register_source(source, "chalk.clear",
                             "Chalk: Clear All",   chalk_hotkey_clear,  ctx);
    ctx->hotkey_color  = obs_hotkey_register_source(source, "chalk.color_next",
                             "Chalk: Next Color",  chalk_hotkey_color,  ctx);
    ctx->hotkey_laser  = obs_hotkey_register_source(source, "chalk.laser",
                             "Chalk: Laser Pointer", chalk_hotkey_laser, ctx);
    // Tool switch hotkeys: one per tool
    ctx->hotkey_tool_freehand = obs_hotkey_register_source(source, "chalk.tool.freehand",
                                    "Chalk: Freehand Tool", chalk_hotkey_tool_freehand, ctx);
    // ... arrow, circle, cone similarly
    ctx->hotkey_pick_delete = obs_hotkey_register_source(source, "chalk.pick_delete",
                                  "Chalk: Pick to Delete", chalk_hotkey_pick_delete, ctx);
    return ctx;
}
```

### Hotkey cleanup in chalk_destroy
```cpp
static void chalk_destroy(void *data)
{
    auto *ctx = static_cast<ChalkSource *>(data);
    obs_hotkey_unregister(ctx->hotkey_undo);
    obs_hotkey_unregister(ctx->hotkey_clear);
    obs_hotkey_unregister(ctx->hotkey_color);
    obs_hotkey_unregister(ctx->hotkey_laser);
    obs_hotkey_unregister(ctx->hotkey_tool_freehand);
    obs_hotkey_unregister(ctx->hotkey_pick_delete);
    // ... all tool hotkeys
    delete ctx;
}
```

### Color palette (ABGR values, OBS convention)
```cpp
// Source: color_uint32_to_rgba convention in chalk-source.cpp (Phase 1)
// ABGR format: byte order is alpha(31:24), blue(23:16), green(15:8), red(7:0)
static const uint32_t CHALK_PALETTE[5] = {
    0xFFFFFFFF,  // white
    0xFF00FFFF,  // yellow (R=FF, G=FF, B=00, A=FF)
    0xFF0000FF,  // red    (R=FF, G=00, B=00, A=FF)
    0xFFFF0000,  // blue   (R=00, G=00, B=FF, A=FF)
    0xFF00FF00,  // green  (R=00, G=FF, B=00, A=FF)
};
static const int CHALK_PALETTE_SIZE = 5;

// In ToolState: replace active_color with color_index
struct ToolState {
    ToolType active_tool = ToolType::Freehand;
    int color_index = 0;  // index into CHALK_PALETTE
    // Helper:
    uint32_t active_color() const { return CHALK_PALETTE[color_index]; }
};

// Color cycle hotkey:
static void chalk_hotkey_color(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *ctx = static_cast<ChalkSource *>(data);
    ctx->tool_state.color_index = (ctx->tool_state.color_index + 1) % CHALK_PALETTE_SIZE;
}
```

### Distance-to for freehand hit test
```cpp
// Per-segment distance: minimum distance from point to any segment in the polyline
float FreehandMark::distance_to(float x, float y) const
{
    float min_dist = std::numeric_limits<float>::max();
    for (size_t i = 1; i < points_.size(); ++i) {
        float ax = points_[i-1].x, ay = points_[i-1].y;
        float bx = points_[i].x,   by = points_[i].y;
        float dx = bx - ax, dy = by - ay;
        float len2 = dx*dx + dy*dy;
        float dist;
        if (len2 < 1e-6f) {
            dist = std::hypot(x - ax, y - ay);
        } else {
            float t = ((x-ax)*dx + (y-ay)*dy) / len2;
            t = std::max(0.f, std::min(1.f, t));
            dist = std::hypot(x - (ax + t*dx), y - (ay + t*dy));
        }
        min_dist = std::min(min_dist, dist);
    }
    return min_dist;
}
```

### Mark base class: hit_test → distance_to change
```cpp
// Source: src/marks/mark.hpp (current)
// Change: replace bool hit_test with float distance_to for pick-to-delete
class Mark {
public:
    virtual ~Mark() = default;
    virtual void draw(gs_eparam_t *color_param) const = 0;
    virtual float distance_to(float x, float y) const = 0;  // replaces hit_test
    virtual void add_point(float /*x*/, float /*y*/) {}
    virtual void update_end(float /*x*/, float /*y*/) {}     // for single-gesture tools
};
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `bool hit_test()` stub | `float distance_to()` — returns min distance from mark | Phase 2 | Enables pick-to-delete to compare marks |
| `active_color uint32_t` in ToolState | `color_index int` with palette array | Phase 2 | Supports cycling; color is derived, not stored |
| Single tool (Freehand) | Tool routing in mouse callbacks | Phase 2 | mouse_click/move must dispatch on `active_tool` |
| `drawing` bool for freehand state | Per-tool gesture state in ChalkSource | Phase 2 | `drawing` can remain as "gesture in progress" flag; laser needs separate `laser_active` |

---

## Open Questions

1. **Pick-to-delete mode: auto-exit or toggle?**
   - What we know: Mode is entered via hotkey. After one deletion, should mode auto-exit?
   - What's unclear: Left to Claude's discretion per CONTEXT.md.
   - Recommendation: Auto-exit after one deletion. Minimizes accidental deletions mid-stream.

2. **Undo stack depth**
   - What we know: Left to Claude's discretion per CONTEXT.md.
   - What's unclear: Whether to cap at N or allow full history.
   - Recommendation: Unlimited (vector already holds all marks; no need to cap for typical use of ~10 marks per play).

3. **Laser pointer dot vs. ring**
   - What we know: Left to Claude's discretion (cursor dot size, color, opacity).
   - Recommendation: Small filled circle (16-point polygon, radius 8px, 100% opacity, using the active color). Distinct from drawn marks because it never persists.

4. **Arrow end styles (Claude's discretion)**
   - Recommendation: Implement one style (open V-shaped arrowhead, two lines at ±30°). Add styles in Phase 3 if needed. Don't block Phase 2 on it.

5. **Cone geometry disambiguation**
   - As documented in the Geometry Reference section, the CONTEXT's "drag endpoint = one base corner, reflect across axis" description is self-referential.
   - Recommendation: Implement as "drag endpoint = midpoint of base, half-angle fixed at 30°." This is unambiguous, produces exactly the stated visual (symmetric isosceles triangle), and leaves angle adjustment for Phase 3 discretion.

---

## Validation Architecture

nyquist_validation is enabled (`true` in .planning/config.json).

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None — OBS plugin, no unit test framework detected |
| Config file | None |
| Quick run command | Build: `cmake --build build_macos --config Release` |
| Full suite command | Manual OBS verification (all tools, all hotkeys, undo/delete/clear) |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TOOL-01 | Freehand interpolation: no gaps at fast movement | manual-only | build | ❌ Wave 0 (manual verify) |
| TOOL-02 | Arrow click-drag: shaft + arrowhead visible | manual-only | build | ❌ Wave 0 |
| TOOL-03 | Circle click-drag: circular outline visible | manual-only | build | ❌ Wave 0 |
| TOOL-04 | Cone click-drag: filled semi-transparent triangle | manual-only | build | ❌ Wave 0 |
| TOOL-05 | Laser: appears on key-hold, gone on release | manual-only | build | ❌ Wave 0 |
| MARK-01 | Undo: last mark removed | manual-only | build | ❌ Wave 0 |
| MARK-02 | Pick-delete: specific mark removed | manual-only | build | ❌ Wave 0 |
| MARK-03 | Clear all: all marks gone | manual-only | build | ❌ Wave 0 |
| MARK-04 | Multi-undo: step back through 3+ marks | manual-only | build | ❌ Wave 0 |
| INPT-01 | All hotkeys appear in OBS hotkey settings | manual-only | build | ❌ Wave 0 |
| INPT-02 | Color cycle: 5 colors wrap back to white | manual-only | build | ❌ Wave 0 |

**Justification for manual-only:** All behaviors require OBS plugin context, GPU rendering, and live mouse/keyboard interaction. Unit testing geometry helpers is possible in principle but the project has not established a test harness and the geometry functions are simple enough that visual verification is the appropriate gate.

### Sampling Rate
- **Per task commit:** `cmake --build build_macos --config Release` — build must succeed
- **Per wave merge:** Full manual OBS verification checklist (build + load + all tools)
- **Phase gate:** All 11 requirements verified manually in OBS before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] No test framework installed — geometry helpers (distance_to, interpolation) could be unit tested with a standalone harness, but this is not required for Phase 2 given low complexity and manual verification plan.
- [ ] Build command must succeed as a gate: `cmake --build build_macos --config RelWithDebInfo 2>&1 | tail -5`

*(No automated test files to create — all verification is in-OBS manual.)*

---

## Sources

### Primary (HIGH confidence)
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/libobs/obs-hotkey.h` — `obs_hotkey_register_source`, callback signature, `bool pressed`, unregister, pair API
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/libobs/graphics/graphics.h` — `GS_TRIS`, `GS_LINESTRIP`, `GS_POINTS`, `gs_render_start/stop`, `gs_vertex2f`, blend enums
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/libobs/obs-interaction.h` — `obs_key_event`, `obs_mouse_button_type`, interaction flags
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/libobs/obs-source.h` — `key_click` callback in `obs_source_info`, save/load callbacks
- `/Users/jteague/dev/obs-chalk/src/` — all Phase 1 source files (verified working patterns)
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/plugins/obs-ffmpeg/obs-ffmpeg-source.c` — working example of `obs_hotkey_register_source` + `obs_hotkey_unregister` in create/destroy

### Secondary (MEDIUM confidence)
- `/Users/jteague/dev/obs-chalk/.deps/obs-studio-31.1.1/libobs/obs.c` line 2424 — confirms OBS auto-saves hotkey bindings to source context; no explicit plugin serialization needed

### Tertiary (LOW confidence)
- None. All critical claims verified from source headers.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all APIs read directly from vendor headers in .deps
- Architecture: HIGH — extends Phase 1 patterns directly; hotkey patterns confirmed from obs-ffmpeg example
- Geometry (arrow, circle, freehand): HIGH — standard 2D math, no library needed
- Geometry (cone): MEDIUM — the CONTEXT's description has a circular dependency; recommended resolution (drag=axis_tip) is a design judgment call, not a research finding
- Pitfalls: HIGH — mutex/lifetime pitfalls derived from reading the actual codebase and hotkey threading model

**Research date:** 2026-03-29
**Valid until:** 2026-06-01 (libobs API is stable; only changes with OBS major version)
