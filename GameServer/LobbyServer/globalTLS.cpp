#include "pch.hpp"
#include "globalTLS.hpp"
#include "SendBuffer.hpp"

thread_local SendBufferChunk* LSendBufferChunk = nullptr;
