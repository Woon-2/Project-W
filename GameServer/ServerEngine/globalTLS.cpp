#include "sepch.hpp"
#include "globalTLS.hpp"
#include "SendBuffer.hpp"
#include "JobQueue.hpp"

thread_local SendBufferChunk* LSendBufferChunk = nullptr;
thread_local JobQueue* LJobQueue = nullptr;
