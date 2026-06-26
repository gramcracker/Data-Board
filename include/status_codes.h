#pragma once

#include <cstdint>

enum class Board : uint8_t
{
    NONE  = 0,
    STM32 = 1,
    ESP32 = 2
};

enum class ComponentId : uint8_t
{
    NONE        = 0,
    SYSTEM      = 1,
    POWER       = 2,
    IMU         = 3,
    ENCODER     = 4,
    MOTOR       = 5,
    DRIVETRAIN  = 6,
    IR_EMITTER  = 7,
    IR_RECEIVER = 8,
    IR_PAIR     = 9,
    CLIFF       = 10,
    UART_LINK   = 11,
    DISPLAY     = 12,
    CAMERA      = 13,
    FLASH_MEM   = 14
};

enum class Fault : uint8_t
{
    OK              = 0,
    NO_RESPONSE     = 1,
    OUT_OF_RANGE    = 2,
    STUCK_HIGH      = 3,
    STUCK_LOW       = 4,
    NO_CHANGE       = 5,
    WRONG_DIRECTION = 6,
    TIMEOUT         = 7,
    UNDERPERFORM    = 8,
    CROSSTALK       = 9,
    NOT_PRESENT     = 10,
    NOT_TESTED      = 11,
    USER_FAIL       = 12,
    INVALID_ID      = 13
};

class StatusCode
{
public:
    StatusCode();

    bool        isOk() const;
    Board       getBoard() const;
    ComponentId getComponent() const;
    uint8_t     getInstance() const;
    Fault       getFault() const;
    uint16_t    getRaw() const;
    void        setRaw(uint16_t raw);

    uint16_t m_raw;
};

StatusCode makeStatus(Board board, ComponentId component, uint8_t instance, Fault fault);
StatusCode makeOk(Board board, ComponentId component, uint8_t instance);

bool combineWorst(StatusCode &io_accumulator, StatusCode candidate);

const char *boardName(Board board);
const char *componentName(ComponentId component);
const char *faultName(Fault fault);
bool        statusToString(StatusCode status, char *p_out, uint32_t out_len);
