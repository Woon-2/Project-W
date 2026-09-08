#include "sepch.hpp"
#include "globalTLS.hpp"
#include "SendBuffer.hpp"

thread_local std::shared_ptr<SendBufferChunk> LSendBufferChunk = nullptr;
