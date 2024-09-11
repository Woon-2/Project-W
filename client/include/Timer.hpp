#ifndef TIMER_H
#define TIMER_H

#include <Windows.h>
#include <string>

class Timer
{
public:
	Timer();
	
	void update();
	const std::string str() const;
	const double GetDT() const;

private:
	LARGE_INTEGER frequency_;
	LARGE_INTEGER prevCount_;
	LARGE_INTEGER curCount_;

	double deltaTime_;
	double accTime_;
	double fps_;

	UINT callCount_;
};

#endif