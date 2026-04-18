#include "sepch.hpp"
#include "globalTLS.hpp"
#include "SendBuffer.hpp"
#include "JobQueue.hpp"

thread_local std::shared_ptr<SendBufferChunk> LSendBufferChunk = nullptr;

thread_local HighResolutionClock::time_point LWorkStartTime;
thread_local JobQueue* LJobQueue = nullptr;
