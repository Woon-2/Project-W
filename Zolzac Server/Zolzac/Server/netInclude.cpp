#include "netInclude.hpp"

#include <system_error>
#include <string_view>

void errorDisplay( std::string_view where, int error ) {
	std::cerr << where << " failed : "
		<< std::system_category( ).message( error ) << '\n';
	exit( -1 );
}
