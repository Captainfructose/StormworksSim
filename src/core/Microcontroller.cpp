#include "Microcontroller.h"
#include "nodes/IONodes.h"
#include "nodes/CompositeNodes.h"
#include "nodes/MathNodes.h"
#include "nodes/StatefulNodes.h"
#include "nodes/FunctionNodes.h"

#include <QFile>
#include <QTextStream>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QDebug>

#include <queue>
#include <unordered_set>
#include <algorithm>

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
// loadFromFile
// ─────────────────────────────────────────────────────────────────────────────
bool Microcontroller::loadFromFile(const QString& path, QString& errorMsg)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QStringLiteral("Cannot open file: ") + path;
        return false;
    }

    QDomDocument doc;
    QString parseError;
    int line = 0, col = 0;
    if (!doc.setContent(&file, &parseError, &line, &col)) {
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
        std::vector<std::pair<int,int>> inputs; // (portIdx, srcComponentId)
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
    };

    std::unordered_map<int, NodeInfo> allNodes;

    // ── Helper: parse a single <object> element ───────────────────────────────
    // Position scale: Stormworks XML units are converted to pixels using our
    // grid cell size (80px) so loaded nodes land exactly on grid lines without
    // relying on NodeGraphView's snap-to-grid to silently correct the offset.
    constexpr int kPositionScale = 40; // matches the 40px grid cell size
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

        // Inputs: <in1 component_id="X"/>, <in2 .../> etc.
        for (int port = 1; port <= 32; ++port) {
            QDomElement inp = obj.firstChildElement(QString("in%1").arg(port));
            if (inp.isNull()) break;
            if (inp.hasAttribute("component_id")) {
                int srcId = inp.attribute("component_id").toInt();
                info.inputs.push_back({port - 1, srcId});
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
            else                     ntype = NodeType::BoolInput;      // no type = bool input (source)

            QDomElement obj = c.firstChildElement("object");
            if (obj.isNull()) continue;

            int compId = obj.attribute("id", "-1").toInt();
            NodeInfo info;
            info.type = ntype;
            parseObject(obj, info);
            info.px += 400; // 10 grid cells right
            info.py += 120; // 3 grid cells down
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
            info.px += 40;  // 1 grid cell right
            info.py += 120; // 3 grid cells down
            allNodes[compId] = std::move(info);
            originalComponentIds_[compId] = true;
        }
    }

    // ── Step 3: create Node objects ───────────────────────────────────────────
    for (auto& [compId, info] : allNodes) {
        auto n = Node::create(info.type);
        if (!n) continue;
        n->id   = compId;
        n->posX = info.px;
        n->posY = info.py;

        // Number literal — set constant value
        if (info.type == NodeType::NumberLiteral && info.hasLiteral) {
            auto* lit = static_cast<NumberLiteralNode*>(n.get());
            lit->constantValue = info.literalValue;
            n->name = QString("= %1").arg(info.literalValue);
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
            for (auto& [portIdx, srcId] : info.inputs) portIdx += 1;
        }

        // Composite write bool — same treatment as write number
        if (info.type == NodeType::CompositeWriteBool) {
            auto* wb = static_cast<CompositeWriteBoolNode*>(n.get());
            wb->setChannelCount(info.channelCount);
            wb->setStartIndex(info.startIndex);
            for (auto& [portIdx, srcId] : info.inputs) portIdx += 1;
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

        for (auto& [portIdx, srcId] : info.inputs) {
            if (!nodes_.count(srcId)) continue;
            Connection conn;
            conn.sourceNodeId  = srcId;
            conn.sourcePortIdx = 0;
            conn.targetNodeId  = targetId;
            conn.targetPortIdx = portIdx;
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
        case 14: return NodeType::Abs;
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

        // ── Composite ─────────────────────────────────────────────────────────
        case 29: return NodeType::CompositeReadBool;     // Composite Read (on/off)
        case 31: return NodeType::CompositeReadNumber;   // Composite Read (number)
        case 40: return NodeType::CompositeWriteNumber;  // Composite Write (number)
        case 41: return NodeType::CompositeWriteBool;    // Composite Write (on/off)

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
        case NodeType::CompositeReadBool:    return 29;
        case NodeType::CompositeReadNumber:  return 31;
        case NodeType::CompositeWriteNumber: return 40;
        case NodeType::CompositeWriteBool:   return 41;

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
    if (originalDoc_.isNull()) {
        errorMsg = "No microcontroller loaded to save.";
        return false;
    }

    // Work on a deep copy so a failed write doesn't corrupt our in-memory
    // original (cloneNode is the QDom way to deep-copy a document)
    QDomDocument doc = originalDoc_.cloneNode(true).toDocument();

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

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildXmlSections
//
// Rewrites <group><components> and <group><components_bridge> in place:
//   • Existing entries (tracked in originalComponentIds_/originalBridgeIds_)
//     have their <pos> zeroed and their <inN> children rebuilt to match
//     current connections.
//   • New logic/math/composite nodes (added this session, not in
//     originalComponentIds_) get freshly synthesized <c><object> elements
//     appended to <components>.
//   • Logic/math/composite nodes that existed originally but were deleted
//     this session have their <c> element removed entirely.
//   • I/O bridge nodes (Bool/Number Input/Output, CompositeInput) are only
//     ever edited in place — new ones are never written, and deleted ones
//     are left untouched in the DOM (per the Option 2 safety design), since
//     synthesizing/removing their matching <nodes> vehicle-edge entries
//     isn't supported.
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
                obj.appendChild(inElem);
            }
        }
    };

    // ── Helper: zero out (or create) the <pos> child of obj ───────────────────
    auto zeroPos = [&](QDomElement& obj) {
        QDomElement pos = obj.firstChildElement("pos");
        if (pos.isNull()) {
            pos = doc.createElement("pos");
            obj.appendChild(pos);
        }
        pos.setAttribute("x", "0");
        pos.setAttribute("y", "0");
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
                        zeroPos(obj);
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

            zeroPos(newObj);
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

    // ── Step 2: update existing <components_bridge> entries (edit only) ──────
    QDomElement compBridge = group.firstChildElement("components_bridge");
    if (!compBridge.isNull()) {
        QDomElement c = compBridge.firstChildElement();
        while (!c.isNull()) {
            QDomElement next = c.nextSiblingElement();
            QDomElement obj = c.firstChildElement("object");
            if (!obj.isNull()) {
                int compId = obj.attribute("id", "-1").toInt();
                if (originalBridgeIds_.count(compId) && nodes_.count(compId)) {
                    zeroPos(obj);
                    rewriteInputs(obj, compId);
                }
                // Deleted or newly-added I/O nodes are intentionally left
                // untouched here — see function header comment.
            }
            c = next;
        }
    }
}
