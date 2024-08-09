#include "InputSystem.h"

#include <stdexcept>

using namespace std::literals;

int keys[KEY_COUNT] =
{
	VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',	'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	VK_MENU, VK_CONTROL, VK_LSHIFT, VK_SPACE, VK_RETURN, VK_ESCAPE,
	VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
	VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
	VK_LBUTTON, VK_RBUTTON
};

KeyBoard::KeyBoard()
{
	for (auto& key : keyStates_) {
		key = (char)KeyState::None;
	}

	for (auto& pressed : isPressedBefore_) {
		pressed = false;
	}
}

void KeyBoard::patchKeyState()
{
	for (int i = 0; i < KEY_COUNT; ++i)
	{
		// keys[i] is pressed
		if (GetAsyncKeyState(keys[i]) & 0x8000) 
		{
			if (isPressedBefore_[i]) 
				keyStates_[i] = (char)KeyState::Hold;
			else 
				keyStates_[i] = (char)KeyState::Tap;

			isPressedBefore_[i] = true;
		}
		// keys[i] isn't pressed
		else 
		{
			if (isPressedBefore_[i])
				keyStates_[i] = (char)KeyState::Away;
			else
				keyStates_[i] = (char)KeyState::None;

			isPressedBefore_[i] = false;
		}
	}	
}

bool KeyBoard::isKeyTap(Key key)
{
	return keyCheck(key, KeyState::Tap);
}

bool KeyBoard::isKeyHold(Key key)
{
	return keyCheck(key, KeyState::Hold);
}

bool KeyBoard::isKeyAway(Key key)
{
	return keyCheck(key, KeyState::Away);
}

bool KeyBoard::isKeyNone(Key key)
{
	return keyCheck(key, KeyState::None);
}

const char* KeyBoard::GetKeyStates()
{
	return keyStates_;
}

bool KeyBoard::keyCheck(Key key, KeyState state)
{
	return keyStates_[(int)key] == (char)state;
}

KeyManager::KeyManager(int frameRange) : maxKeyFrameIndex_(frameRange), curFrame_(-1)
{
}

void KeyManager::updateFrame(KeyBoard& keyboard)
{
	// get all key states from keyboard
	std::vector<char> keystates(KEY_COUNT);
	std::memcpy(keystates.data(), keyboard.GetKeyStates(), KEY_COUNT);

	// mapping char & Keystate
	if (keyFrame_.size() < maxKeyFrameIndex_)
	{
		std::map<char, KeyState> mappedStates;

		for (int i = 0; i < KEY_COUNT; ++i)
		{
			mappedStates[keys[i]] = (KeyState)keystates[i];
		}
		keyFrame_.push_back(std::move(mappedStates));
	}
	else
	{
		std::map<char, KeyState>& keyStateCopy = keyFrame_[curFrame_];
		for (int i = 0; i < KEY_COUNT; ++i)
		{
			keyStateCopy[keys[i]] = (KeyState)keystates[i];
		}
	}

	curFrame_ = (curFrame_ + 1) % maxKeyFrameIndex_;
}

bool KeyManager::isTap(char key)
{
	if (keyFrame_[curFrame_].at(key) == KeyState::Tap)
		return true;
	return false;
}

bool KeyManager::isHold(char key)
{
	if (keyFrame_[curFrame_].at(key) == KeyState::Hold)
		return true;
	return false;
}

bool KeyManager::isAway(char key)
{
	if (keyFrame_[curFrame_].at(key) == KeyState::Away)
		return true;
	return false;
}

bool KeyManager::isNone(char key)
{
	if (keyFrame_[curFrame_].at(key) == KeyState::None)
		return true;
	return false;
}
