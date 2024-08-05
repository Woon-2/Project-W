#include "Timer.h"

Timer::Timer() : frequency_({ 0 }), curCount_({ 0 }), prevCount_({ 0 }), accTime_(0), fps_(0), deltaTime_(0), callCount_(0)
{
	QueryPerformanceFrequency(&frequency_);

	QueryPerformanceCounter(&prevCount_);
}

void Timer::update()
{
	++callCount_;

	QueryPerformanceCounter(&curCount_);

	deltaTime_ = (double)(curCount_.QuadPart - prevCount_.QuadPart) / (double)frequency_.QuadPart;

	prevCount_ = curCount_;

	accTime_ += deltaTime_;

	if (1.0 <= accTime_)
	{
		fps_ = callCount_;
		callCount_ = 0;
		accTime_ = 0;
	}
}

const std::string Timer::str() const {
	return std::to_string(fps_);
}

const double Timer::GetDT() const
{
	return deltaTime_;
}
