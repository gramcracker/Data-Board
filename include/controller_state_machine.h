#pragma once

#include "controller_globals.h"

class ControllerStateMachine
{
public:
    bool            initialize(ControllerState initial_state);
    bool            setState(ControllerState next_state);
    ControllerState getState();
    bool            isNewState();

private:
    ControllerState m_currentState = ControllerState::BOOT;
    bool            m_isNew = true;
};
