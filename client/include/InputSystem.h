#ifndef INPUTSYSTEM_H
#define INPUTSYSTEM_H

#include <algorithm>
#include <deque>
#include <fstream>
#include <map>
#include <ranges>
#include <vector>
#include <Windows.h>

enum class Key
{
	Left, Right, Up, Down,
	A, B, C, D,	E, F, G, H,	I, J, K, L, M, N, O, P, Q, R, S, T, U ,V, W, X, Y, Z,	
	Alt, Ctrl, LShift, Space, Enter, Esc, 
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
	Lbtn, Rbtn,
	Last
};

enum class KeyState : char
{
	None,
	Tap,
	Hold,
	Away,
};

static constexpr int KEY_COUNT = static_cast<int>(Key::Last);

class KeyBoard
{
public:
	KeyBoard();

	void patchKeyState();
	bool isKeyTap(Key key);
	bool isKeyHold(Key key);
	bool isKeyAway(Key key);
	bool isKeyNone(Key key);

	const char* GetKeyStates();

private:
	bool keyCheck(Key key, KeyState state);

private:
	char keyStates_[KEY_COUNT];

	char isPressedBefore_[KEY_COUNT];
};

// Keyboard로 부터 상태를 받아와 관리를 하는 키 매니저 입니다.
// 생성자에서 최대 검사 프레임을 넣어줄수 있습니다.
// 최대 검사 프레임은 성능에 매우 큰 영향을 주며 작은 값을 사용하는 것을 권장합니다.

class KeyManager
{
public:
	KeyManager(int maxFrame);

	void updateFrame(KeyBoard& keyboard);
	
	template <class... keys>
	bool isSomeKeysPressed(keys... key)
	{
		std::vector<char> keyList{ static_cast<char>(key)... };

		return std::ranges::all_of(keyList, [this](char key) {
			return std::ranges::any_of(keyFrame_, [key](const auto& frame) {
				auto state = frame.at(key);
				return state == KeyState::Tap || state == KeyState::Hold;
				});
			});
	}
	
	bool isTap(char key);
	bool isHold(char key);
	bool isAway(char key);
	bool isNone(char key);

private:	
	std::deque<std::map<char, KeyState>> keyFrame_;
	int maxKeyFrameIndex_;
	int curFrame_;
};

#endif // !INPUTSYSTEM_H
