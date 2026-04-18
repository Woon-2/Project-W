#ifndef global_thread_local_storage_hpp
#define global_thread_local_storage_hpp

extern thread_local std::shared_ptr<class SendBufferChunk> LSendBufferChunk;

extern thread_local HighResolutionClock::time_point LWorkStartTime;
extern thread_local class JobQueue* LJobQueue;

#endif // global_thread_local_storage_hpp