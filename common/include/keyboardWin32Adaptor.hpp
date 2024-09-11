#ifndef __KeyboardWin32Adaptor_HPP
#define __KeyboardWin32Adaptor_HPP

#include "keyboardXX.hpp"

namespace ic {

namespace Win32 {

class Keyboard : public ic::Keyboard {
public:
    void patchKeyState() override;
};

}   // namespace ic::Win32

}   // namespace ic

#endif // __KeyboardWin32Adaptor_HPP