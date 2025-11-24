#include "pch.hpp"
#include "GameLogic.hpp"

void GameLogic::run() {
	while (running_) {
		for (auto& room : rooms_) {
			room->update();
		}
	}
}
