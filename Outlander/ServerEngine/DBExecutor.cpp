#include "sepch.hpp"
#include "DBExecutor.hpp"

/*--------------------
     DBExecutor
--------------------*/

std::thread DBExecutor::thread_;
std::mutex DBExecutor::mutex_;
std::condition_variable DBExecutor::cv_;
std::deque<std::function<void()>> DBExecutor::jobs_;
bool DBExecutor::running_ = false;
DBConnectionPool DBExecutor::pool_;

bool DBExecutor::init( int32 connCnt, const WCHAR* connStr ) {
	if ( !pool_.connect( connCnt, connStr ) ) {
		return false;   // 실패 원인은 DBConnection이 SQLSTATE와 함께 이미 출력했다
	}

	{
		std::lock_guard lock( mutex_ );
		running_ = true;
	}

	thread_ = std::thread( &DBExecutor::run );
	return true;
}

void DBExecutor::shutdown() {
	{
		std::lock_guard lock( mutex_ );
		if ( !running_ ) {
			return;
		}
		running_ = false;
	}

	cv_.notify_all();

	if ( thread_.joinable() ) {
		thread_.join();
	}

	pool_.clear();
}

void DBExecutor::post( std::function<void()> job ) {
	{
		std::lock_guard lock( mutex_ );
		if ( !running_ ) {
			return;
		}
		jobs_.push_back( std::move( job ) );
	}

	cv_.notify_one();
}

void DBExecutor::run() {
	for ( ;; ) {
		std::function<void()> job;

		{
			std::unique_lock lock( mutex_ );
			cv_.wait( lock, [] { return !jobs_.empty() || !running_; } );

			// 종료 요청이 와도 쌓인 잡은 마저 처리한다 (응답 유실 방지).
			if ( jobs_.empty() ) {
				return;
			}

			job = std::move( jobs_.front() );
			jobs_.pop_front();
		}

		job();
	}
}
