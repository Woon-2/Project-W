#include "Window.hpp"
#include <vector>

class App {
public:
    using MyWindow = Win32::Window<Win32::BasicWindowTraits<char>>;

    App(HINSTANCE hInstance);

    int run();

private:
    MyWindow window_;
};