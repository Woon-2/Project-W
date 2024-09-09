#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "keyboardXX.hpp"
#include "player.hpp"

class Rigidbody;

class PlayerController
{
public:
    void processInput(Rigidbody& rb, ic::Keyboard* pKeyboard);
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
