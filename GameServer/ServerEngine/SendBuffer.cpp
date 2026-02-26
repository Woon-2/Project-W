#include "pch.hpp"
#include "SendBuffer.hpp"

/*------------------
     SendBuffer
------------------*/

void SendBuffer::close(uint32 writeSize) {
    ASSERT_CRASH(writeSize <= allocSize_);
	writeSize_ = writeSize;
	owner_->close(writeSize_);
}
