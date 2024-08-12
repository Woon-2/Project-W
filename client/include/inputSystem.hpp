#ifndef __INPUTSYSTEM_HPP
#define __INPUTSYSTEM_HPP

#include "keyboard.hpp"
#include "player.hpp"

class PlayerController
{
public:
    void processInput(Position& position, const char keyStates[KEY_COUNT])
    {
        if (keyStates[(int)Key::A] == (char)KeyState::Tap ||
            keyStates[(int)Key::A] == (char)KeyState::Hold)
        {
            position.x -= 0.5;
        }

        if (keyStates[(int)Key::D] == (char)KeyState::Tap ||
            keyStates[(int)Key::D] == (char)KeyState::Hold)
        {
            position.x += 0.5;
        }

        if (keyStates[(int)Key::W] == (char)KeyState::Tap ||
            keyStates[(int)Key::W] == (char)KeyState::Hold)
        {
            position.y -= 0.5;
        }

        if (keyStates[(int)Key::S] == (char)KeyState::Tap ||
            keyStates[(int)Key::S] == (char)KeyState::Hold)
        {
            position.y += 0.5;
        }
    }
};

class InputSystem
{
public:
    void init(KeyBoard& keyboard);
	void update();

private:
	KeyBoard* keyboard_;
};

#endif // !__INPUTSYSTEM_HPP
