#ifndef __EcsExcept_HPP
#define __EcsExcept_HPP

#include <string>
#include <string_view>

#include "Woon2Exception.hpp"
#include "config.hpp"

#define ECS_EXCEPT(desc) ecs::Exception(__LINE__, __FILE__, desc)

namespace ecs {
	class Exception : public Woon2Exception {
	public:
		Exception(int lineNum, const char* fileStr, std::string_view desc) NOEXCEPT
			: Woon2Exception(lineNum, fileStr) {
			whatBuffer_ += desc;
		}

		const char* type() const NOEXCEPT override {
			return "ECS Exception";
		}
	};

}

#endif // __EcsExcept_HPP
