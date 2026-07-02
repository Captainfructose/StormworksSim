#include "IOPanel.h"

#include <QGroupBox>
#include <QFrame>
#include <QSizePolicy>
#include <QComboBox>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
IOPanel::IOPanel(QWidget* parent)
    : QWidget(parent)
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(4);
    mainLayout_->setContentsMargins(6, 6, 6, 6);

    mainLayout_->addWidget(makeSectionHeader("INPUTS"));

    inputContainer_ = new QWidget;
    inputLayout_ = new QVBoxLayout(inputContainer_);
    inputLayout_->setSpacing(0);
    inputLayout_->setContentsMargins(0, 0, 0, 0);
    inputLayout_->addStretch();

    inputScroll_ = new QScrollArea;
    inputScroll_->setWidgetResizable(true);
    inputScroll_->setWidget(inputContainer_);
    inputScroll_->setMinimumHeight(120);
    inputScroll_->setFrameShape(QFrame::NoFrame);
    mainLayout_->addWidget(inputScroll_, 1);

    QFrame* divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #444;");
    mainLayout_->addWidget(divider);

    mainLayout_->addWidget(makeSectionHeader("OUTPUTS"));

    outputContainer_ = new QWidget;
    outputLayout_ = new QVBoxLayout(outputContainer_);
    outputLayout_->setSpacing(0);
    outputLayout_->setContentsMargins(0, 0, 0, 0);
    outputLayout_->addStretch();

    outputScroll_ = new QScrollArea;
    outputScroll_->setWidgetResizable(true);
    outputScroll_->setWidget(outputContainer_);
    outputScroll_->setMinimumHeight(120);
    outputScroll_->setFrameShape(QFrame::NoFrame);
    mainLayout_->addWidget(outputScroll_, 1);

    setMinimumWidth(240);
}

// ─────────────────────────────────────────────────────────────────────────────
// makeSectionHeader
// ─────────────────────────────────────────────────────────────────────────────
QLabel* IOPanel::makeSectionHeader(const QString& title)
{
    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        "color: #aaa; font-weight: bold; font-size: 10px; "
        "letter-spacing: 2px; padding: 4px 2px;");
    return lbl;
}

// ─────────────────────────────────────────────────────────────────────────────
// makeDivider
// ─────────────────────────────────────────────────────────────────────────────
QFrame* IOPanel::makeDivider()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet("background-color: #3a3a3a; border: none;");
    return line;
}

// ─────────────────────────────────────────────────────────────────────────────
// clearPanel
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::clearPanel()
{
    auto clearLayout = [](QVBoxLayout* layout) {
        for (int i = layout->count() - 2; i >= 0; --i) {
            QLayoutItem* item = layout->takeAt(i);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    };

    clearLayout(inputLayout_);
    clearLayout(outputLayout_);

    boolOutputLabels_.clear();
    numberOutputLabels_.clear();
    boolInputButtons_.clear();
    compositeControls_.clear();
    mc_ = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadMicrocontroller
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::loadMicrocontroller(Microcontroller* mc)
{
    clearPanel();
    mc_ = mc;
    if (!mc_) return;

    bool firstInput  = true;
    bool firstOutput = true;

    for (const auto& [id, nodePtr] : mc_->nodes()) {
        Node* n = nodePtr.get();
        switch (n->type) {
            case NodeType::CompositeInput:
                if (!firstInput) inputLayout_->insertWidget(inputLayout_->count() - 1, makeDivider());
                buildCompositeInputBlock(static_cast<CompositeInputNode*>(n));
                firstInput = false;
                break;
            case NodeType::BoolInput:
                if (!firstInput) inputLayout_->insertWidget(inputLayout_->count() - 1, makeDivider());
                buildInputRow(static_cast<BoolInputNode*>(n));
                firstInput = false;
                break;
            case NodeType::NumberInput:
                if (!firstInput) inputLayout_->insertWidget(inputLayout_->count() - 1, makeDivider());
                buildInputRow(static_cast<NumberInputNode*>(n));
                firstInput = false;
                break;
            case NodeType::BoolOutput:
                if (!firstOutput) outputLayout_->insertWidget(outputLayout_->count() - 1, makeDivider());
                buildOutputRow(static_cast<BoolOutputNode*>(n));
                firstOutput = false;
                break;
            case NodeType::NumberOutput:
                if (!firstOutput) outputLayout_->insertWidget(outputLayout_->count() - 1, makeDivider());
                buildOutputRow(static_cast<NumberOutputNode*>(n));
                firstOutput = false;
                break;
            default:
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildInputRow — Bool toggle button
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::buildInputRow(BoolInputNode* n)
{
    int nodeId = n->id;

    auto* row = new QWidget;
    row->setContentsMargins(0, 4, 0, 4);
    auto* hl  = new QHBoxLayout(row);
    hl->setContentsMargins(2, 2, 2, 2);

    auto* lbl = new QLabel(QString("Bool In %1").arg(n->channelIndex + 1));
    lbl->setStyleSheet("color: #ccc; font-size: 11px;");
    hl->addWidget(lbl);
    hl->addStretch();

    auto* btn = new QPushButton("OFF");
    btn->setCheckable(true);
    btn->setChecked(false);
    btn->setFixedWidth(60);
    btn->setStyleSheet(
        "QPushButton { background: #333; color: #aaa; border: 1px solid #555; border-radius: 4px; padding: 3px; }"
        "QPushButton:checked { background: #00aa33; color: white; border-color: #00dd44; }");

    connect(btn, &QPushButton::toggled, this, [this, nodeId, btn](bool checked) {
        btn->setText(checked ? "ON" : "OFF");
        onBoolToggled(nodeId, checked);
    });

    hl->addWidget(btn);
    boolInputButtons_[nodeId] = btn;
    inputLayout_->insertWidget(inputLayout_->count() - 1, row);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildInputRow — Number slider + spinbox + step size
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::buildInputRow(NumberInputNode* n)
{
    int    nodeId   = n->id;
    double minVal   = n->minValue;
    double maxVal   = n->maxValue;
    double stepSize = n->stepSize;

    auto* container = new QWidget;
    container->setContentsMargins(0, 4, 0, 4);
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(2, 2, 2, 2);
    vl->setSpacing(3);

    // Top row: label + value spinbox
    auto* topRow = new QWidget;
    auto* hl = new QHBoxLayout(topRow);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* lbl = new QLabel(QString("Num In %1").arg(n->channelIndex + 1));
    lbl->setStyleSheet("color: #ccc; font-size: 11px;");
    hl->addWidget(lbl);
    hl->addStretch();

    auto* spin = new QDoubleSpinBox;
    spin->setRange(minVal, maxVal);
    spin->setSingleStep(stepSize);
    spin->setDecimals(2);
    spin->setValue(0.0);
    spin->setFixedWidth(110);
    hl->addWidget(spin);
    vl->addWidget(topRow);

    // Slider
    int sliderScale = (stepSize > 0) ? qMax(1, (int)(1.0 / stepSize)) : 10;
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange((int)(minVal * sliderScale), (int)(maxVal * sliderScale));
    slider->setValue(0);
    slider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #444; }"
        "QSlider::handle:horizontal { background: #00ccdd; width: 12px; margin: -4px 0; border-radius: 6px; }");
    vl->addWidget(slider);

    // Step row
    auto* stepRow = new QWidget;
    auto* stepHl  = new QHBoxLayout(stepRow);
    stepHl->setContentsMargins(0, 0, 0, 0);

    auto* stepLbl = new QLabel("Step:");
    stepLbl->setStyleSheet("color: #888; font-size: 10px;");
    stepHl->addWidget(stepLbl);

    auto* stepSpin = new QDoubleSpinBox;
    stepSpin->setRange(0.001, 1000.0);
    stepSpin->setDecimals(3);
    stepSpin->setSingleStep(0.1);
    stepSpin->setValue(stepSize);
    stepSpin->setFixedWidth(90);
    stepHl->addWidget(stepSpin);
    stepHl->addStretch();
    vl->addWidget(stepRow);

    bool* updating = new bool(false);

    connect(stepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [n, spin, slider, updating](double newStep) {
        if (*updating) return;
        *updating = true;
        n->stepSize = newStep;
        spin->setSingleStep(newStep);
        int newScale = newStep > 0 ? qMax(1, (int)(1.0 / newStep)) : 10;
        double currentVal = spin->value();
        slider->setRange((int)(n->minValue * newScale), (int)(n->maxValue * newScale));
        slider->setValue((int)(currentVal * newScale));
        *updating = false;
    });

    connect(slider, &QSlider::valueChanged, this,
            [this, nodeId, spin, stepSpin, updating](int val) {
        if (*updating) return;
        *updating = true;
        double step = stepSpin->value();
        int scale = step > 0 ? qMax(1, (int)(1.0 / step)) : 10;
        double d = (double)val / scale;
        spin->setValue(d);
        onNumberChanged(nodeId, d);
        *updating = false;
    });

    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, nodeId, slider, stepSpin, updating](double val) {
        if (*updating) return;
        *updating = true;
        double step = stepSpin->value();
        int scale = step > 0 ? qMax(1, (int)(1.0 / step)) : 10;
        slider->setValue((int)(val * scale));
        onNumberChanged(nodeId, val);
        *updating = false;
    });

    inputLayout_->insertWidget(inputLayout_->count() - 1, container);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildCompositeInputBlock
//
// Builds a full inline settings + controls block for a CompositeInput node.
// The channel count spinbox and per-channel index/type settings live here
// instead of in a ⚙ dialog so changes reflect immediately.
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::buildCompositeInputBlock(CompositeInputNode* n)
{
    int nodeId = n->id;

    auto* block = new QWidget;
    block->setContentsMargins(0, 4, 0, 4);
    auto* vl = new QVBoxLayout(block);
    vl->setContentsMargins(2, 2, 2, 2);
    vl->setSpacing(4);

    // ── Header row: label + channel count spinbox ─────────────────────────────
    auto* headerRow = new QWidget;
    auto* headerHl  = new QHBoxLayout(headerRow);
    headerHl->setContentsMargins(0, 0, 0, 0);

    auto* header = new QLabel(QString("Composite In %1").arg(nodeId));
    header->setStyleSheet("color: #aa44dd; font-size: 11px; font-weight: bold;");
    headerHl->addWidget(header);
    headerHl->addStretch();

    auto* countLbl = new QLabel("Ch:");
    countLbl->setStyleSheet("color: #888; font-size: 10px;");
    headerHl->addWidget(countLbl);

    auto* countSpin = new QSpinBox;
    countSpin->setRange(1, 32);
    countSpin->setValue((int)n->channels.size());
    countSpin->setFixedWidth(70);
    headerHl->addWidget(countSpin);
    vl->addWidget(headerRow);

    // ── Channel rows container — rebuilt when count changes ───────────────────
    auto* channelContainer = new QWidget;
    auto* channelLayout    = new QVBoxLayout(channelContainer);
    channelLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->setSpacing(3);
    vl->addWidget(channelContainer);

    // ── Helper: build all channel rows from current node config ──────────────
    // This lambda is called on load and whenever the count spinbox changes.
    auto buildChannelRows = [this, n, nodeId, channelLayout]() {
        // Clear existing channel rows and controls for this node
        compositeControls_.erase(
            std::remove_if(compositeControls_.begin(), compositeControls_.end(),
                           [nodeId](const CompositeChannelControl& c) {
                               return c.nodeId == nodeId;
                           }),
            compositeControls_.end());

        QLayoutItem* item;
        while ((item = channelLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        for (int i = 0; i < (int)n->channels.size(); ++i) {
            CompositeChannel& ch = n->channels[i];

            // Thin separator between channels
            if (i > 0) {
                auto* sep = new QFrame;
                sep->setFrameShape(QFrame::HLine);
                sep->setFixedHeight(2);
                sep->setStyleSheet("background: #333; border: none;");
                channelLayout->addWidget(sep);
            }

            auto* chBlock = new QWidget;
            auto* chVl    = new QVBoxLayout(chBlock);
            chVl->setContentsMargins(4, 2, 2, 2);
            chVl->setSpacing(2);

            // Settings row: Ch label + idx spinbox + type combo
            auto* settingsRow = new QWidget;
            auto* settingsHl  = new QHBoxLayout(settingsRow);
            settingsHl->setContentsMargins(0, 0, 0, 0);

            auto* chLbl = new QLabel(QString("Ch%1").arg(i + 1));
            chLbl->setStyleSheet("color: #aa44dd; font-size: 10px; font-weight: bold;");
            settingsHl->addWidget(chLbl);

            auto* idxLbl = new QLabel("idx:");
            idxLbl->setStyleSheet("color: #777; font-size: 10px;");
            settingsHl->addWidget(idxLbl);

            auto* idxSpin = new QSpinBox;
            idxSpin->setRange(1, 32);
            idxSpin->setValue(ch.index);
            idxSpin->setFixedWidth(70);
            settingsHl->addWidget(idxSpin);

            auto* typeCombo = new QComboBox;
            typeCombo->addItem("Number");
            typeCombo->addItem("Bool");
            typeCombo->setCurrentIndex(ch.dataType == PortType::Bool ? 1 : 0);
            typeCombo->setStyleSheet(
                "QComboBox { background: #2a2a2a; color: #bbb; border: 1px solid #444; "
                "  font-size: 10px; padding: 1px 4px; }"
                "QComboBox::drop-down { width: 16px; }"
                "QComboBox QAbstractItemView { background: #2a2a2a; color: #bbb; }");
            typeCombo->setFixedWidth(70);
            settingsHl->addWidget(typeCombo);
            settingsHl->addStretch();
            chVl->addWidget(settingsRow);

            // Update channel index when idx spinbox changes
            connect(idxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [&ch](int val) { ch.index = val; });

            CompositeChannelControl ctrl;
            ctrl.nodeId     = nodeId;
            ctrl.channelIdx = i;

            // Control row: value control (changes based on type)
            // We use a stacked approach — create both and show the right one
            auto* controlRow = new QWidget;
            auto* controlHl  = new QHBoxLayout(controlRow);
            controlHl->setContentsMargins(0, 0, 0, 0);
            controlHl->setSpacing(4);

            // Bool control
            auto* boolBtn = new QPushButton("OFF");
            boolBtn->setCheckable(true);
            boolBtn->setChecked(false);
            boolBtn->setFixedWidth(60);
            boolBtn->setStyleSheet(
                "QPushButton { background: #333; color: #aaa; border: 1px solid #555; border-radius: 4px; padding: 3px; font-size: 10px; }"
                "QPushButton:checked { background: #00aa33; color: white; border-color: #00dd44; }");
            controlHl->addWidget(boolBtn);

            // Number control
            auto* numSpin = new QDoubleSpinBox;
            numSpin->setRange(-1000.0, 1000.0);
            numSpin->setSingleStep(0.1);
            numSpin->setDecimals(3);
            numSpin->setValue(0.0);
            controlHl->addWidget(numSpin);
            controlHl->addStretch();
            chVl->addWidget(controlRow);

            // Slider (for number only)
            auto* slider = new QSlider(Qt::Horizontal);
            slider->setRange(-10000, 10000);
            slider->setValue(0);
            slider->setStyleSheet(
                "QSlider::groove:horizontal { height: 4px; background: #444; }"
                "QSlider::handle:horizontal { background: #aa44dd; width: 12px; margin: -4px 0; border-radius: 6px; }");
            chVl->addWidget(slider);

            // Show/hide based on current type
            bool isBool = (ch.dataType == PortType::Bool);
            boolBtn->setVisible(isBool);
            numSpin->setVisible(!isBool);
            slider->setVisible(!isBool);

            // Type combo changes visibility and updates channel config
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [&ch, boolBtn, numSpin, slider](int idx) {
                bool nowBool = (idx == 1);
                ch.dataType = nowBool ? PortType::Bool : PortType::Number;
                boolBtn->setVisible(nowBool);
                numSpin->setVisible(!nowBool);
                slider->setVisible(!nowBool);
            });

            // Bool button signal
            connect(boolBtn, &QPushButton::toggled, this,
                    [this, nodeId, i, boolBtn](bool checked) {
                boolBtn->setText(checked ? "ON" : "OFF");
                onCompositeBoolChanged(nodeId, i, checked);
            });

            // Number spin/slider sync
            bool* updating = new bool(false);
            connect(slider, &QSlider::valueChanged, this,
                    [this, nodeId, i, numSpin, updating](int val) {
                if (*updating) return;
                *updating = true;
                double d = val / 10.0;
                numSpin->setValue(d);
                onCompositeNumberChanged(nodeId, i, d);
                *updating = false;
            });
            connect(numSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this, nodeId, i, slider, updating](double val) {
                if (*updating) return;
                *updating = true;
                slider->setValue((int)(val * 10));
                onCompositeNumberChanged(nodeId, i, val);
                *updating = false;
            });

            ctrl.boolBtn = boolBtn;
            ctrl.numSpin = numSpin;
            ctrl.slider  = slider;
            compositeControls_.push_back(ctrl);

            channelLayout->addWidget(chBlock);
        }
    };

    // Build initial rows
    buildChannelRows();

    // Rebuild when channel count changes
    connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [n, buildChannelRows](int count) {
        // Resize channels vector
        while ((int)n->channels.size() < count)
            n->channels.push_back({ (int)n->channels.size() + 1, PortType::Number });
        while ((int)n->channels.size() > count)
            n->channels.pop_back();
        buildChannelRows();
    });

    inputLayout_->insertWidget(inputLayout_->count() - 1, block);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildOutputRow — Bool indicator
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::buildOutputRow(BoolOutputNode* n)
{
    auto* row = new QWidget;
    row->setContentsMargins(0, 4, 0, 4);
    auto* hl  = new QHBoxLayout(row);
    hl->setContentsMargins(2, 2, 2, 2);

    auto* lbl = new QLabel(QString("Bool Out %1").arg(n->channelIndex + 1));
    lbl->setStyleSheet("color: #ccc; font-size: 11px;");
    hl->addWidget(lbl);
    hl->addStretch();

    auto* indicator = new QLabel("OFF");
    indicator->setFixedWidth(60);
    indicator->setAlignment(Qt::AlignCenter);
    indicator->setStyleSheet(
        "background: #440000; color: #aaa; border-radius: 4px; padding: 3px; font-size: 11px;");
    hl->addWidget(indicator);

    boolOutputLabels_[n->id] = indicator;
    outputLayout_->insertWidget(outputLayout_->count() - 1, row);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildOutputRow — Number readout
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::buildOutputRow(NumberOutputNode* n)
{
    auto* row = new QWidget;
    row->setContentsMargins(0, 4, 0, 4);
    auto* hl  = new QHBoxLayout(row);
    hl->setContentsMargins(2, 2, 2, 2);

    auto* lbl = new QLabel(QString("Num Out %1").arg(n->channelIndex + 1));
    lbl->setStyleSheet("color: #ccc; font-size: 11px;");
    hl->addWidget(lbl);
    hl->addStretch();

    auto* value = new QLabel("0.000");
    value->setFixedWidth(90);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setStyleSheet(
        "color: #00ddff; font-family: monospace; font-size: 12px;");
    hl->addWidget(value);

    numberOutputLabels_[n->id] = value;
    outputLayout_->insertWidget(outputLayout_->count() - 1, row);
}

// ─────────────────────────────────────────────────────────────────────────────
// refresh — update output labels after each tick
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::refresh()
{
    if (!mc_) return;

    for (auto& [nodeId, label] : boolOutputLabels_) {
        Node* n = mc_->node(nodeId);
        if (!n || n->outputs.empty()) continue;
        bool on = n->outputs[0].asBool();
        label->setText(on ? "ON" : "OFF");
        label->setStyleSheet(on
            ? "background: #006622; color: #00ff55; border-radius: 4px; padding: 3px; font-size: 11px;"
            : "background: #440000; color: #aaa;    border-radius: 4px; padding: 3px; font-size: 11px;");
    }

    for (auto& [nodeId, label] : numberOutputLabels_) {
        Node* n = mc_->node(nodeId);
        if (!n || n->outputs.empty()) continue;
        label->setText(QString::number(n->outputs[0].asNumber(), 'f', 4));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onBoolToggled
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::onBoolToggled(int nodeId, bool value)
{
    if (!mc_) return;
    Node* n = mc_->node(nodeId);
    if (n && !n->inputs.empty())
        n->inputs[0].set(value);
    emit inputChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// onNumberChanged
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::onNumberChanged(int nodeId, double value)
{
    if (!mc_) return;
    Node* n = mc_->node(nodeId);
    if (n && !n->inputs.empty())
        n->inputs[0].set(value);
    emit inputChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// onCompositeBoolChanged
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::onCompositeBoolChanged(int nodeId, int channelIdx, bool value)
{
    if (!mc_) return;
    Node* n = mc_->node(nodeId);
    if (!n) return;

    while ((int)n->inputs.size() <= channelIdx)
        n->inputs.push_back(Port::makeBool(QString("Ch%1").arg(n->inputs.size() + 1)));

    n->inputs[channelIdx].set(value);
    emit inputChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// onCompositeNumberChanged
// ─────────────────────────────────────────────────────────────────────────────
void IOPanel::onCompositeNumberChanged(int nodeId, int channelIdx, double value)
{
    if (!mc_) return;
    Node* n = mc_->node(nodeId);
    if (!n) return;

    while ((int)n->inputs.size() <= channelIdx)
        n->inputs.push_back(Port::makeNumber(QString("Ch%1").arg(n->inputs.size() + 1)));

    n->inputs[channelIdx].set(value);
    emit inputChanged();
}
