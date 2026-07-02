#include "MainWindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QInputDialog>
#include <QHBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      mc_(std::make_unique<Microcontroller>())
{
    setWindowTitle("Stormworks MC Simulator");
    resize(1200, 750);

    // Note: the global dark stylesheet (including spinbox button styling)
    // is applied once at the QApplication level in main.cpp, so it cascades
    // to every top-level widget — including independent dialogs — not just
    // this window and its children.

    // ── Central splitter ──────────────────────────────────────────────────────
    // Left: node graph canvas   Right: I/O panel (fixed-ish width)
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    graphView_ = new NodeGraphView(splitter);
    ioPanel_   = new IOPanel(splitter);

    splitter->addWidget(graphView_);
    splitter->addWidget(ioPanel_);
    splitter->setStretchFactor(0, 1); // graph gets all extra space
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({880, 300});

    setCentralWidget(splitter);

    // ── Simulation timer ──────────────────────────────────────────────────────
    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(100); // default 10 ticks/sec
    connect(tickTimer_, &QTimer::timeout, this, &MainWindow::onTimerTick);

    // ── Keep IOPanel in sync when nodes are added/removed in the graph ────────
    connect(graphView_, &NodeGraphView::nodeSetChanged, this, [this]() {
        ioPanel_->loadMicrocontroller(mc_.get());
    });

    // ── Initialize the view with an empty microcontroller so the user can
    // start adding nodes immediately without needing to load a file first ──────
    graphView_->loadMicrocontroller(mc_.get());
    ioPanel_->loadMicrocontroller(mc_.get());

    // ── UI chrome ─────────────────────────────────────────────────────────────
    setupMenuBar();
    setupStatusBar();
    updateStatus();
}

// ─────────────────────────────────────────────────────────────────────────────
// setupMenuBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setupMenuBar()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* openAct = fileMenu->addAction("&Open MC File…");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();

    QAction* saveAct = fileMenu->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSaveFile);

    QAction* saveAsAct = fileMenu->addAction("Save &As…");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveFileAs);

    fileMenu->addSeparator();

    QAction* quitAct = fileMenu->addAction("&Quit");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    // Simulation menu
    QMenu* simMenu = menuBar()->addMenu("&Simulation");

    QAction* stepAct = simMenu->addAction("Step One Tick");
    stepAct->setShortcut(Qt::Key_Space);
    connect(stepAct, &QAction::triggered, this, &MainWindow::onStepTick);

    QAction* runAct = simMenu->addAction("Start / Stop");
    runAct->setShortcut(Qt::Key_Return);
    connect(runAct, &QAction::triggered, this, &MainWindow::onStartStop);

    simMenu->addSeparator();

    QAction* rateAct = simMenu->addAction("Set Tick Rate…");
    connect(rateAct, &QAction::triggered, this, &MainWindow::onSetTickRate);
}

// ─────────────────────────────────────────────────────────────────────────────
// setupStatusBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setupStatusBar()
{
    statusLabel_ = new QLabel("No file loaded  |  Tick 0  |  Stopped");
    statusBar()->addWidget(statusLabel_);
}

// ─────────────────────────────────────────────────────────────────────────────
// onOpenFile
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onOpenFile()
{
    // Stop any running simulation before swapping files
    tickTimer_->stop();
    running_ = false;

    QString path = QFileDialog::getOpenFileName(
        this, "Open Stormworks Microcontroller", QString(),
        "Stormworks MC (*.xml);;All Files (*)");

    if (!path.isEmpty())
        loadFile(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFile
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::loadFile(const QString& path)
{
    QString errMsg;
    if (!mc_->loadFromFile(path, errMsg)) {
        QMessageBox::critical(this, "Load Error", errMsg);
        return;
    }

    // Rebuild visual representations from the newly loaded graph
    graphView_->loadMicrocontroller(mc_.get());
    ioPanel_->loadMicrocontroller(mc_.get());

    setWindowTitle(QString("Stormworks MC Simulator — %1")
                   .arg(QFileInfo(path).fileName()));
    updateStatus();
}

// ─────────────────────────────────────────────────────────────────────────────
// onSaveFile — write back to the currently loaded file path.
// Falls back to Save As if no file has been loaded yet.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onSaveFile()
{
    if (!mc_->hasFilePath()) {
        onSaveFileAs();
        return;
    }

    QString errMsg;
    if (!mc_->saveToFile(errMsg)) {
        QMessageBox::critical(this, "Save Error", errMsg);
        return;
    }

    statusLabel_->setText(QString("Saved to %1").arg(QFileInfo(mc_->currentFilePath()).fileName()));
}

// ─────────────────────────────────────────────────────────────────────────────
// onSaveFileAs — prompt for a new path and save there
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onSaveFileAs()
{
    QString startPath = mc_->hasFilePath() ? mc_->currentFilePath() : QString();

    QString path = QFileDialog::getSaveFileName(
        this, "Save Stormworks Microcontroller As", startPath,
        "Stormworks MC (*.xml);;All Files (*)");

    if (path.isEmpty()) return;

    QString errMsg;
    if (!mc_->saveToFileAs(path, errMsg)) {
        QMessageBox::critical(this, "Save Error", errMsg);
        return;
    }

    setWindowTitle(QString("Stormworks MC Simulator — %1")
                   .arg(QFileInfo(path).fileName()));
    statusLabel_->setText(QString("Saved to %1").arg(QFileInfo(path).fileName()));
}

// ─────────────────────────────────────────────────────────────────────────────
// doTick — advance simulation by one step and refresh UI
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::doTick()
{
    mc_->tick();
    graphView_->refreshNodes();
    ioPanel_->refresh();
    updateStatus();
}

// ─────────────────────────────────────────────────────────────────────────────
// onTimerTick — called by the QTimer for continuous simulation
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onTimerTick()
{
    doTick();
}

// ─────────────────────────────────────────────────────────────────────────────
// onStepTick — advance exactly one tick (Space key)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onStepTick()
{
    if (!running_) doTick();
}

// ─────────────────────────────────────────────────────────────────────────────
// onStartStop — toggle continuous simulation (Enter key)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onStartStop()
{
    running_ = !running_;
    if (running_)
        tickTimer_->start();
    else
        tickTimer_->stop();
    updateStatus();
}

// ─────────────────────────────────────────────────────────────────────────────
// onSetTickRate — let the user choose ticks per second
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onSetTickRate()
{
    bool ok = false;
    int tps = QInputDialog::getInt(this, "Set Tick Rate",
                                   "Ticks per second (1–60):",
                                   1000 / tickTimer_->interval(), // current
                                   1, 60, 1, &ok);
    if (ok) {
        tickTimer_->setInterval(1000 / tps);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateStatus
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateStatus()
{
    QString state = running_ ? "Running" : "Stopped";
    statusLabel_->setText(
        QString("Tick %1  |  %2  |  %3 tps")
            .arg(mc_->tickCount())
            .arg(state)
            .arg(1000 / tickTimer_->interval()));
}
