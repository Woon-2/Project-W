#include "Woon2Exception.hpp"

#include <sstream>

Woon2Exception::Woon2Exception( int lineNum,
    const char* fileStr ) noexcept
    : lineNum_(lineNum), fileStr_(fileStr)
{}


const char* Woon2Exception::what() const noexcept
{
    auto oss = std::ostringstream();
    oss << type() << '\n'
        << metaStr() << '\n';
    whatBuffer_.insert(0, oss.str());
    return whatBuffer_.c_str();
}

std::string Woon2Exception::metaStr() const noexcept
{
    auto oss = std::ostringstream();
    
    oss << "[File] " << fileStr() << '\n'
        << "[Line] " << lineNum() << '\n';
    return oss.str();
}