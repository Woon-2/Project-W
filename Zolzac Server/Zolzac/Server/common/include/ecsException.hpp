#ifndef __ECS_EXCEPTION_HPP
#define __ECS_EXCEPTION_HPP

#include "ShException.hpp"
#include "config.hpp"

#include <string_view>

#define ECS_EXCEPT(desc) ecs::Exception(__LINE__, __FILE__, desc)

namespace ecs {
	class Exception : public ShException {
	public:
		Exception( int lineNum, const char* fileName, std::string_view desc ) NOEXCEPT
			: ShException( lineNum, fileName ) {
			whatBuffer_ += desc;
		}

		const char* type( ) const NOEXCEPT override {
			return "ECS Exception";
		}
	};
}

#endif	// __ECS_EXCEPTION_HPP