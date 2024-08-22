#include "keyboardWin32Adaptor.hpp"

#include "Window.hpp"

namespace ic {

namespace Win32 {

void Keyboard::patchKeyState() {
    GetKeyboardState(states().data());
}

}   // namespace ic::Win32

}   // namespace ic