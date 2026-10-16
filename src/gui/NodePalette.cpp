#include "NodePalette.h"
#include "NodeCatalog.h"
#include "NodeGraphItem.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kTileW   = 80;
constexpr int kTileH   = 76;
constexpr int kSpacing = 8;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor — builds one section per category, one tile per catalog entry
// ─────────────────────────────────────────────────────────────────────────────
NodePalette::NodePalette(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("nodePalette");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "#nodePalette { background-color: rgba(28, 32, 36, 240);"
        "  border: 1px solid #555; border-radius: 8px; }");
    hide();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(8);

    search_ = new QLineEdit;
    search_->setPlaceholderText("Search nodes…   (Enter adds the first match, Esc closes)");
    search_->setClearButtonEnabled(true);
    outer->addWidget(search_);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet("QScrollArea { background: transparent; }");
    outer->addWidget(scroll_, 1);

    auto* content = new QWidget;
    content->setStyleSheet("background: transparent;");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);
    scroll_->setWidget(content);

    // One section per category, in catalog order
    QString currentCategory;
    int     sectionIndex = -1;
    for (const NodeCatalogEntry& entry : nodeCatalog()) {
        if (entry.category != currentCategory) {
            currentCategory = entry.category;

            auto* sectionWidget = new QWidget;
            sectionWidget->setStyleSheet("background: transparent;");
            auto* sectionLayout = new QVBoxLayout(sectionWidget);
            sectionLayout->setContentsMargins(0, 0, 0, 0);
            sectionLayout->setSpacing(4);

            auto* header = new QLabel(currentCategory.toUpper());
            header->setStyleSheet("background: transparent; color: #d08a3c;"
                                  " font-size: 11px; font-weight: bold;");
            sectionLayout->addWidget(header);

            // A rule under the title, the full width of the section
            auto* rule = new QFrame;
            rule->setFixedHeight(1);
            rule->setStyleSheet("background-color: #d08a3c; border: none;"); // same as the title
            sectionLayout->addWidget(rule);
            sectionLayout->addSpacing(2);

            auto* gridHolder = new QWidget;
            gridHolder->setStyleSheet("background: transparent;");
            auto* grid = new QGridLayout(gridHolder);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setSpacing(kSpacing);
            grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            sectionLayout->addWidget(gridHolder);

            contentLayout->addWidget(sectionWidget);
            sections_.push_back({ sectionWidget, grid });
            ++sectionIndex;
        }

        // The tile: a white card with the node's grey icon square and its name
        auto* tile = new QPushButton(content);
        tile->setFixedSize(kTileW, kTileH);
        tile->setToolTip(entry.label);
        tile->setCursor(Qt::PointingHandCursor);
        tile->setStyleSheet(
            "QPushButton { background-color: #f4f4f4; border: 1px solid #9a9a9a;"
            "  border-radius: 6px; }"
            "QPushButton:hover { background-color: #ffffff; border-color: #ffa020; }"
            "QPushButton:pressed { background-color: #e2e2e2; }");

        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(4, 6, 4, 4);
        tileLayout->setSpacing(3);

        auto* glyph = new QLabel(NodeGraphItem::iconGlyph(entry.type, entry.label));
        glyph->setAlignment(Qt::AlignCenter);
        glyph->setFixedHeight(28);
        glyph->setStyleSheet("background-color: #808080; color: white; border-radius: 4px;"
                             " font-size: 12px; font-weight: bold;");
        glyph->setAttribute(Qt::WA_TransparentForMouseEvents);
        tileLayout->addWidget(glyph);

        auto* name = new QLabel(entry.label);
        name->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        name->setWordWrap(true);
        name->setStyleSheet("background: transparent; color: #333333; font-size: 9px;");
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        tileLayout->addWidget(name, 1);

        const NodeType type = entry.type;
        connect(tile, &QPushButton::clicked, this, [this, type]() {
            closePalette();
            emit nodeChosen(type);
        });

        tiles_.push_back({ tile, type,
                           (entry.label + " " + entry.category).toLower(), sectionIndex });
    }
    contentLayout->addStretch(1);

    connect(search_, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
    connect(search_, &QLineEdit::returnPressed, this, [this]() { chooseFirstMatch(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// openPalette / closePalette
// ─────────────────────────────────────────────────────────────────────────────
void NodePalette::openPalette()
{
    search_->clear();
    applyFilter();
    show();
    raise();
    search_->setFocus();
    // The scroll area only gets its real width once shown, so flow the tiles
    // again after the layout has settled
    QTimer::singleShot(0, this, [this]() { if (isVisible()) applyFilter(); });
}

void NodePalette::closePalette()
{
    hide();
    // Hand focus back to whatever the palette covers
    if (parentWidget()) parentWidget()->setFocus();
}

// ─────────────────────────────────────────────────────────────────────────────
// applyFilter — show the tiles that match the search text and re-flow them
//
// A tile matches if every space-separated word in the search box appears in its
// "label category" text. Sections with no matching tile are hidden. Tiles are
// laid out left to right, wrapping to fit the panel's current width.
// ─────────────────────────────────────────────────────────────────────────────
void NodePalette::applyFilter()
{
    const QStringList words =
        search_->text().toLower().split(' ', Qt::SkipEmptyParts);

    const int available = scroll_->viewport()->width();
    const int columns   = std::max(1, (available + kSpacing) / (kTileW + kSpacing));

    // Take every tile out of its grid first, then re-add the visible ones
    for (Section& s : sections_)
        while (s.grid->count() > 0)
            delete s.grid->takeAt(0);   // removes the layout item, not the widget

    std::vector<int> shown(sections_.size(), 0);
    for (Tile& t : tiles_) {
        bool match = true;
        for (const QString& w : words)
            if (!t.searchText.contains(w)) { match = false; break; }

        t.button->setVisible(match);
        if (!match) continue;

        const int n = shown[t.section]++;
        sections_[t.section].grid->addWidget(t.button, n / columns, n % columns);
    }

    for (size_t i = 0; i < sections_.size(); ++i)
        sections_[i].widget->setVisible(shown[i] > 0);
}

void NodePalette::chooseFirstMatch()
{
    for (const Tile& t : tiles_) {
        if (t.button->isVisible()) {
            const NodeType type = t.type;
            closePalette();
            emit nodeChosen(type);
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Events
// ─────────────────────────────────────────────────────────────────────────────
void NodePalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        closePalette();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void NodePalette::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    if (isVisible()) applyFilter(); // re-wrap the tiles for the new width
}
