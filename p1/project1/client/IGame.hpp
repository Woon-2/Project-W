#ifndef __IGame_HPP
#define __IGame_HPP

#include "pch.hpp"

class IGame {
public:
	virtual ~IGame() = default;

	virtual void update(Milliseconds deltaTime) = 0;
	virtual void render() = 0;
	// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
	virtual LRESULT receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
};

#endif	// __IGame_HPP