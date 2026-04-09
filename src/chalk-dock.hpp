#pragma once
// chalk-dock.hpp
//
// Read-only Qt dock panel showing current chalk mode state, active tool,
// and selected color. Polls state every 100ms via QTimer so coaches can
// see what the plugin is doing without pressing hotkeys to find out.

#include <QWidget>

class QLabel;
class QTimer;

class ChalkDock : public QWidget {
    Q_OBJECT
public:
    explicit ChalkDock(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QLabel *mode_label_       = nullptr;
    QLabel *tool_label_       = nullptr;
    QLabel *color_swatch_     = nullptr;
    QLabel *color_name_label_ = nullptr;
};
