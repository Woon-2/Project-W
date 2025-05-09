#ifndef __SH_EXCEPTION_HPP
#define __SH_EXCEPTION_HPP

#include <exception>
#include <string>

class ShException : public std::exception {
public:
	ShException( int lineNum, const char* fileName ) noexcept;
	const char* what( ) const noexcept override;

	virtual const char* type( ) const noexcept {
		return "ShException";
	}

	int lineNum( ) const noexcept {
		return lineNum_;
	}

	const std::string& fileName( ) const noexcept {
		return fileName_;
	}

	std::string metaStr( ) const noexcept;

private:
	int lineNum_;
	std::string fileName_;

protected:
	mutable std::string whatBuffer_;
};

#endif	// __SH_EXCEPTION_HPP