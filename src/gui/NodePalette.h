#pragma once

#include <QFrame>
#include <vector>

#include "../core/Node.h"

class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;

// ─────────────────────────────────────────────────────────────────────────────
// NodePalette
//
// The panel that opens over the node graph when Tab is pressed, similar to the
// Stormworks editor's: every node type as an icon tile, grouped by category.
// A search box at the top filters the tiles as you type.
//
//   • Click a tile (or press Enter to take the first match) → nodeChosen()
//   • Escape closes the palette without choosing anything
//
// The palette is a child widget of the view it covers; the owner positions it,
// calls openPalette(), and listens for nodeChosen().
// ─────────────────────────────────────────────────────────────────────────────
class NodePalette : public QFrame {
    Q_OBJECT

public:
    explicit NodePalette(QWidget* parent = nullptr);

    void openPalette();   // clear the search, show, and focus the search box
    void closePalette();

signals:
    void nodeChosen(NodeType type);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Tile {
        QPushButton* button;
        NodeType     type;
        QString      searchText; // lower-case "label category" for filtering
        int          section;    // index into sections_
    };
    struct Section {
        QWidget*     widget;
        QGridLayout* grid;
    };

    QLineEdit*   search_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    std::vector<Section> sections_;
    std::vector<Tile>    tiles_;

    void applyFilter();   // show only matching tiles and re-flow them
    void chooseFirstMatch();
};
