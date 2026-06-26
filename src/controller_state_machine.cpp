#include "controller_state_machine.h"

bool ControllerStateMachine::initialize(ControllerState initial_state)
{
    m_currentState = initial_state;
    m_isNew        = true;

    return true;
}

bool ControllerStateMachine::setState(ControllerState next_state)
{
    m_currentState = next_state;
    m_isNew        = true;

    return true;
}

ControllerState ControllerStateMachine::getState()
{
    return m_currentState;
}

bool ControllerStateMachine::isNewState()
{
    if (m_isNew == true)
    {
        m_isNew = false;
        return true;
    }

    return false;
}
