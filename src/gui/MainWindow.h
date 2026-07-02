#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <memory>

#include "../core/Microcontroller.h"
#include "NodeGraphView.h"
#include "IOPanel.h"

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
//
// Top-level application window.  Layout:
//
//   ┌─────────────────────────────────────────────┬──────────┐
//   │                                             │          │
//   │           NodeGraphView (canvas)            │ IOPanel  │
//   │                                             │ (right)  │
//   └─────────────────────────────────────────────┴──────────┘
//   │  status bar: tick counter + run state                   │
//
// Simulation is driven by a QTimer that fires at the target tick rate.
// The user can also step one tick at a time.
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // Menu actions
    void onOpenFile();
    void onSaveFile();
    void onSaveFileAs();
    void onStepTick();
    void onStartStop();
    void onSetTickRate();

    // Timer callback — fires once per simulated tick
    void onTimerTick();

private:
    // ── Simulation ────────────────────────────────────────────────────────────
    std::unique_ptr<Microcontroller> mc_;
    QTimer* tickTimer_;
    bool    running_ = false;

    // ── Widgets ───────────────────────────────────────────────────────────────
    NodeGraphView* graphView_;
    IOPanel*       ioPanel_;
    QLabel*        statusLabel_;  // lives in the status bar

    // ── Helpers ───────────────────────────────────────────────────────────────
    void setupMenuBar();
    void setupStatusBar();
    void loadFile(const QString& path);

    // Run one tick and refresh both panels
    void doTick();

    // Update the status bar text
    void updateStatus();
};
