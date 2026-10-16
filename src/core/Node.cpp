#include "Node.h"
#include "nodes/LogicNodes.h"
#include "nodes/MathNodes.h"
#include "nodes/IONodes.h"
#include "nodes/CompositeNodes.h"
#include "nodes/StatefulNodes.h"
#include "nodes/FunctionNodes.h"

std::unique_ptr<Node> Node::create(NodeType type)
{
    switch (type) {
        // ── I/O ──────────────────────────────────────────────────────────────
        case NodeType::BoolInput:           return std::make_unique<BoolInputNode>();
        case NodeType::BoolOutput:          return std::make_unique<BoolOutputNode>();
        case NodeType::NumberInput:         return std::make_unique<NumberInputNode>();
        case NodeType::NumberOutput:        return std::make_unique<NumberOutputNode>();
        case NodeType::NumberLiteral:       return std::make_unique<NumberLiteralNode>();
        case NodeType::PropertyDropdown:    return std::make_unique<PropertyDropdownNode>();
        case NodeType::PropertyNumber:      return std::make_unique<PropertyNumberNode>();
        case NodeType::PropertySlider:      return std::make_unique<PropertySliderNode>();
        case NodeType::PropertyToggle:      return std::make_unique<PropertyToggleNode>();
        case NodeType::PropertyText:        return std::make_unique<PropertyTextNode>();
        case NodeType::TooltipNumber:       return std::make_unique<TooltipNumberNode>();
        case NodeType::TooltipOnOff:        return std::make_unique<TooltipOnOffNode>();

        // ── Composite ─────────────────────────────────────────────────────────
        case NodeType::CompositeInput:       return std::make_unique<CompositeInputNode>();
        case NodeType::CompositeOutput:      return std::make_unique<CompositeOutputNode>();
        case NodeType::VideoInput:           return std::make_unique<VideoInputNode>();
        case NodeType::VideoOutput:          return std::make_unique<VideoOutputNode>();
        case NodeType::VideoSwitchbox:       return std::make_unique<VideoSwitchboxNode>();
        case NodeType::AudioInput:           return std::make_unique<AudioInputNode>();
        case NodeType::AudioOutput:          return std::make_unique<AudioOutputNode>();
        case NodeType::AudioSwitchbox:       return std::make_unique<AudioSwitchboxNode>();
        case NodeType::CompositePacker:      return std::make_unique<CompositePackerNode>();
        case NodeType::CompositeReadNumber:  return std::make_unique<CompositeReadNumberNode>();
        case NodeType::CompositeReadBool:    return std::make_unique<CompositeReadBoolNode>();
        case NodeType::CompositeWriteNumber: return std::make_unique<CompositeWriteNumberNode>();
        case NodeType::CompositeWriteBool:   return std::make_unique<CompositeWriteBoolNode>();
        case NodeType::CompositeSwitchbox:   return std::make_unique<CompositeSwitchboxNode>();
        case NodeType::LuaScript:            return std::make_unique<LuaScriptNode>();
        case NodeType::BinaryToNumber:       return std::make_unique<BinaryToNumberNode>();
        case NodeType::NumberToBinary:       return std::make_unique<NumberToBinaryNode>();

        // ── Logic ─────────────────────────────────────────────────────────────
        case NodeType::And:                 return std::make_unique<AndNode>();
        case NodeType::Or:                  return std::make_unique<OrNode>();
        case NodeType::Not:                 return std::make_unique<NotNode>();
        case NodeType::Xor:                 return std::make_unique<XorNode>();
        case NodeType::Nand:                return std::make_unique<NandNode>();
        case NodeType::Nor:                 return std::make_unique<NorNode>();
        case NodeType::ConstantOn:          return std::make_unique<ConstantOnNode>();
        case NodeType::PushToToggle:        return std::make_unique<PushToToggleNode>();
        case NodeType::ToggleToPush:        return std::make_unique<ToggleToPushNode>();
        case NodeType::SRLatch:             return std::make_unique<SRLatchNode>();
        case NodeType::JKFlipFlop:          return std::make_unique<JKFlipFlopNode>();
        case NodeType::Capacitor:           return std::make_unique<CapacitorNode>();

        // ── Math ──────────────────────────────────────────────────────────────
        case NodeType::Add:                 return std::make_unique<AddNode>();
        case NodeType::Subtract:            return std::make_unique<SubtractNode>();
        case NodeType::Multiply:            return std::make_unique<MultiplyNode>();
        case NodeType::Divide:              return std::make_unique<DivideNode>();
        case NodeType::Clamp:               return std::make_unique<ClampNode>();
        case NodeType::Abs:                 return std::make_unique<AbsNode>();
        case NodeType::Modulo:              return std::make_unique<ModuloNode>();
        case NodeType::Delta:               return std::make_unique<DeltaNode>();
        case NodeType::Threshold:           return std::make_unique<ThresholdNode>();
        case NodeType::NumericalSwitchbox:  return std::make_unique<NumericalSwitchboxNode>();
        case NodeType::NumericalJunction:   return std::make_unique<NumericalJunctionNode>();
        case NodeType::GreaterThan:         return std::make_unique<GreaterThanNode>();
        case NodeType::LessThan:            return std::make_unique<LessThanNode>();
        case NodeType::EqualTo:             return std::make_unique<EqualToNode>();

        // ── Stateful ──────────────────────────────────────────────────────────
        case NodeType::UpDownCounter:         return std::make_unique<UpDownCounterNode>();
        case NodeType::MemoryRegister:        return std::make_unique<MemoryRegisterNode>();
        case NodeType::Blinker:               return std::make_unique<BlinkerNode>();
        case NodeType::TimerTON:              return std::make_unique<TimerTONNode>();
        case NodeType::TimerTOF:              return std::make_unique<TimerTOFNode>();
        case NodeType::TimerRTO:              return std::make_unique<TimerRTONode>();
        case NodeType::TimerRTF:              return std::make_unique<TimerRTFNode>();
        case NodeType::PIDController:         return std::make_unique<PIDControllerNode>();
        case NodeType::PIDControllerAdvanced: return std::make_unique<PIDControllerAdvancedNode>();

        case NodeType::Function1:      return std::make_unique<FunctionNode1>();
        case NodeType::Function3:      return std::make_unique<FunctionNode3>();
        case NodeType::Function8:      return std::make_unique<FunctionNode8>();
        case NodeType::FunctionBool4:  return std::make_unique<FunctionBoolNode4>();
        case NodeType::FunctionBool8:  return std::make_unique<FunctionBoolNode8>();

        default: return nullptr;
    }
}
