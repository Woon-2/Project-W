#include "Window.hpp"

using MyWindow = Win32::Window<Win32::BasicWindowTraits<char>>;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MyWindow::setHInst(hInstance);
    auto wnd = MyWindow();
    wnd.setTitle("project-W client");
    wnd.show(SW_SHOW);

    wnd.addMsgHandler(10000, std::make_unique< Win32::BasicMsgHandler<MyWindow> >(wnd));

    wnd.msgLoop();
}