#include "ShException.hpp"

ShException::ShException( int lineNum, const char* fileName ) noexcept
	: lineNum_{ lineNum }, fileName_{ fileName } {}

const char* ShException::what( ) const noexcept {
	auto oss = std::ostringstream( );
	oss << type( ) << '\n'
		<< metaStr( ) << '\n';
	whatBuffer_.insert( 0, oss.str( ) );
	return oss.str( ).c_str( );
}

std::string ShException::metaStr( ) const noexcept {
	auto oss = std::ostringstream( );
	oss << "[File] " << fileName( ) << '\n'
		<< "[Line] " << lineNum( ) << '\n';
	return oss.str( );
}
