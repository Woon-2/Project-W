#include "Window.hpp"
#include <vector>
#include <memory>

class App {
public:
    using MyWindow = Win32::Window<Win32::BasicWindowTraits<char>>;

    App(HINSTANCE hInstance);

    int run();

private:
    std::vector< std::unique_ptr<MyWindow> > windows_;
};