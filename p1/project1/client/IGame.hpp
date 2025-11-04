#ifndef __IGame_HPP
#define __IGame_HPP

#include "pch.hpp"

class IGame {
public:
	virtual ~IGame() = default;

	virtual void update(Milliseconds deltaTime) = 0;
	virtual void render() = 0;
};

#endif	// __IGame_HPP