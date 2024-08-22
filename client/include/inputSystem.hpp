#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "keyboardXX.hpp"
#include "player.hpp"

class PlayerController
{
public:
    void processInput(Position& position, ic::Keyboard* pKeyboard)
    {
        if (pKeyboard->pressed('A')) {
            position.x -= 0.5;
        }

        if (pKeyboard->pressed('D')) {
            position.x += 0.5;
        }

        if (pKeyboard->pressed('W')) {
            position.y -= 0.5;
        }

        if (pKeyboard->pressed('S')) {
            position.y += 0.5;
        }
    }
};

class InputSystem
{
public:
    InputSystem() : pKeyboard_(nullptr) {}
    InputSystem(ic::Keyboard& keyboard) : pKeyboard_(&keyboard) {}

	void update();

private:
	ic::Keyboard* pKeyboard_;
};

#endif // !__INPUTSYSTEM_HPP
