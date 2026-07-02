#include "Node.h"
#include "nodes/LogicNodes.h"
#include "nodes/MathNodes.h"
#include "nodes/IONodes.h"
#include "nodes/CompositeNodes.h"
#include "nodes/StatefulNodes.h"

std::unique_ptr<Node> Node::create(NodeType type)
{
    switch (type) {
        case NodeType::BoolInput:           return std::make_unique<BoolInputNode>();
        case NodeType::BoolOutput:          return std::make_unique<BoolOutputNode>();
        case NodeType::NumberInput:         return std::make_unique<NumberInputNode>();
        case NodeType::NumberOutput:        return std::make_unique<NumberOutputNode>();
        case NodeType::NumberLiteral:       return std::make_unique<NumberLiteralNode>();
        case NodeType::CompositeInput:      return std::make_unique<CompositeInputNode>();
        case NodeType::CompositePacker:     return std::make_unique<CompositePackerNode>();
        case NodeType::CompositeReadNumber: return std::make_unique<CompositeReadNumberNode>();
        case NodeType::CompositeReadBool:   return std::make_unique<CompositeReadBoolNode>();
        case NodeType::CompositeWriteNumber: return std::make_unique<CompositeWriteNumberNode>();
        case NodeType::CompositeWriteBool:   return std::make_unique<CompositeWriteBoolNode>();
        case NodeType::And:                 return std::make_unique<AndNode>();
        case NodeType::Or:                  return std::make_unique<OrNode>();
        case NodeType::Not:                 return std::make_unique<NotNode>();
        case NodeType::Xor:                 return std::make_unique<XorNode>();
        case NodeType::Nand:                return std::make_unique<NandNode>();
        case NodeType::Nor:                 return std::make_unique<NorNode>();
        case NodeType::Add:                 return std::make_unique<AddNode>();
        case NodeType::Subtract:            return std::make_unique<SubtractNode>();
        case NodeType::Multiply:            return std::make_unique<MultiplyNode>();
        case NodeType::Divide:              return std::make_unique<DivideNode>();
        case NodeType::Clamp:               return std::make_unique<ClampNode>();
        case NodeType::Abs:                 return std::make_unique<AbsNode>();
        case NodeType::Min:                 return std::make_unique<MinNode>();
        case NodeType::Max:                 return std::make_unique<MaxNode>();
        case NodeType::Modulo:              return std::make_unique<ModuloNode>();
        case NodeType::GreaterThan:         return std::make_unique<GreaterThanNode>();
        case NodeType::LessThan:            return std::make_unique<LessThanNode>();
        case NodeType::EqualTo:             return std::make_unique<EqualToNode>();
        case NodeType::UpDownCounter:       return std::make_unique<UpDownCounterNode>();
        case NodeType::TimerTON:            return std::make_unique<TimerTONNode>();
        case NodeType::TimerTOF:            return std::make_unique<TimerTOFNode>();
        case NodeType::TimerRTO:            return std::make_unique<TimerRTONode>();
        case NodeType::TimerRTF:            return std::make_unique<TimerRTFNode>();
        case NodeType::PIDController:       return std::make_unique<PIDControllerNode>();
        case NodeType::PIDControllerAdvanced: return std::make_unique<PIDControllerAdvancedNode>();
        default: return nullptr;
    }
}
