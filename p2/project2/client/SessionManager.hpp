#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

//class SessionManager {
//public:
//	static void add( const SPServerSession& session ) {
//		std::lock_guard<std::mutex> lock( mtx_ );
//		sessions_.emplace( session->getId( ), session );
//	}
//
//	static void remove( const SPServerSession& session ) {
//		std::lock_guard<std::mutex> lock( mtx_ );
//		sessions_.erase( session->getId( ) );
//	}
//
//private:
//	static std::mutex mtx_;
//	static std::unordered_map<int32, SPServerSession> sessions_;
//};

#endif // SESSION_MANAGER_HPP