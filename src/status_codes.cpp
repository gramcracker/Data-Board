#include "status_codes.h"
#include <cstdio>

static constexpr uint16_t BOARD_SHIFT     = 13;
static constexpr uint16_t COMPONENT_SHIFT = 8;
static constexpr uint16_t INSTANCE_SHIFT  = 5;

static constexpr uint16_t COMPONENT_MASK  = 0x1F;
static constexpr uint16_t INSTANCE_MASK   = 0x07;
static constexpr uint16_t FAULT_MASK      = 0x1F;

StatusCode::StatusCode()
{
    m_raw = 0;
}

bool StatusCode::isOk() const
{
    if (getFault() == Fault::OK)
    {
        return true;
    }

    return false;
}

Board StatusCode::getBoard() const
{
    return Board(m_raw >> BOARD_SHIFT);
}

ComponentId StatusCode::getComponent() const
{
    return ComponentId((m_raw >> COMPONENT_SHIFT) & COMPONENT_MASK);
}

uint8_t StatusCode::getInstance() const
{
    return uint8_t((m_raw >> INSTANCE_SHIFT) & INSTANCE_MASK);
}

Fault StatusCode::getFault() const
{
    return Fault(m_raw & FAULT_MASK);
}

uint16_t StatusCode::getRaw() const
{
    return m_raw;
}

void StatusCode::setRaw(uint16_t raw)
{
    m_raw = raw;
}

StatusCode makeStatus(Board board, ComponentId component, uint8_t instance, Fault fault)
{
    StatusCode status;
    uint16_t   packed = 0;

    packed |= uint16_t(uint16_t(board) << BOARD_SHIFT);
    packed |= uint16_t(uint16_t(component) << COMPONENT_SHIFT);
    packed |= uint16_t((instance & INSTANCE_MASK) << INSTANCE_SHIFT);
    packed |= uint16_t(uint16_t(fault) & FAULT_MASK);

    status.setRaw(packed);

    return status;
}

StatusCode makeOk(Board board, ComponentId component, uint8_t instance)
{
    return makeStatus(board, component, instance, Fault::OK);
}

bool combineWorst(StatusCode &io_accumulator, StatusCode candidate)
{
    if (io_accumulator.isOk() == false)
    {
        return false;
    }

    if (candidate.isOk() == true)
    {
        return false;
    }

    io_accumulator = candidate;

    return true;
}

const char *boardName(Board board)
{
    if (board == Board::STM32)
    {
        return "STM32";
    }

    if (board == Board::ESP32)
    {
        return "ESP32";
    }

    return "NONE";
}

const char *componentName(ComponentId component)
{
    switch (component)
    {
        case ComponentId::SYSTEM:      return "System";
        case ComponentId::POWER:       return "Power";
        case ComponentId::IMU:         return "Imu";
        case ComponentId::ENCODER:     return "Encoder";
        case ComponentId::MOTOR:       return "Motor";
        case ComponentId::DRIVETRAIN:  return "Drivetrain";
        case ComponentId::IR_EMITTER:  return "IrEmitter";
        case ComponentId::IR_RECEIVER: return "IrReceiver";
        case ComponentId::IR_PAIR:     return "IrPair";
        case ComponentId::CLIFF:       return "Cliff";
        case ComponentId::UART_LINK:   return "UartLink";
        case ComponentId::DISPLAY:     return "Display";
        case ComponentId::CAMERA:      return "Camera";
        case ComponentId::FLASH_MEM:   return "Flash";
        default:                       return "None";
    }
}

const char *faultName(Fault fault)
{
    switch (fault)
    {
        case Fault::OK:              return "Ok";
        case Fault::NO_RESPONSE:     return "NoResponse";
        case Fault::OUT_OF_RANGE:    return "OutOfRange";
        case Fault::STUCK_HIGH:      return "StuckHigh";
        case Fault::STUCK_LOW:       return "StuckLow";
        case Fault::NO_CHANGE:       return "NoChange";
        case Fault::WRONG_DIRECTION: return "WrongDirection";
        case Fault::TIMEOUT:         return "Timeout";
        case Fault::UNDERPERFORM:    return "Underperform";
        case Fault::CROSSTALK:       return "Crosstalk";
        case Fault::NOT_PRESENT:     return "NotPresent";
        case Fault::NOT_TESTED:      return "NotTested";
        case Fault::USER_FAIL:       return "UserFail";
        case Fault::INVALID_ID:      return "InvalidId";
        default:                     return "Unknown";
    }
}

bool statusToString(StatusCode status, char *p_out, uint32_t out_len)
{
    if (p_out == nullptr)
    {
        return false;
    }

    int written = snprintf(p_out, out_len, "%s/%s[%u]:%s",
                           boardName(status.getBoard()),
                           componentName(status.getComponent()),
                           status.getInstance(),
                           faultName(status.getFault()));

    if (written < 0)
    {
        return false;
    }

    return true;
}
