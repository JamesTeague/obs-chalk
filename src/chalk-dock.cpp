// chalk-dock.cpp
//
// QWidget dock panel that polls chalk state every 100ms and shows:
//   - Mode: ACTIVE / off
//   - Tool: Freehand / Arrow / Circle / Cone / Laser / Delete
//   - Color: swatch + name
//
// Thread context: QTimer fires on Qt main thread. chalk_find_source() and
// reading tool_state fields (active_tool, color_index) are safe from this
// thread — tool_state writes are serialized by OBS hotkey callbacks and
// atomic on x86/ARM for the 1-byte and 4-byte reads involved.
//
// OBS owns the widget after obs_frontend_add_dock_by_id — do NOT delete it.

#include "chalk-dock.hpp"
#include "chalk-mode.hpp"
#include "chalk-source.hpp"
#include "tool-state.hpp"

#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFont>

// ---------------------------------------------------------------------------
// Name helpers
// ---------------------------------------------------------------------------

static const char *tool_name(ToolType t)
{
    switch (t) {
        case ToolType::Freehand: return "Freehand";
        case ToolType::Arrow:    return "Arrow";
        case ToolType::Circle:   return "Circle";
        case ToolType::Cone:     return "Cone";
        case ToolType::Laser:    return "Laser";
        case ToolType::Delete:   return "Delete";
    }
    return "Unknown";
}

static const char *color_name(int index)
{
    static const char *names[] = { "White", "Yellow", "Red", "Blue", "Green" };
    if (index >= 0 && index < CHALK_PALETTE_SIZE)
        return names[index];
    return "Unknown";
}

// ---------------------------------------------------------------------------
// ChalkDock constructor
// ---------------------------------------------------------------------------

ChalkDock::ChalkDock(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(180);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Row 1: Mode
    {
        auto *row = new QHBoxLayout();
        auto *key = new QLabel("Mode:", this);
        mode_label_ = new QLabel("off", this);

        QFont bold = mode_label_->font();
        bold.setBold(true);
        mode_label_->setFont(bold);

        row->addWidget(key);
        row->addWidget(mode_label_);
        row->addStretch();
        layout->addLayout(row);
    }

    // Row 2: Tool
    {
        auto *row = new QHBoxLayout();
        auto *key = new QLabel("Tool:", this);
        tool_label_ = new QLabel("(no source)", this);

        row->addWidget(key);
        row->addWidget(tool_label_);
        row->addStretch();
        layout->addLayout(row);
    }

    // Row 3: Color swatch + name
    {
        auto *row = new QHBoxLayout();
        auto *key = new QLabel("Color:", this);

        color_swatch_ = new QLabel(this);
        color_swatch_->setFixedSize(20, 20);
        color_swatch_->setStyleSheet("background-color: #808080; border: 1px solid #444;");

        color_name_label_ = new QLabel("(no source)", this);

        row->addWidget(key);
        row->addWidget(color_swatch_);
        row->addWidget(color_name_label_);
        row->addStretch();
        layout->addLayout(row);
    }

    layout->addStretch();

    // Start polling timer
    auto *timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, &ChalkDock::refresh);
    timer->start();
}

// ---------------------------------------------------------------------------
// refresh() — called every 100ms on Qt main thread
// ---------------------------------------------------------------------------

void ChalkDock::refresh()
{
    // Update mode label
    bool active = chalk_mode_is_active();
    if (active) {
        mode_label_->setText("ACTIVE");
        mode_label_->setStyleSheet("color: #00cc44;");
    } else {
        mode_label_->setText("off");
        mode_label_->setStyleSheet("");
    }

    // Probe for a chalk source in the current scene
    ChalkSource *ctx = chalk_find_source();
    if (!ctx) {
        tool_label_->setText("(no source)");
        color_swatch_->setStyleSheet("background-color: #808080; border: 1px solid #444;");
        color_name_label_->setText("(no source)");
        return;
    }

    // Tool name
    tool_label_->setText(tool_name(ctx->tool_state.active_tool));

    // Color swatch and name
    uint32_t abgr = ctx->tool_state.active_color();
    // ABGR layout: bits [7:0]=R, [15:8]=G, [23:16]=B, [31:24]=A
    int r = static_cast<int>( abgr        & 0xFF);
    int g = static_cast<int>((abgr >>  8) & 0xFF);
    int b = static_cast<int>((abgr >> 16) & 0xFF);

    QString css = QString("background-color: rgb(%1,%2,%3); border: 1px solid #444;")
                      .arg(r).arg(g).arg(b);
    color_swatch_->setStyleSheet(css);

    color_name_label_->setText(color_name(ctx->tool_state.color_index));
}
