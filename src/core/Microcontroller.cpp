#include "Microcontroller.h"
#include "nodes/IONodes.h"
#include "nodes/CompositeNodes.h"
#include "nodes/MathNodes.h"
#include "nodes/StatefulNodes.h"
#include "nodes/FunctionNodes.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QDebug>

#include <queue>
#include <unordered_set>
#include <set>
#include <utility>
#include <algorithm>
#include <cmath>

// ── Stormworks XML position <-> simulator pixel mapping ───────────────────────
// Loading converts <pos x y> (grid units) to pixels with these constants; saving
// applies the exact inverse so positions round-trip. I/O bridge nodes are
// shifted further right than logic components so they start on the far side.
namespace {
constexpr int kPositionScale  = 40;  // pixels per Stormworks grid unit
constexpr int kCompOffsetX    = 40;  // components: 1 grid cell right
constexpr int kBridgeOffsetX  = 400; // I/O bridge nodes: 10 grid cells right
constexpr int kOffsetY        = 120; // 3 grid cells down
}

// ─────────────────────────────────────────────────────────────────────────────
// addNode
// ─────────────────────────────────────────────────────────────────────────────
int Microcontroller::addNode(std::unique_ptr<Node> node)
{
    int id = nextId_++;
    node->id = id;
    nodes_[id] = std::move(node);
    buildEvalOrder();
    return id;
}

// ─────────────────────────────────────────────────────────────────────────────
// addConnection
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::addConnection(const Connection& conn)
{
    for (const auto& c : connections_) {
        if (c.sourceNodeId  == conn.sourceNodeId  &&
            c.sourcePortIdx == conn.sourcePortIdx &&
            c.targetNodeId  == conn.targetNodeId  &&
            c.targetPortIdx == conn.targetPortIdx)
            return;
    }
    connections_.push_back(conn);
    buildEvalOrder();
}

// ─────────────────────────────────────────────────────────────────────────────
// removeConnectionToInput
//
// Since each input port may have at most one source, this removes the
// single connection (if any) whose target matches targetNodeId/targetPortIdx.
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::removeConnectionToInput(int targetNodeId, int targetPortIdx)
{
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->targetNodeId == targetNodeId && it->targetPortIdx == targetPortIdx) {
            connections_.erase(it);
            buildEvalOrder();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// removeNode
//
// Removes the node itself plus every connection where it appears as either
// the source or the target. Rebuilds the evaluation order afterward.
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::removeNode(int nodeId)
{
    // Remove all connections touching this node (as source or target)
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [nodeId](const Connection& c) {
                           return c.sourceNodeId == nodeId || c.targetNodeId == nodeId;
                       }),
        connections_.end());

    // Remove the node itself
    nodes_.erase(nodeId);

    buildEvalOrder();
}

// ─────────────────────────────────────────────────────────────────────────────
// clear
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::clear()
{
    nodes_.clear();
    connections_.clear();
    evalOrder_.clear();
    nextId_    = 0;
    tickCount_ = 0;
    originalComponentIds_.clear();
    originalBridgeIds_.clear();
    originalDoc_ = QDomDocument();
    currentFilePath_.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// node — lookup by id
// ─────────────────────────────────────────────────────────────────────────────
Node* Microcontroller::node(int id) const
{
    auto it = nodes_.find(id);
    return (it != nodes_.end()) ? it->second.get() : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// tick — evaluate in topological order, push outputs immediately after each node
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::tick()
{
    for (int id : evalOrder_) {
        Node* n = node(id);
        if (!n) continue;

        n->evaluate();

        // Push outputs to downstream inputs immediately so next node sees fresh values
        for (const auto& conn : connections_) {
            if (conn.sourceNodeId != id) continue;
            Node* dst = node(conn.targetNodeId);
            if (!dst) continue;
            if (conn.sourcePortIdx >= (int)n->outputs.size()) continue;
            if (conn.targetPortIdx >= (int)dst->inputs.size()) continue;
            dst->inputs[conn.targetPortIdx].value =
                n->outputs[conn.sourcePortIdx].value;
        }
    }
    ++tickCount_;
}

// ─────────────────────────────────────────────────────────────────────────────
// propagateValues
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::propagateValues()
{
    for (const auto& conn : connections_) {
        Node* src = node(conn.sourceNodeId);
        Node* dst = node(conn.targetNodeId);
        if (!src || !dst) continue;
        if (conn.sourcePortIdx >= (int)src->outputs.size()) continue;
        if (conn.targetPortIdx >= (int)dst->inputs.size())  continue;
        dst->inputs[conn.targetPortIdx].value =
            src->outputs[conn.sourcePortIdx].value;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildEvalOrder — Kahn's topological sort
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::buildEvalOrder()
{
    evalOrder_.clear();
    if (nodes_.empty()) return;

    std::unordered_map<int, int> inDegree;
    for (const auto& [id, _] : nodes_) inDegree[id] = 0;

    std::unordered_map<int, std::vector<int>> adj;
    for (const auto& conn : connections_) {
        adj[conn.sourceNodeId].push_back(conn.targetNodeId);
        inDegree[conn.targetNodeId]++;
    }

    std::vector<int> zeroIn;
    for (const auto& [id, deg] : inDegree) {
        if (deg == 0) zeroIn.push_back(id);
    }

    std::sort(zeroIn.begin(), zeroIn.end(), [this](int a, int b) {
        Node* na = node(a);
        Node* nb = node(b);
        auto priority = [](NodeType t) -> int {
            switch (t) {
                case NodeType::BoolInput:     return 0;
                case NodeType::NumberInput:   return 0;
                case NodeType::NumberLiteral: return 0;
                default:                      return 1;
            }
        };
        if (!na || !nb) return a < b;
        int pa = priority(na->type);
        int pb = priority(nb->type);
        if (pa != pb) return pa < pb;
        return a < b;
    });

    std::queue<int> ready;
    for (int id : zeroIn) ready.push(id);

    while (!ready.empty()) {
        int id = ready.front();
        ready.pop();
        evalOrder_.push_back(id);
        std::vector<int>& neighbours = adj[id];
        std::sort(neighbours.begin(), neighbours.end());
        for (int neighbour : neighbours) {
            if (--inDegree[neighbour] == 0)
                ready.push(neighbour);
        }
    }

    if ((int)evalOrder_.size() < (int)nodes_.size()) {
        for (const auto& [id, _] : nodes_) {
            bool already = std::find(evalOrder_.begin(), evalOrder_.end(), id)
                           != evalOrder_.end();
            if (!already) evalOrder_.push_back(id);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// preserveAttributeWhitespace
//
// Replaces raw \n, \r and \t characters found inside quoted attribute values
// with character references (&#10; &#13; &#9;), which XML parsers keep as-is.
// Text outside tags and comments is left untouched.
// ─────────────────────────────────────────────────────────────────────────────
static QString preserveAttributeWhitespace(const QString& xml)
{
    QString out;
    out.reserve(xml.size());

    bool inTag   = false; // between '<' and '>'
    QChar quote;          // the open quote character, or null if not in a value

    for (int i = 0; i < xml.size(); ++i) {
        const QChar c = xml[i];

        if (!inTag) {
            if (c == '<') {
                // Copy comments and CDATA verbatim so quotes in them are ignored
                if (xml.mid(i, 4) == "<!--") {
                    int end = xml.indexOf("-->", i + 4);
                    end = (end < 0) ? xml.size() : end + 3;
                    out.append(xml.mid(i, end - i));
                    i = end - 1;
                    continue;
                }
                if (xml.mid(i, 9) == "<![CDATA[") {
                    int end = xml.indexOf("]]>", i + 9);
                    end = (end < 0) ? xml.size() : end + 3;
                    out.append(xml.mid(i, end - i));
                    i = end - 1;
                    continue;
                }
                inTag = true;
            }
            out.append(c);
            continue;
        }

        // Inside a tag
        if (!quote.isNull()) {
            if (c == quote)       { quote = QChar(); out.append(c); }
            else if (c == '\n')   out.append("&#10;");
            else if (c == '\r')   out.append("&#13;");
            else if (c == '\t')   out.append("&#9;");
            else                  out.append(c);
        } else {
            if (c == '"' || c == '\'') quote = c;
            else if (c == '>')         inTag = false;
            out.append(c);
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromFile
// ─────────────────────────────────────────────────────────────────────────────
bool Microcontroller::loadFromFile(const QString& path, QString& errorMsg)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Cannot open file: ") + path;
        return false;
    }

    // XML parsers normalise raw newlines/tabs inside attribute values to plain
    // spaces, but Stormworks writes Lua scripts as raw multi-line attributes.
    // Turning them into character references first keeps the script intact
    // (a flattened script would have its -- comments swallow the code).
    const QString xmlText = preserveAttributeWhitespace(QString::fromUtf8(file.readAll()));

    QDomDocument doc;
    QString parseError;
    int line = 0, col = 0;
    if (!doc.setContent(xmlText, &parseError, &line, &col)) {
        errorMsg = QString("XML parse error at line %1, col %2: %3")
                   .arg(line).arg(col).arg(parseError);
        return false;
    }

    clear();
    bool ok = parseXmlDocument(doc, errorMsg);
    if (ok) {
        originalDoc_ = doc;
        currentFilePath_ = path;
    }
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// parseXmlDocument
// ─────────────────────────────────────────────────────────────────────────────
bool Microcontroller::parseXmlDocument(const QDomDocument& doc, QString& errorMsg)
{
    QDomElement root = doc.documentElement();

    QDomElement group = root.firstChildElement("group");
    if (group.isNull()) {
        errorMsg = "Missing <group> element";
        return false;
    }

    struct NodeInfo {
        NodeType type;
        int px = 0, py = 0;
        // One entry per wired input: target port, source component, and which
        // of the source's outputs it comes from (node_index attribute, 0 if absent)
        struct InputRef { int portIdx; int srcId; int srcPort; };
        std::vector<InputRef> inputs;
        double literalValue = 0.0;
        bool   hasLiteral   = false;
        int    channelCount = 1;   // for composite packer/splitter/write
        int    channelIndex = 1;   // for composite read nodes
        int    startIndex   = 1;   // for composite write nodes
        int    compositeInSrc = -1; // source component id for <inc/>, -1 if none
        // UpDownCounter fields
        double counterStep   = 1.0;
        double counterMin    = 0.0;
        double counterMax    = 0.0;
        // PID fields
        double pidKp = 0.0;
        double pidKi = 0.0;
        double pidKd = 0.0;
        // Function node expression
        QString expression;
        // Capacitor: ct / dt attributes (seconds)
        double capCharge    = 1.0;
        double capDischarge = 1.0;
        // Property dropdown: name="" and i="" attributes plus <items>
        QString dropName = "value";
        int     dropSelected = 0;
        std::vector<std::pair<QString, double>> dropItems;
        bool    hasDropItems = false;
        // Property number/slider/toggle/text and tooltip nodes
        double  propValue = 0.0;            // <v text="X"/>
        double  propStep  = 1.0;            // <int text="X"/> (slider step)
        QString textN;                      // n="..." (property name)
        bool    propBool = false;           // v="true" (property toggle)
        QString tipLabel = "value";         // l="..." (tooltips)
        QString tipOn    = "on";            // on="..." (tooltip on/off)
        QString tipOff   = "off";           // off="..."
        double  blinkOn  = 0.5;             // on="seconds"  (blinker)
        double  blinkOff = 0.5;             // off="seconds"
        QString script;                     // script="..." (Lua node)
    };

    std::unordered_map<int, NodeInfo> allNodes;

    // ── Helper: parse a single <object> element ───────────────────────────────
    // Position scale: Stormworks XML units are converted to pixels using our
    // grid cell size (80px) so loaded nodes land exactly on grid lines without
    // relying on NodeGraphView's snap-to-grid to silently correct the offset.
    auto parseObject = [](const QDomElement& obj, NodeInfo& info) {
        // Position
        QDomElement pos = obj.firstChildElement("pos");
        if (!pos.isNull()) {
            info.px = (int)(pos.attribute("x", "0").toFloat() * kPositionScale);
            info.py = (int)(pos.attribute("y", "0").toFloat() * kPositionScale);
        }

        // Channel count for packer/splitter/write
        if (obj.hasAttribute("count"))
            info.channelCount = obj.attribute("count", "1").toInt();

        // Channel index for composite read nodes
        if (obj.hasAttribute("channel"))
            info.channelIndex = obj.attribute("channel", "1").toInt();

        // Start index for composite write nodes (if Stormworks exposes one
        // as an attribute; falls back to 1 if absent)
        if (obj.hasAttribute("start_channel"))
            info.startIndex = obj.attribute("start_channel", "1").toInt();

        // Composite-in connection: <inc component_id="X"/> — used by
        // Composite Write nodes to pass an existing composite wire through
        QDomElement incElem = obj.firstChildElement("inc");
        if (!incElem.isNull() && incElem.hasAttribute("component_id"))
            info.compositeInSrc = incElem.attribute("component_id").toInt();

        // Number literal value: <n text="X" value="X"/>
        QDomElement nElem = obj.firstChildElement("n");
        if (!nElem.isNull() && nElem.hasAttribute("value")) {
            info.literalValue = nElem.attribute("value", "0").toDouble();
            info.hasLiteral   = true;
        }

        // Equal To tolerance: <e text="X" value="X"/> — reuses the same
        // literalValue/hasLiteral fields since the structure is identical,
        // just a different element name and semantic meaning.
        QDomElement eElem = obj.firstChildElement("e");
        if (!eElem.isNull() && eElem.hasAttribute("value")) {
            info.literalValue = eElem.attribute("value", "0.0001").toDouble();
            info.hasLiteral   = true;
        }

        // UpDownCounter: <i value="X"/> = step, <min text="X"/> = min,
        // <max text="X"/> = max. Min/max of 0 means unclamped in Stormworks.
        QDomElement iElem = obj.firstChildElement("i");
        if (!iElem.isNull())
            info.counterStep = iElem.attribute("value", "1").toDouble();
        QDomElement minElem = obj.firstChildElement("min");
        if (!minElem.isNull())
            info.counterMin = minElem.attribute("text", "0").toDouble();
        QDomElement maxElem = obj.firstChildElement("max");
        if (!maxElem.isNull())
            info.counterMax = maxElem.attribute("text", "0").toDouble();

        // Memory Register / UpDownCounter reset value: <r text="X"/>
        // For MemoryRegister this is the initial stored value.
        // We reuse literalValue/hasLiteral since the structure is the same.
        QDomElement rElem2 = obj.firstChildElement("r");
        if (!rElem2.isNull() && !rElem2.attribute("text").isEmpty()) {
            info.literalValue = rElem2.attribute("text", "0").toDouble();
            info.hasLiteral   = true;
        }

        // PID Controller gains: <kp text="X"/>, <ki text="X"/>, <kd text="X"/>
        QDomElement kpElem = obj.firstChildElement("kp");
        if (!kpElem.isNull())
            info.pidKp = kpElem.attribute("text", "0").toDouble();
        QDomElement kiElem = obj.firstChildElement("ki");
        if (!kiElem.isNull())
            info.pidKi = kiElem.attribute("text", "0").toDouble();
        QDomElement kdElem = obj.firstChildElement("kd");
        if (!kdElem.isNull())
            info.pidKd = kdElem.attribute("text", "0").toDouble();

        // Function node expression stored as e="..." attribute on <object>
        if (obj.hasAttribute("e"))
            info.expression = obj.attribute("e");

        // Property dropdown: name="X" i="selected index", <items><i l="label"><v value="N"/>
        if (obj.hasAttribute("name"))
            info.dropName = obj.attribute("name");
        if (obj.hasAttribute("i"))
            info.dropSelected = obj.attribute("i", "0").toInt();
        QDomElement itemsElem = obj.firstChildElement("items");
        if (!itemsElem.isNull()) {
            info.hasDropItems = true;
            for (QDomElement it = itemsElem.firstChildElement("i"); !it.isNull();
                 it = it.nextSiblingElement("i")) {
                QDomElement v = it.firstChildElement("v");
                double val = v.isNull() ? 0.0
                           : v.attribute("value", v.attribute("text", "0")).toDouble();
                info.dropItems.push_back({ it.attribute("l"), val });
            }
        }

        // Property number/slider value, slider step, property text, tooltip text
        QDomElement vElem = obj.firstChildElement("v");
        if (!vElem.isNull())
            info.propValue = vElem.attribute("value", vElem.attribute("text", "0")).toDouble();
        QDomElement intElem = obj.firstChildElement("int");
        if (!intElem.isNull())
            info.propStep = intElem.attribute("value", intElem.attribute("text", "1")).toDouble();
        if (obj.hasAttribute("script")) info.script = obj.attribute("script");
        if (obj.hasAttribute("n"))   info.textN   = obj.attribute("n");
        if (obj.hasAttribute("v"))   info.propBool = (obj.attribute("v") == "true");
        if (obj.hasAttribute("l"))   info.tipLabel = obj.attribute("l");
        if (obj.hasAttribute("on")) {
            info.tipOn   = obj.attribute("on");
            info.blinkOn = obj.attribute("on").toDouble();
        }
        if (obj.hasAttribute("off")) {
            info.tipOff   = obj.attribute("off");
            info.blinkOff = obj.attribute("off").toDouble();
        }

        // Capacitor charge/discharge times: ct="X" dt="Y" attributes on <object>
        if (obj.hasAttribute("ct"))
            info.capCharge = obj.attribute("ct", "1").toDouble();
        if (obj.hasAttribute("dt"))
            info.capDischarge = obj.attribute("dt", "1").toDouble();

        // Inputs: <in1 component_id="X"/>, <in2 .../> etc.
        for (int port = 1; port <= 32; ++port) {
            QDomElement inp = obj.firstChildElement(QString("in%1").arg(port));
            if (inp.isNull()) break;
            if (inp.hasAttribute("component_id")) {
                int srcId   = inp.attribute("component_id").toInt();
                int srcPort = inp.attribute("node_index", "0").toInt();
                info.inputs.push_back({port - 1, srcId, srcPort});
            }
        }
    };

    // ── Step 1: parse components_bridge (I/O and composite input nodes) ───────
    // Type mapping:
    //   1 = BoolOutput (no type attr also = BoolOutput)
    //   2 = NumberInput
    //   3 = NumberOutput
    //   4 = NumberInput (composite input port — treated as number input)
    //   5 = BoolInput   (composite input port — treated as bool input)
    QDomElement compBridge = group.firstChildElement("components_bridge");
    if (!compBridge.isNull()) {
        QDomNodeList cList = compBridge.childNodes();
        for (int i = 0; i < cList.count(); ++i) {
            QDomElement c = cList.at(i).toElement();
            if (c.isNull()) continue;

            QString typeStr = c.attribute("type", "");
            NodeType ntype;
            if      (typeStr == "1") ntype = NodeType::BoolOutput;     // bool output (has input wire)
            else if (typeStr == "2") ntype = NodeType::NumberInput;    // number input (source)
            else if (typeStr == "3") ntype = NodeType::NumberOutput;   // number output (has input wire)
            else if (typeStr == "4") ntype = NodeType::CompositeInput; // composite input (source)
            else if (typeStr == "5") ntype = NodeType::CompositeOutput; // composite output (has input wire)
            else if (typeStr == "6") ntype = NodeType::VideoInput;      // video input (source)
            else if (typeStr == "7") ntype = NodeType::VideoOutput;     // video output (has input wire)
            else if (typeStr == "8") ntype = NodeType::AudioInput;      // audio input (source)
            else if (typeStr == "9") ntype = NodeType::AudioOutput;     // audio output (has input wire)
            else                     ntype = NodeType::BoolInput;      // no type = bool input (source)

            QDomElement obj = c.firstChildElement("object");
            if (obj.isNull()) continue;

            int compId = obj.attribute("id", "-1").toInt();
            NodeInfo info;
            info.type = ntype;
            parseObject(obj, info);
            info.px += kBridgeOffsetX;
            info.py += kOffsetY;
            allNodes[compId] = std::move(info);
            originalBridgeIds_[compId] = true;
        }
    }

    // ── Step 2: parse components (logic/math/composite nodes) ─────────────────
    QDomElement comps = group.firstChildElement("components");
    if (!comps.isNull()) {
        QDomNodeList cList = comps.childNodes();
        for (int i = 0; i < cList.count(); ++i) {
            QDomElement c = cList.at(i).toElement();
            if (c.isNull()) continue;

            int typeId = c.attribute("type", "-1").toInt();
            bool typeOk = false;
            NodeType ntype = nodeTypeFromComponentType(typeId, typeOk);
            if (!typeOk) {
                qWarning("Skipping unknown component type %d", typeId);
                continue;
            }

            QDomElement obj = c.firstChildElement("object");
            if (obj.isNull()) continue;

            int compId = obj.attribute("id", "-1").toInt();
            NodeInfo info;
            info.type = ntype;
            parseObject(obj, info);
            info.px += kCompOffsetX;
            info.py += kOffsetY;
            allNodes[compId] = std::move(info);
            originalComponentIds_[compId] = true;
        }
    }

    // ── Edge connector labels ─────────────────────────────────────────────────
    // <nodes><n component_id="X"><node label="..."> names each I/O connector.
    std::unordered_map<int, QString> edgeLabels;
    {
        QDomElement nodesRoot = root.firstChildElement("nodes");
        for (QDomElement n = nodesRoot.firstChildElement("n"); !n.isNull();
             n = n.nextSiblingElement("n")) {
            QDomElement nd = n.firstChildElement("node");
            if (nd.hasAttribute("label"))
                edgeLabels[n.attribute("component_id", "-1").toInt()] = nd.attribute("label");
        }
    }

    // ── Step 3: create Node objects ───────────────────────────────────────────
    for (auto& [compId, info] : allNodes) {
        auto n = Node::create(info.type);
        if (!n) continue;
        n->id   = compId;
        n->posX = info.px;
        n->posY = info.py;

        // I/O bridge nodes carry their connector label
        if (originalBridgeIds_.count(compId) && edgeLabels.count(compId))
            n->label = edgeLabels.at(compId);

        // Number literal — set constant value
        if (info.type == NodeType::NumberLiteral && info.hasLiteral) {
            auto* lit = static_cast<NumberLiteralNode*>(n.get());
            lit->constantValue = info.literalValue;
        }

        // Equal To — set tolerance from the <e value="X"/> element
        if (info.type == NodeType::EqualTo && info.hasLiteral) {
            auto* eq = static_cast<EqualToNode*>(n.get());
            eq->tolerance = info.literalValue;
        }

        // Function evaluator nodes — set expression from e="" attribute
        if (info.type == NodeType::Function1 || info.type == NodeType::Function3 ||
            info.type == NodeType::Function8  || info.type == NodeType::FunctionBool4 ||
            info.type == NodeType::FunctionBool8) {
            FunctionNodeBase* fn = static_cast<FunctionNodeBase*>(n.get());
            fn->expression = info.expression;
        }

        // PID Controller — set gains from XML elements
        if (info.type == NodeType::PIDController) {
            PIDControllerNode* pid = static_cast<PIDControllerNode*>(n.get());
            pid->kp = info.pidKp;
            pid->ki = info.pidKi;
            pid->kd = info.pidKd;
        }
        if (info.type == NodeType::UpDownCounter) {
            UpDownCounterNode* ctr = static_cast<UpDownCounterNode*>(n.get());
            ctr->step         = info.counterStep;
            ctr->minVal       = info.counterMin;
            ctr->maxVal       = info.counterMax;
            ctr->clampEnabled = (info.counterMin != 0.0 || info.counterMax != 0.0);
        }

        // Lua script text
        if (info.type == NodeType::LuaScript)
            static_cast<LuaScriptNode*>(n.get())->script = info.script;

        // Blinker — on/off durations from the on="" and off="" attributes
        if (info.type == NodeType::Blinker) {
            auto* bl = static_cast<BlinkerNode*>(n.get());
            bl->onTime  = std::clamp(info.blinkOn,  BlinkerNode::kMinTime, BlinkerNode::kMaxTime);
            bl->offTime = std::clamp(info.blinkOff, BlinkerNode::kMinTime, BlinkerNode::kMaxTime);
        }

        // Property / tooltip nodes
        // The property name is n="..." on <object>; absent means the default
        const QString propName = info.textN.isEmpty() ? QString("value") : info.textN;
        if (info.type == NodeType::PropertyNumber) {
            auto* pn = static_cast<PropertyNumberNode*>(n.get());
            pn->propertyName = propName;
            pn->value        = info.propValue;
        }
        if (info.type == NodeType::PropertySlider) {
            auto* ps = static_cast<PropertySliderNode*>(n.get());
            ps->propertyName = propName;
            ps->minValue     = info.counterMin;
            ps->maxValue     = info.counterMax;
            ps->step         = info.propStep;
            ps->value        = info.propValue;
        }
        if (info.type == NodeType::PropertyToggle) {
            auto* pt = static_cast<PropertyToggleNode*>(n.get());
            pt->propertyName = propName;
            pt->value        = info.propBool;
        }
        if (info.type == NodeType::PropertyText) {
            if (!info.textN.isEmpty())
                static_cast<PropertyTextNode*>(n.get())->propertyName = info.textN;
        }
        if (info.type == NodeType::TooltipNumber) {
            static_cast<TooltipNumberNode*>(n.get())->tooltipLabel = info.tipLabel;
        }
        if (info.type == NodeType::TooltipOnOff) {
            auto* tt = static_cast<TooltipOnOffNode*>(n.get());
            tt->tooltipLabel = info.tipLabel;
            tt->onText       = info.tipOn;
            tt->offText      = info.tipOff;
        }

        // Threshold — bounds come from <min text="X"/> / <max text="X"/>, which
        // parseObject already stores in counterMin/counterMax
        if (info.type == NodeType::Threshold) {
            auto* th = static_cast<ThresholdNode*>(n.get());
            th->minBound = info.counterMin;
            th->maxBound = info.counterMax;
        }

        // Property dropdown — name, options and selected index
        if (info.type == NodeType::PropertyDropdown) {
            auto* dd = static_cast<PropertyDropdownNode*>(n.get());
            dd->propertyName = info.dropName;
            if (info.hasDropItems) {
                dd->items.clear();
                for (const auto& [label, value] : info.dropItems)
                    dd->items.push_back({ label, value });
            }
            dd->selected = info.dropSelected;
        }

        // Capacitor — charge/discharge times from ct/dt attributes
        if (info.type == NodeType::Capacitor) {
            auto* cap = static_cast<CapacitorNode*>(n.get());
            cap->chargeTime    = info.capCharge;
            cap->dischargeTime = info.capDischarge;
        }

        // Memory Register — initial stored value from <r text="X"/>
        if (info.type == NodeType::MemoryRegister && info.hasLiteral) {
            MemoryRegisterNode* mem = static_cast<MemoryRegisterNode*>(n.get());
            mem->setStored(info.literalValue);
        }

        // Composite packer — set channel count
        if (info.type == NodeType::CompositePacker) {
            auto* packer = static_cast<CompositePackerNode*>(n.get());
            packer->setChannelCount(info.channelCount);
        }

        // Composite read number — set channel index
        if (info.type == NodeType::CompositeReadNumber) {
            auto* rn = static_cast<CompositeReadNumberNode*>(n.get());
            rn->channelIndex = info.channelIndex;
            n->name = QString("Comp. Read Num (ch%1)").arg(info.channelIndex);
        }

        // Composite read bool — set channel index
        if (info.type == NodeType::CompositeReadBool) {
            auto* rb = static_cast<CompositeReadBoolNode*>(n.get());
            rb->channelIndex = info.channelIndex;
            n->name = QString("Comp. Read Bool (ch%1)").arg(info.channelIndex);
        }

        // Composite write number — set channel count and start index,
        // then shift its parsed inN port indices by +1 since port 0 is the
        // Composite In port (the inN elements map to channels starting at
        // port 1, not port 0)
        if (info.type == NodeType::CompositeWriteNumber) {
            auto* wn = static_cast<CompositeWriteNumberNode*>(n.get());
            wn->setChannelCount(info.channelCount);
            wn->setStartIndex(info.startIndex);
            for (auto& in : info.inputs) in.portIdx += 1;
        }

        // Composite write bool — same treatment as write number
        if (info.type == NodeType::CompositeWriteBool) {
            auto* wb = static_cast<CompositeWriteBoolNode*>(n.get());
            wb->setChannelCount(info.channelCount);
            wb->setStartIndex(info.startIndex);
            for (auto& in : info.inputs) in.portIdx += 1;
        }

        nodes_[compId] = std::move(n);
    }

    // ── Step 4: build connections ─────────────────────────────────────────────
    for (auto& [targetId, info] : allNodes) {
        if (!nodes_.count(targetId)) continue;

        // Composite-in connection (<inc/>) always targets port 0 on write nodes
        if (info.compositeInSrc >= 0 && nodes_.count(info.compositeInSrc)) {
            Connection incConn;
            incConn.sourceNodeId  = info.compositeInSrc;
            incConn.sourcePortIdx = 0;
            incConn.targetNodeId  = targetId;
            incConn.targetPortIdx = 0;
            connections_.push_back(incConn);
        }

        for (const auto& in : info.inputs) {
            if (!nodes_.count(in.srcId)) continue;
            // Ignore an out-of-range node_index rather than wiring a bad port
            const auto& srcOutputs = nodes_.at(in.srcId)->outputs;
            int srcPort = (in.srcPort >= 0 && in.srcPort < (int)srcOutputs.size())
                          ? in.srcPort : 0;
            Connection conn;
            conn.sourceNodeId  = in.srcId;
            conn.sourcePortIdx = srcPort;
            conn.targetNodeId  = targetId;
            conn.targetPortIdx = in.portIdx;
            connections_.push_back(conn);
        }
    }

    int maxId = -1;
    for (const auto& [id, _] : nodes_)
        if (id > maxId) maxId = id;
    nextId_ = maxId + 1;

    buildEvalOrder();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// nodeTypeFromComponentType
// ─────────────────────────────────────────────────────────────────────────────
NodeType Microcontroller::nodeTypeFromComponentType(int id, bool& ok) const
{
    ok = true;
    switch (id) {
        // ── Logic ─────────────────────────────────────────────────────────────
        // NOT has no type attribute at all in real Stormworks files, which
        // our parser reads as the default sentinel -1 (see c.attribute call).
        case -1: return NodeType::Not;
        case  1: return NodeType::And;
        case  2: return NodeType::Or;
        case  3: return NodeType::Xor;
        case  4: return NodeType::Nand;
        case  5: return NodeType::Nor;

        // ── Math ──────────────────────────────────────────────────────────────
        case  6: return NodeType::Add;
        case  7: return NodeType::Subtract;
        case  8: return NodeType::Multiply;
        case  9: return NodeType::Divide;
        case 11: return NodeType::Clamp;
        case 13: return NodeType::MemoryRegister;
        case 12: return NodeType::Threshold;
        case 14: return NodeType::Abs;
        case 21: return NodeType::NumericalJunction;
        case 22: return NodeType::NumericalSwitchbox;
        case 35: return NodeType::Delta;
        case 38: return NodeType::Modulo;

        // ── Function evaluator ────────────────────────────────────────────────
        case 10: return NodeType::Function3;
        case 36: return NodeType::Function8;
        case 45: return NodeType::Function1;
        case 46: return NodeType::FunctionBool4;
        case 47: return NodeType::FunctionBool8;

        // ── Comparison ────────────────────────────────────────────────────────
        case 17: return NodeType::GreaterThan;
        case 18: return NodeType::LessThan;
        case 42: return NodeType::EqualTo;

        // ── Stateful ──────────────────────────────────────────────────────────
        case 37: return NodeType::UpDownCounter;

        // ── Timers ────────────────────────────────────────────────────────────
        case 49: return NodeType::TimerTON;
        case 50: return NodeType::TimerTOF;
        case 51: return NodeType::TimerRTO;
        case 52: return NodeType::TimerRTF;

        // ── PID Controllers ───────────────────────────────────────────────────
        case 23: return NodeType::PIDController;
        case 39: return NodeType::PIDControllerAdvanced;

        // ── Constants ─────────────────────────────────────────────────────────
        case 15: return NodeType::NumberLiteral;
        case 16: return NodeType::ConstantOn;
        case 20: return NodeType::PropertyDropdown;
        case 19: return NodeType::PropertySlider;
        case 33: return NodeType::PropertyToggle;
        case 34: return NodeType::PropertyNumber;
        case 43: return NodeType::TooltipNumber;
        case 44: return NodeType::TooltipOnOff;
        case 58: return NodeType::PropertyText;

        // ── Latches / capacitor ───────────────────────────────────────────────
        case 24: return NodeType::SRLatch;
        case 25: return NodeType::JKFlipFlop;
        case 26: return NodeType::Capacitor;

        // ── Pulse nodes ───────────────────────────────────────────────────────
        case 28: return NodeType::PushToToggle;
        case 48: return NodeType::ToggleToPush;
        case 27: return NodeType::Blinker;
        case 54: return NodeType::NumberToBinary;
        case 56: return NodeType::LuaScript;
        case 57: return NodeType::VideoSwitchbox;
        case 59: return NodeType::AudioSwitchbox;
        case 55: return NodeType::BinaryToNumber;

        // ── Composite ─────────────────────────────────────────────────────────
        case 29: return NodeType::CompositeReadBool;     // Composite Read (on/off)
        case 31: return NodeType::CompositeReadNumber;   // Composite Read (number)
        case 40: return NodeType::CompositeWriteNumber;  // Composite Write (number)
        case 41: return NodeType::CompositeWriteBool;    // Composite Write (on/off)
        case 53: return NodeType::CompositeSwitchbox;

        default:
            ok = false;
            return NodeType::And;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// componentTypeFromNodeType
//
// Reverse of nodeTypeFromComponentType, used when writing new <components>
// entries for nodes added during this session. Returns -1 for two distinct
// cases the caller must handle differently:
//   • NodeType::Not — real Stormworks files omit the type attribute entirely
//     for NOT gates, so the caller should skip setAttribute("type", ...)
//     rather than writing type="-1".
//   • I/O bridge types — these don't belong in <components> at all and are
//     never written as new entries here.
// Callers should check `type == NodeType::Not` separately if they need to
// distinguish "omit attribute" from "skip this node entirely".
// ─────────────────────────────────────────────────────────────────────────────
int Microcontroller::componentTypeFromNodeType(NodeType type) const
{
    switch (type) {
        case NodeType::And:      return 1;
        case NodeType::Or:       return 2;
        case NodeType::Xor:      return 3;
        case NodeType::Nand:     return 4;
        case NodeType::Nor:      return 5;
        // NodeType::Not intentionally has no case here — see header comment.

        case NodeType::Add:      return 6;
        case NodeType::Subtract: return 7;
        case NodeType::Multiply: return 8;
        case NodeType::Divide:   return 9;
        case NodeType::Clamp:    return 11;
        case NodeType::Abs:      return 14;
        case NodeType::Threshold:          return 12;
        case NodeType::NumericalJunction:  return 21;
        case NodeType::NumericalSwitchbox: return 22;
        case NodeType::Delta:    return 35;
        case NodeType::Modulo:   return 38;

        case NodeType::Function1:     return 45;
        case NodeType::Function3:     return 10;
        case NodeType::Function8:     return 36;
        case NodeType::FunctionBool4: return 46;
        case NodeType::FunctionBool8: return 47;
        case NodeType::MemoryRegister: return 13;

        case NodeType::GreaterThan: return 17;
        case NodeType::LessThan:    return 18;
        case NodeType::EqualTo:     return 42;
        case NodeType::UpDownCounter: return 37;
        case NodeType::TimerTON:      return 49;
        case NodeType::TimerTOF:      return 50;
        case NodeType::TimerRTO:      return 51;
        case NodeType::TimerRTF:              return 52;
        case NodeType::PIDController:         return 23;
        case NodeType::PIDControllerAdvanced: return 39;

        case NodeType::NumberLiteral:       return 15;
        case NodeType::ConstantOn:          return 16;
        case NodeType::PropertyDropdown:    return 20;
        case NodeType::PropertySlider:      return 19;
        case NodeType::PropertyToggle:      return 33;
        case NodeType::PropertyNumber:      return 34;
        case NodeType::TooltipNumber:       return 43;
        case NodeType::TooltipOnOff:        return 44;
        case NodeType::PropertyText:        return 58;
        case NodeType::SRLatch:             return 24;
        case NodeType::JKFlipFlop:          return 25;
        case NodeType::Capacitor:           return 26;
        case NodeType::Blinker:             return 27;
        case NodeType::NumberToBinary:      return 54;
        case NodeType::LuaScript:           return 56;
        case NodeType::VideoSwitchbox:      return 57;
        case NodeType::AudioSwitchbox:      return 59;
        case NodeType::BinaryToNumber:      return 55;
        case NodeType::PushToToggle:        return 28;
        case NodeType::ToggleToPush:        return 48;
        case NodeType::CompositeReadBool:    return 29;
        case NodeType::CompositeReadNumber:  return 31;
        case NodeType::CompositeWriteNumber: return 40;
        case NodeType::CompositeWriteBool:   return 41;
        case NodeType::CompositeSwitchbox:   return 53;

        default: return -1; // I/O types — not written as new <components>
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// saveToFile / saveToFileAs
// ─────────────────────────────────────────────────────────────────────────────
bool Microcontroller::saveToFile(QString& errorMsg)
{
    if (currentFilePath_.isEmpty()) {
        errorMsg = "No file path set — use Save As first.";
        return false;
    }
    return saveToFileAs(currentFilePath_, errorMsg);
}

bool Microcontroller::saveToFileAs(const QString& path, QString& errorMsg)
{
    // Work on a deep copy so a failed write doesn't corrupt our in-memory
    // original (cloneNode is the QDom way to deep-copy a document)
    QDomDocument doc;
    if (originalDoc_.isNull()) {
        // Nothing was loaded (a microcontroller built from scratch in the
        // editor): start from the same empty skeleton Stormworks writes for a
        // new microcontroller, then let rebuildXmlSections fill it in.
        const QString skeleton = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<microprocessor name=\"Microcontroller\" description=\"No description set.\" "
            "width=\"2\" length=\"2\" id_counter=\"0\" id_counter_node=\"0\">"
            "<nodes/>"
            "<group>"
            "<data><inputs/><outputs/></data>"
            "<components/><components_bridge/><groups/>"
            "<component_states/><component_bridge_states/><group_states/>"
            "</group>"
            "</microprocessor>");
        if (!doc.setContent(skeleton)) {
            errorMsg = "Could not create a new microcontroller file.";
            return false;
        }
        doc.documentElement().setAttribute("name", QFileInfo(path).completeBaseName());
    } else {
        doc = originalDoc_.cloneNode(true).toDocument();
    }

    rebuildXmlSections(doc);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Cannot open file for writing: ") + path;
        return false;
    }

    QTextStream out(&file);
    doc.save(out, 2); // 2-space indent, matches typical Stormworks formatting
    file.close();

    currentFilePath_ = path;
    // The just-written document becomes our new "original" baseline, so a
    // subsequent plain Save continues to diff against what's actually on disk
    originalDoc_ = doc;

    // Drop bookkeeping for nodes that no longer exist (their entries were just
    // removed from the file), then mark everything now present as original.
    for (auto it = originalBridgeIds_.begin(); it != originalBridgeIds_.end();)
        it = nodes_.count(it->first) ? std::next(it) : originalBridgeIds_.erase(it);
    for (auto it = originalComponentIds_.begin(); it != originalComponentIds_.end();)
        it = nodes_.count(it->first) ? std::next(it) : originalComponentIds_.erase(it);

    // Everything now present is on disk. Mark it as original so the next save
    // updates those entries in place instead of appending duplicates.
    for (const auto& [id, nodePtr] : nodes_) {
        switch (nodePtr->type) {
            case NodeType::BoolInput:
            case NodeType::BoolOutput:
            case NodeType::NumberInput:
            case NodeType::NumberOutput:
            case NodeType::CompositeInput:
            case NodeType::CompositeOutput:
            case NodeType::VideoInput:
            case NodeType::VideoOutput:
            case NodeType::AudioInput:
            case NodeType::AudioOutput:
                originalBridgeIds_[id] = true;
                break;
            default:
                if (nodePtr->type == NodeType::Not ||
                    componentTypeFromNodeType(nodePtr->type) >= 0)
                    originalComponentIds_[id] = true;
                break;
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildXmlSections
//
// Rewrites <group><components> and <group><components_bridge> in place:
//   • Existing entries (tracked in originalComponentIds_/originalBridgeIds_)
//     have their <pos> updated and their <inN> children rebuilt to match
//     current connections.
//   • New logic/math/composite nodes (added this session, not in
//     originalComponentIds_) get freshly synthesized <c><object> elements
//     appended to <components>.
//   • Logic/math/composite nodes that existed originally but were deleted
//     this session have their <c> element removed entirely.
//   • I/O bridge nodes (Bool/Number/Composite Input/Output) are handled in
//     <components_bridge> plus their <nodes> edge connector: edited in place
//     if they still exist, removed (both entries) if deleted, and appended
//     (both entries, in the first free board slot) if new.
// ─────────────────────────────────────────────────────────────────────────────
void Microcontroller::rebuildXmlSections(QDomDocument& doc) const
{
    QDomElement root = doc.documentElement();
    QDomElement group = root.firstChildElement("group");
    if (group.isNull()) return;

    // ── Helper: build a fresh <inN component_id="X"/> set of children on obj,
    // replacing whatever inN/inc elements it currently has, based on this
    // node's current incoming connections.
    //
    // Composite Write nodes (Number/Bool) are special-cased: their port 0 is
    // the Composite In connection, written as <inc component_id="X"/>, while
    // ports 1+ are the channel inputs, written as <in1>, <in2>, ... (shifted
    // down by one relative to our internal port indices).
    auto rewriteInputs = [&](QDomElement& obj, int nodeId) {
        // Remove all existing inN and inc elements
        QDomElement incOld = obj.firstChildElement("inc");
        if (!incOld.isNull()) obj.removeChild(incOld);
        for (int port = 1; port <= 32; ++port) {
            QDomElement inp = obj.firstChildElement(QString("in%1").arg(port));
            if (inp.isNull()) continue;
            obj.removeChild(inp);
        }

        Node* targetNode = node(nodeId);
        bool isCompositeWrite = targetNode &&
            (targetNode->type == NodeType::CompositeWriteNumber ||
             targetNode->type == NodeType::CompositeWriteBool);

        // Add elements per current connection targeting this node
        for (const auto& conn : connections_) {
            if (conn.targetNodeId != nodeId) continue;

            if (isCompositeWrite && conn.targetPortIdx == 0) {
                // Port 0 on a write node is the composite-in connection
                QDomElement incElem = doc.createElement("inc");
                incElem.setAttribute("component_id", conn.sourceNodeId);
                obj.appendChild(incElem);
            } else {
                // Regular channel input — shift write-node ports down by 1
                // so port 1 becomes <in1>, port 2 becomes <in2>, etc.
                int xmlPortNum = isCompositeWrite ? conn.targetPortIdx : conn.targetPortIdx + 1;
                QDomElement inElem = doc.createElement(QString("in%1").arg(xmlPortNum));
                inElem.setAttribute("component_id", conn.sourceNodeId);
                // Wires from a node's 2nd+ output carry node_index (omitted for the first)
                if (conn.sourcePortIdx > 0)
                    inElem.setAttribute("node_index", conn.sourcePortIdx);
                obj.appendChild(inElem);
            }
        }
    };

    // ── Helper: write the node's position into the <pos> child of obj ─────────
    // Inverse of the load-time mapping (offset, then divide by the grid scale),
    // rounded to Stormworks' 0.25 placement grid.
    auto writePos = [&](QDomElement& obj, int nodeId) {
        QDomElement pos = obj.firstChildElement("pos");
        if (pos.isNull()) {
            pos = doc.createElement("pos");
            obj.appendChild(pos);
        }

        double x = 0.0, y = 0.0;
        if (const Node* n = node(nodeId)) {
            bool isBridge = false;
            switch (n->type) {
                case NodeType::BoolInput:
                case NodeType::BoolOutput:
                case NodeType::NumberInput:
                case NodeType::NumberOutput:
                case NodeType::CompositeInput:
                case NodeType::CompositeOutput:
                case NodeType::VideoInput:
                case NodeType::VideoOutput:
                case NodeType::AudioInput:
                case NodeType::AudioOutput:
                    isBridge = true;
                    break;
                default: break;
            }
            const int offX = isBridge ? kBridgeOffsetX : kCompOffsetX;
            x = (n->posX - offX)    / double(kPositionScale);
            y = (n->posY - kOffsetY) / double(kPositionScale);
            x = std::round(x * 4.0) / 4.0;
            y = std::round(y * 4.0) / 4.0;
        }
        pos.setAttribute("x", QString::number(x));
        pos.setAttribute("y", QString::number(y));
    };

    // ── Helper: write a Threshold's bounds as <min text/> and <max text value/> ─
    auto writeThresholdBounds = [&](QDomElement& obj, const ThresholdNode* th) {
        QDomElement minElem = obj.firstChildElement("min");
        if (minElem.isNull()) { minElem = doc.createElement("min"); obj.appendChild(minElem); }
        minElem.setAttribute("text", QString::number(th->minBound));

        QDomElement maxElem = obj.firstChildElement("max");
        if (maxElem.isNull()) { maxElem = doc.createElement("max"); obj.appendChild(maxElem); }
        maxElem.setAttribute("text",  QString::number(th->maxBound));
        maxElem.setAttribute("value", QString::number(th->maxBound));
    };

    // ── Helper: write a Property/Tooltip node's attributes and child elements ──
    // Returns false for node types it doesn't handle.
    auto writePropertyData = [&](QDomElement& obj, const Node* n) -> bool {
        auto setTextValue = [&](const QString& tag, double v, bool withValue) {
            QDomElement el = obj.firstChildElement(tag);
            if (el.isNull()) { el = doc.createElement(tag); obj.appendChild(el); }
            el.setAttribute("text", QString::number(v));
            if (withValue) el.setAttribute("value", QString::number(v));
        };

        switch (n->type) {
            case NodeType::PropertyNumber: {
                auto* pn = static_cast<const PropertyNumberNode*>(n);
                obj.setAttribute("n", pn->propertyName);
                setTextValue("v", pn->value, false);
                return true;
            }
            case NodeType::PropertySlider: {
                auto* ps = static_cast<const PropertySliderNode*>(n);
                obj.setAttribute("n", ps->propertyName);
                setTextValue("min", ps->minValue, false);
                setTextValue("max", ps->maxValue, true);
                setTextValue("int", ps->step,     true);
                setTextValue("v",   ps->value,    false);
                return true;
            }
            case NodeType::PropertyToggle: {
                auto* pt = static_cast<const PropertyToggleNode*>(n);
                obj.setAttribute("n", pt->propertyName);
                if (pt->value) obj.setAttribute("v", "true");
                else           obj.removeAttribute("v");
                return true;
            }
            case NodeType::PropertyText:
                obj.setAttribute("n", static_cast<const PropertyTextNode*>(n)->propertyName);
                return true;
            case NodeType::TooltipNumber:
                obj.setAttribute("l", static_cast<const TooltipNumberNode*>(n)->tooltipLabel);
                return true;
            case NodeType::LuaScript: {
                // Script text lives in script="..." on <object>, and a copy sits
                // on the matching entry in <component_states>; keep both in sync
                const QString& script = static_cast<const LuaScriptNode*>(n)->script;
                const int compId = obj.attribute("id", "-1").toInt();
                auto apply = [&](QDomElement& el) {
                    if (script.isEmpty()) el.removeAttribute("script");
                    else                  el.setAttribute("script", script);
                };
                apply(obj);
                QDomElement states = group.firstChildElement("component_states");
                for (QDomElement s = states.firstChildElement(); !s.isNull();
                     s = s.nextSiblingElement()) {
                    if (s.attribute("id", "-1").toInt() == compId) apply(s);
                }
                return true;
            }
            case NodeType::Blinker: {
                auto* bl = static_cast<const BlinkerNode*>(n);
                obj.setAttribute("on",  QString::number(bl->onTime));
                obj.setAttribute("off", QString::number(bl->offTime));
                return true;
            }
            case NodeType::TooltipOnOff: {
                auto* tt = static_cast<const TooltipOnOffNode*>(n);
                obj.setAttribute("l",   tt->tooltipLabel);
                obj.setAttribute("on",  tt->onText);
                obj.setAttribute("off", tt->offText);
                return true;
            }
            default:
                return false;
        }
    };

    // ── Helper: write a Property Dropdown's name, selection and <items> ───────
    auto writeDropdown = [&](QDomElement& obj, const PropertyDropdownNode* dd) {
        obj.setAttribute("name", dd->propertyName);
        obj.setAttribute("i", dd->selected);

        QDomElement itemsElem = obj.firstChildElement("items");
        if (itemsElem.isNull()) {
            itemsElem = doc.createElement("items");
            obj.appendChild(itemsElem);
        }
        while (itemsElem.hasChildNodes())
            itemsElem.removeChild(itemsElem.firstChild());

        for (const auto& item : dd->items) {
            QDomElement iElem = doc.createElement("i");
            iElem.setAttribute("l", item.label);
            QDomElement vElem = doc.createElement("v");
            vElem.setAttribute("text",  QString::number(item.value));
            vElem.setAttribute("value", QString::number(item.value));
            iElem.appendChild(vElem);
            itemsElem.appendChild(iElem);
        }
    };

    // ── Step 1: update existing <components> entries, remove deleted ones ─────
    QDomElement comps = group.firstChildElement("components");
    if (!comps.isNull()) {
        QDomElement c = comps.firstChildElement();
        while (!c.isNull()) {
            QDomElement next = c.nextSiblingElement(); // grab before possible removal
            QDomElement obj = c.firstChildElement("object");
            if (!obj.isNull()) {
                int compId = obj.attribute("id", "-1").toInt();
                if (originalComponentIds_.count(compId)) {
                    if (nodes_.count(compId)) {
                        // Still present — update in place
                        writePos(obj, compId);
                        rewriteInputs(obj, compId);

                        // Sync channel count for write nodes in case it was
                        // changed via the settings dialog this session
                        Node* liveNode = nodes_.at(compId).get();
                        if (liveNode->type == NodeType::CompositeWriteNumber) {
                            auto* wn = static_cast<CompositeWriteNumberNode*>(liveNode);
                            obj.setAttribute("count", wn->channelCount);
                            obj.setAttribute("start_channel", wn->startIndex);
                        } else if (liveNode->type == NodeType::CompositeWriteBool) {
                            auto* wb = static_cast<CompositeWriteBoolNode*>(liveNode);
                            obj.setAttribute("count", wb->channelCount);
                            obj.setAttribute("start_channel", wb->startIndex);
                        } else if (liveNode->type == NodeType::CompositePacker) {
                            auto* pk = static_cast<CompositePackerNode*>(liveNode);
                            obj.setAttribute("count", pk->channelCount);
                        } else if (liveNode->type == NodeType::CompositeReadNumber) {
                            auto* rn = static_cast<CompositeReadNumberNode*>(liveNode);
                            obj.setAttribute("channel", rn->channelIndex);
                        } else if (liveNode->type == NodeType::CompositeReadBool) {
                            auto* rb = static_cast<CompositeReadBoolNode*>(liveNode);
                            obj.setAttribute("channel", rb->channelIndex);
                        } else if (liveNode->type == NodeType::EqualTo) {
                            auto* eq = static_cast<EqualToNode*>(liveNode);
                            QDomElement eElem = obj.firstChildElement("e");
                            if (eElem.isNull()) { eElem = doc.createElement("e"); obj.appendChild(eElem); }
                            eElem.setAttribute("text", QString::number(eq->tolerance));
                            eElem.setAttribute("value", QString::number(eq->tolerance));
                        } else if (liveNode->type == NodeType::Function1 ||
                                   liveNode->type == NodeType::Function3 ||
                                   liveNode->type == NodeType::Function8 ||
                                   liveNode->type == NodeType::FunctionBool4 ||
                                   liveNode->type == NodeType::FunctionBool8) {
                            FunctionNodeBase* fn = static_cast<FunctionNodeBase*>(liveNode);
                            if (!fn->expression.isEmpty())
                                obj.setAttribute("e", fn->expression);
                            else
                                obj.removeAttribute("e");
                        } else if (writePropertyData(obj, liveNode)) {
                            // property / tooltip node — attributes written above
                        } else if (liveNode->type == NodeType::Threshold) {
                            writeThresholdBounds(obj, static_cast<ThresholdNode*>(liveNode));
                        } else if (liveNode->type == NodeType::PropertyDropdown) {
                            writeDropdown(obj, static_cast<PropertyDropdownNode*>(liveNode));
                        } else if (liveNode->type == NodeType::Capacitor) {
                            auto* cap = static_cast<CapacitorNode*>(liveNode);
                            obj.setAttribute("ct", QString::number(cap->chargeTime));
                            obj.setAttribute("dt", QString::number(cap->dischargeTime));
                        } else if (liveNode->type == NodeType::PIDController) {
                            PIDControllerNode* pid = static_cast<PIDControllerNode*>(liveNode);
                            auto writeGainElem = [&](const QString& tag, double gainVal) {
                                QDomElement el = obj.firstChildElement(tag);
                                if (el.isNull()) { el = doc.createElement(tag); obj.appendChild(el); }
                                el.setAttribute("text", QString::number(gainVal));
                            };
                            writeGainElem("kp", pid->kp);
                            writeGainElem("ki", pid->ki);
                            writeGainElem("kd", pid->kd);
                        } else if (liveNode->type == NodeType::UpDownCounter) {
                            UpDownCounterNode* ctr = static_cast<UpDownCounterNode*>(liveNode);
                            QDomElement rElem = obj.firstChildElement("r");
                            if (rElem.isNull()) { rElem = doc.createElement("r"); obj.appendChild(rElem); }
                            rElem.setAttribute("text", QString::number(ctr->resetValue));
                            QDomElement iElem = obj.firstChildElement("i");
                            if (iElem.isNull()) { iElem = doc.createElement("i"); obj.appendChild(iElem); }
                            iElem.setAttribute("text", QString::number(ctr->step));
                            iElem.setAttribute("value", QString::number(ctr->step));
                            QDomElement minElem = obj.firstChildElement("min");
                            if (minElem.isNull()) { minElem = doc.createElement("min"); obj.appendChild(minElem); }
                            minElem.setAttribute("text", QString::number(ctr->clampEnabled ? ctr->minVal : 0.0));
                            QDomElement maxElem = obj.firstChildElement("max");
                            if (maxElem.isNull()) { maxElem = doc.createElement("max"); obj.appendChild(maxElem); }
                            maxElem.setAttribute("text", QString::number(ctr->clampEnabled ? ctr->maxVal : 0.0));
                        }
                    } else {
                        // Existed originally but was deleted this session
                        comps.removeChild(c);
                    }
                }
                // Entries not in originalComponentIds_ (shouldn't normally
                // happen here since this loop only sees original entries on
                // a freshly cloned original doc) are left untouched.
            }
            c = next;
        }

        // ── Step 1b: append brand-new logic/math/composite nodes ──────────────
        for (const auto& [id, nodePtr] : nodes_) {
            if (originalComponentIds_.count(id)) continue; // already handled above
            if (originalBridgeIds_.count(id)) continue;    // I/O node, never appended here

            // NOT is the one type that's written without any type attribute
            // at all in real Stormworks files, so it needs special handling
            // here rather than going through the normal compType < 0 skip.
            bool isNot = (nodePtr->type == NodeType::Not);

            int compType = componentTypeFromNodeType(nodePtr->type);
            if (compType < 0 && !isNot) continue; // I/O-only type — no <components> representation

            QDomElement newC = doc.createElement("c");
            if (!isNot)
                newC.setAttribute("type", QString::number(compType));
            // else: leave the type attribute unset entirely, matching how
            // real Stormworks NOT gates are written

            QDomElement newObj = doc.createElement("object");
            newObj.setAttribute("id", id);
            newC.appendChild(newObj);

            writePos(newObj, id);
            rewriteInputs(newObj, id);

            // Preserve type-specific extra attributes/children where relevant
            if (nodePtr->type == NodeType::NumberLiteral) {
                auto* lit = static_cast<NumberLiteralNode*>(nodePtr.get());
                QDomElement nElem = doc.createElement("n");
                nElem.setAttribute("text", QString::number(lit->constantValue));
                nElem.setAttribute("value", QString::number(lit->constantValue));
                newObj.appendChild(nElem);
            }
            if (nodePtr->type == NodeType::Function1 || nodePtr->type == NodeType::Function3 ||
                nodePtr->type == NodeType::Function8  || nodePtr->type == NodeType::FunctionBool4 ||
                nodePtr->type == NodeType::FunctionBool8) {
                FunctionNodeBase* fn = static_cast<FunctionNodeBase*>(nodePtr.get());
                if (!fn->expression.isEmpty())
                    newObj.setAttribute("e", fn->expression);
            }
            writePropertyData(newObj, nodePtr.get());
            if (nodePtr->type == NodeType::Threshold)
                writeThresholdBounds(newObj, static_cast<ThresholdNode*>(nodePtr.get()));
            if (nodePtr->type == NodeType::PropertyDropdown)
                writeDropdown(newObj, static_cast<PropertyDropdownNode*>(nodePtr.get()));
            if (nodePtr->type == NodeType::Capacitor) {
                auto* cap = static_cast<CapacitorNode*>(nodePtr.get());
                newObj.setAttribute("ct", QString::number(cap->chargeTime));
                newObj.setAttribute("dt", QString::number(cap->dischargeTime));
            }
            if (nodePtr->type == NodeType::EqualTo) {
                auto* eq = static_cast<EqualToNode*>(nodePtr.get());
                QDomElement eElem = doc.createElement("e");
                eElem.setAttribute("text", QString::number(eq->tolerance));
                eElem.setAttribute("value", QString::number(eq->tolerance));
                newObj.appendChild(eElem);
            }
            if (nodePtr->type == NodeType::PIDController) {
                auto* pid = static_cast<PIDControllerNode*>(nodePtr.get());
                QDomElement kpElem = doc.createElement("kp");
                kpElem.setAttribute("text", QString::number(pid->kp));
                newObj.appendChild(kpElem);
                QDomElement kiElem = doc.createElement("ki");
                kiElem.setAttribute("text", QString::number(pid->ki));
                newObj.appendChild(kiElem);
                QDomElement kdElem = doc.createElement("kd");
                kdElem.setAttribute("text", QString::number(pid->kd));
                newObj.appendChild(kdElem);
            }
            if (nodePtr->type == NodeType::UpDownCounter) {
                auto* ctr = static_cast<UpDownCounterNode*>(nodePtr.get());
                QDomElement rElem = doc.createElement("r");
                rElem.setAttribute("text", QString::number(ctr->resetValue));
                newObj.appendChild(rElem);
                QDomElement iElem = doc.createElement("i");
                iElem.setAttribute("text", QString::number(ctr->step));
                iElem.setAttribute("value", QString::number(ctr->step));
                newObj.appendChild(iElem);
                QDomElement minElem = doc.createElement("min");
                minElem.setAttribute("text", QString::number(ctr->clampEnabled ? ctr->minVal : 0.0));
                newObj.appendChild(minElem);
                QDomElement maxElem = doc.createElement("max");
                maxElem.setAttribute("text", QString::number(ctr->clampEnabled ? ctr->maxVal : 0.0));
                newObj.appendChild(maxElem);
            }
            if (nodePtr->type == NodeType::MemoryRegister) {
                // <r text="0"/> stores the initial/reset value
                QDomElement rElem = doc.createElement("r");
                rElem.setAttribute("text", "0");
                newObj.appendChild(rElem);
            }
            if (nodePtr->type == NodeType::CompositeReadNumber) {
                auto* rn = static_cast<CompositeReadNumberNode*>(nodePtr.get());
                newObj.setAttribute("channel", rn->channelIndex);
            }
            if (nodePtr->type == NodeType::CompositeReadBool) {
                auto* rb = static_cast<CompositeReadBoolNode*>(nodePtr.get());
                newObj.setAttribute("channel", rb->channelIndex);
            }
            if (nodePtr->type == NodeType::CompositeWriteNumber) {
                auto* wn = static_cast<CompositeWriteNumberNode*>(nodePtr.get());
                newObj.setAttribute("count", wn->channelCount);
                newObj.setAttribute("start_channel", wn->startIndex);
            }
            if (nodePtr->type == NodeType::CompositeWriteBool) {
                auto* wb = static_cast<CompositeWriteBoolNode*>(nodePtr.get());
                newObj.setAttribute("count", wb->channelCount);
                newObj.setAttribute("start_channel", wb->startIndex);
            }

            comps.appendChild(newC);
        }
    }

    // ── Step 2: update existing <components_bridge> entries, remove deleted ──
    // A deleted I/O node needs both its bridge <c> and its edge <n> connector
    // (in <nodes>, matched by component_id) removed.
    std::set<int> deletedBridgeIds;
    QDomElement compBridge = group.firstChildElement("components_bridge");
    if (!compBridge.isNull()) {
        QDomElement c = compBridge.firstChildElement();
        while (!c.isNull()) {
            QDomElement next = c.nextSiblingElement();
            QDomElement obj = c.firstChildElement("object");
            if (!obj.isNull()) {
                int compId = obj.attribute("id", "-1").toInt();
                if (originalBridgeIds_.count(compId)) {
                    if (nodes_.count(compId)) {
                        writePos(obj, compId);
                        rewriteInputs(obj, compId);
                        // Label edits go to the matching edge connector in <nodes>
                        const QString& label = nodes_.at(compId)->label;
                        if (!label.isEmpty()) {
                            QDomElement nodesEl = root.firstChildElement("nodes");
                            for (QDomElement n = nodesEl.firstChildElement("n"); !n.isNull();
                                 n = n.nextSiblingElement("n")) {
                                if (n.attribute("component_id", "-1").toInt() == compId) {
                                    n.firstChildElement("node").setAttribute("label", label);
                                    break;
                                }
                            }
                        }
                    } else {
                        deletedBridgeIds.insert(compId);
                        compBridge.removeChild(c);
                    }
                }
                // New I/O nodes are added in step 3.
            }
            c = next;
        }
    }
    if (!deletedBridgeIds.empty()) {
        QDomElement nodesEl = root.firstChildElement("nodes");
        QDomElement n = nodesEl.firstChildElement("n");
        while (!n.isNull()) {
            QDomElement nextN = n.nextSiblingElement("n");
            if (deletedBridgeIds.count(n.attribute("component_id", "-1").toInt()))
                nodesEl.removeChild(n);
            n = nextN;
        }
    }

    // ── Step 3: append brand-new I/O nodes (bridge components + edge nodes) ───
    // Each new Bool/Number/Composite Input/Output needs two entries:
    //   • <components_bridge><c type=..><object id=..>   (the component)
    //   • <nodes><n id=.. component_id=..><node label mode type ...>
    //     (the connector on the microcontroller's edge, at an (x, z) slot)
    // Formats taken from real saved files; see bridgeSpec below.
    struct BridgeSpec {
        const char* compType; // <c type=..> in components_bridge; null = no attribute
        bool        isInput;  // <node mode="1">
        const char* nodeType; // <node type=..>; null = no attribute (bool)
    };
    auto bridgeSpec = [](NodeType t, BridgeSpec& out) -> bool {
        switch (t) {
            case NodeType::BoolInput:       out = { nullptr, true,  nullptr }; return true;
            case NodeType::BoolOutput:      out = { "1",     false, nullptr }; return true;
            case NodeType::NumberInput:     out = { "2",     true,  "1"     }; return true;
            case NodeType::NumberOutput:    out = { "3",     false, "1"     }; return true;
            case NodeType::CompositeInput:  out = { "4",     true,  "5"     }; return true;
            case NodeType::CompositeOutput: out = { "5",     false, "5"     }; return true;
            case NodeType::VideoInput:      out = { "6",     true,  "6"     }; return true;
            case NodeType::VideoOutput:     out = { "7",     false, "6"     }; return true;
            case NodeType::AudioInput:      out = { "8",     true,  "7"     }; return true;
            case NodeType::AudioOutput:     out = { "9",     false, "7"     }; return true;
            default: return false;
        }
    };

    QDomElement nodesElem = root.firstChildElement("nodes");
    if (nodesElem.isNull()) {
        nodesElem = doc.createElement("nodes");
        root.insertBefore(nodesElem, group);
    }
    QDomElement bridgeElem = group.firstChildElement("components_bridge");
    if (bridgeElem.isNull()) {
        bridgeElem = doc.createElement("components_bridge");
        group.appendChild(bridgeElem);
    }

    int boardW = std::max(1, root.attribute("width",  "1").toInt());
    int boardL = std::max(1, root.attribute("length", "1").toInt());

    // Edge-node ids: id_counter_node holds the last id used
    int lastNodeId = root.attribute("id_counter_node", "0").toInt();
    std::set<std::pair<int,int>> usedSlots;
    for (QDomElement n = nodesElem.firstChildElement("n"); !n.isNull();
         n = n.nextSiblingElement("n")) {
        lastNodeId = std::max(lastNodeId, n.attribute("id", "0").toInt());
        QDomElement pos = n.firstChildElement("node").firstChildElement("position");
        usedSlots.insert({ pos.attribute("x", "0").toInt(), pos.attribute("z", "0").toInt() });
    }

    // First free (x, z) slot, row by row; adds a row to the board when full
    auto takeSlot = [&]() -> std::pair<int,int> {
        for (int z = 0; z < boardL; ++z)
            for (int x = 0; x < boardW; ++x)
                if (!usedSlots.count({ x, z })) { usedSlots.insert({ x, z }); return { x, z }; }
        std::pair<int,int> slot{ 0, boardL };
        ++boardL;
        usedSlots.insert(slot);
        return slot;
    };

    // Deterministic order: by component id
    std::vector<int> newBridgeIds;
    for (const auto& [id, nodePtr] : nodes_) {
        BridgeSpec spec;
        if (bridgeSpec(nodePtr->type, spec) && !originalBridgeIds_.count(id))
            newBridgeIds.push_back(id);
    }
    std::sort(newBridgeIds.begin(), newBridgeIds.end());

    for (int id : newBridgeIds) {
        const Node* nodePtr = nodes_.at(id).get();
        BridgeSpec spec;
        bridgeSpec(nodePtr->type, spec);

        // <components_bridge> entry
        QDomElement c = doc.createElement("c");
        if (spec.compType) c.setAttribute("type", spec.compType);
        QDomElement obj = doc.createElement("object");
        obj.setAttribute("id", id);
        c.appendChild(obj);
        writePos(obj, id);
        rewriteInputs(obj, id);
        bridgeElem.appendChild(c);

        // <nodes> entry (edge connector)
        const auto [slotX, slotZ] = takeSlot();
        QDomElement n = doc.createElement("n");
        n.setAttribute("id", ++lastNodeId);
        n.setAttribute("component_id", id);
        QDomElement nd = doc.createElement("node");
        nd.setAttribute("label", !nodePtr->label.isEmpty() ? nodePtr->label
                                 : QString(spec.isInput ? "Input" : "Output"));
        if (spec.isInput) nd.setAttribute("mode", "1");
        if (spec.nodeType) nd.setAttribute("type", spec.nodeType);
        nd.setAttribute("description", "The input signal to be processed.");
        if (slotX != 0 || slotZ != 0) {
            QDomElement pos = doc.createElement("position");
            if (slotX != 0) pos.setAttribute("x", slotX);
            if (slotZ != 0) pos.setAttribute("z", slotZ);
            nd.appendChild(pos);
        }
        n.appendChild(nd);
        nodesElem.appendChild(n);
    }

    if (!newBridgeIds.empty()) {
        root.setAttribute("length", boardL); // may have grown to fit new connectors
        root.setAttribute("id_counter_node", lastNodeId);
    }

    // id_counter holds the highest component id in use; keep it ahead of every
    // node id this session created so the game never reuses one
    int maxCompId = root.attribute("id_counter", "0").toInt();
    for (const auto& [id, _] : nodes_) maxCompId = std::max(maxCompId, id);
    root.setAttribute("id_counter", maxCompId);
}
