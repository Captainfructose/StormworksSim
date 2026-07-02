#include <QApplication>
#include <QFont>
#include "gui/MainWindow.h"

// ─────────────────────────────────────────────────────────────────────────────
// main — create the Qt application and launch the main window
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Set a consistent base font so Windows display scaling doesn't cause
    // overlapping text in menus and dropdowns
    QFont appFont("Segoe UI", 9);
    app.setFont(appFont);

    // Application metadata (used by QSettings etc.)
    app.setApplicationName("StormworksSim");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("StaleyEngineering");

    // ── Global dark stylesheet ────────────────────────────────────────────────
    // Applied to the QApplication (not just MainWindow) so every top-level
    // widget in the app — including independent dialogs like the per-node
    // settings dialog — picks up the same styling automatically.
    app.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #1e1e1e;
            color: #dddddd;
        }
        /* Overall SpinBox container */
        QDoubleSpinBox, QSpinBox {
            border: 1px solid #999999;
            background-color: #424242;
            border-radius: 4px;
            padding-right: 20px; /* Leave room for the buttons */
            min-height: 24px;
            color: white; /* Ensure text is visible on dark background */
        }
        /* Focused state */
        QDoubleSpinBox:focus, QSpinBox:focus {
            border: 1px solid #db8d34;
        }
        /* Style and position the Up Button specifically */
        QDoubleSpinBox::up-button, QSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right; /* Top right anchor */
            width: 16px;
            height: 12px; /* Half of your min-height */
            border-left: 1px solid #999999;
            border-bottom: 1px solid #999999;
            background-color: #424242;
        }
        /* Style and position the Down Button specifically */
        QDoubleSpinBox::down-button, QSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right; /* Bottom right anchor */
            width: 16px;
            height: 12px; /* Half of your min-height */
            border-left: 1px solid #999999;
            background-color: #424242;
        }
        /* Hover effects for buttons */
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover, QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #b6b6b6;
            border: 1px solid #db8d34;
        }
        /* Pressed effects for buttons */
        QDoubleSpinBox::up-button:pressed, QDoubleSpinBox::down-button:pressed, QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
            background-color: #d4d4d4;
        }
        /* Customizing the arrows */
        QDoubleSpinBox::up-arrow, QSpinBox::up-arrow {
            image: url(:/icons/up_arrow.png);
            width: 5px;
            height: 5px;
        }
        QDoubleSpinBox::down-arrow, QSpinBox::down-arrow {
            image: url(:/icons/down_arrow.png);
            width: 5px;
            height: 5px;
        }
        QMenuBar {
            background-color: #252525;
            color: #cccccc;
        }
        QMenuBar::item:selected { background: #3a3a3a; }
        QMenu {
            background-color: #252525;
            color: #cccccc;
            border: 1px solid #444;
        }
        QMenu::item {
            padding: 4px 24px 4px 24px;
        }
        QMenu::item:selected { background: #3a3a3a; }
        QStatusBar {
            background-color: #252525;
            color: #888888;
        }
        QSplitter::handle { background: #333; }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #1a1a1a;
            width: 10px; height: 10px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #444;
            border-radius: 4px;
        }
    )");

    MainWindow window;
    window.show();

    return app.exec();
}
