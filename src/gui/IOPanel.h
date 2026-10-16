#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QFrame>
#include <unordered_map>
#include <vector>

#include "../core/Microcontroller.h"
#include "../core/nodes/IONodes.h"
#include "../core/nodes/CompositeNodes.h"

// ─────────────────────────────────────────────────────────────────────────────
// CompositeChannelValue — stores the current value of one composite channel
// so refresh() can update the display without rebuilding the whole panel
// ─────────────────────────────────────────────────────────────────────────────
struct CompositeChannelControl {
    int          nodeId;      // which CompositeInputNode this belongs to
    int          channelIdx;  // index into node->channels
    QPushButton* boolBtn  = nullptr; // set if channel is Bool
    QDoubleSpinBox* numSpin = nullptr; // set if channel is Number
    QSlider*     slider   = nullptr;
};

class IOPanel : public QWidget {
    Q_OBJECT

public:
    explicit IOPanel(QWidget* parent = nullptr);

    void loadMicrocontroller(Microcontroller* mc);
    void refresh();
    void clearPanel();

    // Re-reads every row's connector label from its node, without rebuilding the
    // panel (so the controls keep the values they have)
    void updateLabels();

signals:
    void inputChanged();

private slots:
    void onBoolToggled(int nodeId, bool value);
    void onNumberChanged(int nodeId, double value);
    // Composite channel value changed
    void onCompositeBoolChanged(int nodeId, int channelIdx, bool value);
    void onCompositeNumberChanged(int nodeId, int channelIdx, double value);

private:
    Microcontroller* mc_ = nullptr;

    QVBoxLayout*  mainLayout_;
    QScrollArea*  inputScroll_;
    QWidget*      inputContainer_;
    QVBoxLayout*  inputLayout_;

    QScrollArea*  outputScroll_;
    QWidget*      outputContainer_;
    QVBoxLayout*  outputLayout_;

    // Output display widgets
    std::unordered_map<int, QLabel*>      boolOutputLabels_;
    std::unordered_map<int, QLabel*>      numberOutputLabels_;

    // Input control references
    std::unordered_map<int, QPushButton*> boolInputButtons_;

    // Composite channel controls — one entry per channel per CompositeInputNode
    std::vector<CompositeChannelControl>  compositeControls_;

    // The title label of each row, by node id, with the text to show when the
    // node has no connector label of its own — used by updateLabels()
    struct RowLabel {
        QLabel* widget;
        QString fallback;
    };
    std::unordered_map<int, RowLabel> rowLabels_;

    // Creates a row title showing the node's label (or `fallback`) and registers
    // it so updateLabels() can refresh it later
    QLabel* makeRowLabel(Node* n, const QString& fallback);

    void buildInputRow(BoolInputNode*     n);
    void buildInputRow(NumberInputNode*   n);
    void buildCompositeInputBlock(CompositeInputNode* n);
    void buildOutputRow(BoolOutputNode*   n);
    void buildOutputRow(NumberOutputNode* n);

    QLabel* makeSectionHeader(const QString& title);
    QFrame* makeDivider();
};
