#ifndef __Window
#define __Window

#include <Windows.h>

#include <string>
#include <string_view>
#include <memory>
#include <concepts>
#include <list>
#include <optional>
#include <map>

#include "Woon2Exception.hpp"

template <class T, class ... Args>
concept contains = (std::same_as<T, Args> || ...);

template <class T>
concept Win32Char = contains<T, CHAR, WCHAR>;

#define WND_EXCEPT(hr) Win32::WindowException(__LINE__, __FILE__, (hr))
#define WND_THROW_FAILED(hrcall)    \
    if ( HRESULT hr = (hrcall); hr < 0 ) \
        throw WND_EXCEPT(hr)
#define WND_LAST_EXCEPT() WND_EXCEPT( GetLastError() )

namespace Win32
{

/**
 * @brief Exception class for Windows API
 * @details call WindowException::what to get error message, WindowException::type to get exception type
 * @code
 * try {
 *     // some Windows API call
 * } catch (const WindowException& e) {
 *     MessageBoxA(nullptr, e.what(), e.type(), MB_OK | MB_ICONEXCLAMATION);
 * }
 * @endcode
 */
class WindowException : public Woon2Exception
{
public:
    WindowException( int lineNum, const char* fileStr,
        HRESULT hr ) NOEXCEPT;


    /**
     * @brief Get the error message.
     * @return `const char*`  the error message.
     * @see WindowException
     */
    const char* what() const NOEXCEPT override;
    /**
     * @brief Get the exception type.
     * @return `const char*`  the exception type.
     * @see WindowException
     */
    const char* type() const NOEXCEPT override;

    /**
     * @brief Get the error code retreived from API function(`HRESULT`).
     * @return `HRESULT`  the error code.
     * @see WindowException
     */
    HRESULT errorCode() const NOEXCEPT;
    /**
     * @brief Get the error message as `std::string`.     
     * It calls WindowException::translateErrorCode function to translate the error code to the human-readable error message.
     * @return `std::string`  the error message.
     * @see WindowException
     */
    const std::string errorStr() const NOEXCEPT;

    /**
     * @brief Translate the error code to the human-readable error message.
     * @param hr  the error code to translate.
     * @return `const std::string`  the error message.
     * @see WindowException
     */
    static const std::string
        translateErrorCode( HRESULT hr ) NOEXCEPT;

private:
    HRESULT hr_;
};

template <class T>
concept Win32Char = contains<T, CHAR, WCHAR>;

struct WndFrame
{
    int x;
    int y;
    int width;
    int height;

    operator RECT() const NOEXCEPT {
        return RECT{x, y, x + width, y + height};
    }
};

struct Message
{
    UINT type;
    WPARAM wParam;
    LPARAM lParam;
};

template <class Traits, class ... Args>
concept canRegist = requires (Args&& ... args) {
    Traits::regist( std::forward<Args>(args)... );
};

template <class Traits, class ... Args>
concept canCreate = requires (Args&& ... args) {
    { Traits::create( std::forward<Args>(args)... ) } -> std::same_as<HWND>;
};

template <class Traits, class ... Args>
concept canShow = requires (Args&& ... args) {
    Traits::show( std::forward<Args>(args)... );
};

template <class Traits, class ... Args>
concept canDestroy = requires (Args&& ... args) {
    Traits::destroy( std::forward<Args>(args)... );
};

template <class Traits, class ... Args>
concept canUnregist = requires (Args&& ... args) {
    Traits::unregist( std::forward<Args>(args)... );
};

/**
 * @brief Interface for message handlers, since base class exist, do not directly inherit from this class.
 * @see MsgHandler
 */
class IMsgHandler {
public:
    virtual ~IMsgHandler() {}
    virtual std::optional<LRESULT> operator()(const Message& msg) = 0;
};

/**
 * @brief
 * A base class template for message handlers.    
 * Inherit from this class to create a message handler    
 * and override the MsgHandler::operator()(const Message& msg) to handle messages.    
 * @tparam Wnd  the window class that this message handler is for.
 * @details
 * message handlers are called in the order by their index,    
 * which is set by Window::addMsgHandler.   
 * 
 * Multiple message handlers cooperate to handle messages with chain of responsibility pattern.    
 * Each message handler should proccess the message if it is in interest of the handler.    
 * And each message handler can decide whether to pass the message to the next handler or not.   
 * returning `std::nullopt` means the message handler passes the message to the next handler.    
 * Otherwise, it means the message handler has processed the message and the handler chain should stop.    
 * If message handler chain reaches the end, the default window procedure will handle the message.
 *     
 * Following code is an example of a message handler.    
 * @code{.cpp}
 * template <class Wnd>
 * class SampleMsgHandler : public MsgHandler<Wnd> {
 * public:
 *     using MsgHandler<Wnd>::window;
 * 
 *     SampleMsgHandler(Wnd& wnd)
 *         : MsgHandler<Wnd>(wnd) {}
 * 
 *     std::optional<LRESULT> operator()(const Message& msg) override;
 * };
 * 
 * template <class Wnd>
 * std::optional<LRESULT> SampleMsgHandler<Wnd>::operator()(
 *     const Message& msg
 * ) {
 *     switch (msg.type) {
 *     case WM_CLOSE:
 *         PostMessageA(window().nativeHandle(), WM_DESTROY, 0, 0);
 *         window().close();
 *         return 0;
 *
 *     case WM_DESTROY:
 *         PostQuitMessage(0);
 *         return 0;
 *
 *     default: return {};
 *     }
 * }
 * @endcode
 * 
 * @see Window    
 * Message
 */
template <class Wnd>
class MsgHandler : public IMsgHandler
{
public:
    using MyWindow = Wnd;

    MsgHandler(Wnd& wnd) NOEXCEPT
        : window_(wnd)
    {}
    MsgHandler(const MsgHandler&) = delete;
    MsgHandler(MsgHandler&&) = delete;
    MsgHandler& operator=(const MsgHandler&) = delete;
    MsgHandler& operator=(MsgHandler&&) = delete;
    virtual ~MsgHandler() {}

    /**
     * @brief Override this function to handle a message.
     * @param msg  the message to handle.
     * @return `std::optional<LRESULT>`     
     * return `std::nullopt` if the handler wants to pass through the message to the next handler,    
     * otherwise return the result of the message handling.
     * @see
     * Message    
     * MsgHandler
     */
    virtual std::optional<LRESULT> operator()(const Message& msg) = 0;

protected:
    const Wnd& window() const NOEXCEPT { return window_; }
    Wnd& window() NOEXCEPT { return window_; }

private:
    Wnd& window_;
};

/**
 * @brief
 * A basic sample message handler that handles WM_CLOSE and WM_DESTROY messages.     
 * This class is for demonstration purpose.     
 * 
 * It terminates application when the window is destroyed via sending WM_QUIT on WM_DESTROY.
 * @tparam Wnd  the window class that this message handler is for.
 * 
 * @see MsgHandler
 */
template <class Wnd>
class BasicMsgHandler : public MsgHandler<Wnd>
{
public:
    using MsgHandler<Wnd>::window;
    using MyChar = typename Wnd::MyChar;

    BasicMsgHandler(Wnd& wnd)
        : MsgHandler<Wnd>(wnd) {}

    std::optional<LRESULT> operator()(const Message& msg) override;
};


/**
 * @brief
 * A class template for Win32 window.    
 * Win32's window Class part is encapsulated in the `Traits`,      
 * and the window Instance part is encapsulated in this class.
 * @tparam Traits  the traits class that encapsulates Win32's window Class part.
 * @details
 * `Traits` should have the 5 following static member functions:
 * - static void regist(HINSTANCE hInst);
 * - static void unregist(HINSTANCE hInst);
 * - static HWND create(HINSTANCE hInst, Window<Traits>* pWnd, ...);
 * - static void destroy(HWND hWnd);
 * - static void show(HWND hWnd, int nCmdShow);
 * 
 * When the Window is created, it is not opened yet.    
 * To open the window, call Window::open with the arguments that the `Traits::create` requires.    
 * (You can define additional custom arguments on ... part of the `Traits::create`.)    
 * Window::open registers a Win32 window class through `Traits::register` if it is not registered yet.     
 * And it passes the arguments to the `Traits::create` function     
 * with insertion of the proper `HINSTANCE` and Window object's pointer as the foremost arguments.
 * 
 * To make the Window object visible, call Window::show function with the `nCmdShow`(`SW_SHOW`) argument.
 * 
 * @code{.cpp}
 * Window<BasicWindowTraits> wnd;
 * wnd.open();
 * wnd.show(SW_SHOW);
 * 
 * wnd.msgLoop();
 * // wnd.close(); // automatically called on the Window's destructor if the window is not closed.
 * @endcode
 * 
 * You can add your own message handlers arbitrarily by calling Window::addMsgHandler.    
 * If no message handler is configured, Win32's default window procedure will handle all messages.
 * 
 * @see MsgHandler
 */
template <class Traits>
class Window
{
public:
    using MyType = Window<Traits>;
    using MyTraits = Traits;
    using MyChar = Traits::MyChar;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    friend MyTraits;

    /**
     * @brief Construct a new Window object.     
     * unless Window::open is called, the window is not created neither registered. 
     */

    Window()
        : title_(), frame_(), msgHandlers_(), hWnd_(nullptr) {}

    ~Window() requires canDestroy<Traits, HWND>
    { 
        close();
    }

    Window(const Window&) requires false;
    Window(Window&&) requires false;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    /**
     * @brief Open the window.    
     * If the window is already opened, it closes the window first, then opens the window again.
     * @tparam Args  the types of the custom arguments for the `Traits::create` function.
     * @param args  the custom arguments for the `Traits::create` function.
     * @details
     * It calls Traits::create with the `hInst` and the `this` pointer of the Window object, and varadic custom arguments.    
     * If Traits::regist is not called yet, it calls Traits::regist with the `hInst` that is set by Window::setHInst.    
     * @see
     * Window::setHInst
     * Window::close
     */
    template <class ... Args>
    requires canRegist<Traits, HINSTANCE>
        && canCreate<Traits, HINSTANCE, Window<Traits>*, Args...>
    void open(Args&& ... args);

    /**
     * @brief Close the window.
     * If the window is already closed, it does nothing.
     * @details
     * It calls Traits::destroy with the window handle, and sets the window handle to `nullptr`.
     * @see Window::open
     */
    void close() requires canDestroy<Traits, HWND> {
        if ( nativeHandle() ) {
            Traits::destroy( nativeHandle() );
            hWnd_ = nullptr;
        }
    }

    /**
     * @brief Set the `HINSTANCE` for the Window.    
     * It should be called before any Window instance's Window::open is called.
     * @param hInstance  the `HINSTANCE` to set.
     * @details
     * The `HINSTANCE` value is shared among all Window instances.    
     * However, it is not shared among different Window classes.     
     * So, if you have made multiple Window classes derived from this class,
     * or if you have made multiple Window classes with different Traits,    
     * you should call this function for each derived class.
     * @code
     * Window<BasicWindowTraits>::setHInst(hInstance);
     * auto wnd = Window<BasicWindowTraits>();
     * wnd.open();
     * wnd.show(SW_SHOW);
     * 
     * Window<AnotherWindowTraits>::setHInst(hInstance);    // diffrent traits, different Window class.
     * auto wnd2 = Window<AnotherWindowTraits>();
     * wnd2.open();
     * wnd2.show(SW_SHOW);
     * 
     * MyWindow<BasicWindowTraits>::setHInst(hInstance);    // derivation, also different Window class.
     * auto wnd3 = MyWindow<BasicWindowTraits>();
     * wnd3.open();
     * wnd3.show(SW_SHOW);
     * @endcode
     * @see Window::open
     */
    static void setHInst(HINSTANCE hInstance) NOEXCEPT
    { hInst = hInstance; }
    static HINSTANCE getHInst() NOEXCEPT { return hInst; }

    /**
     * @brief Run the message loop.
     * @details It calls GetMessage, TranslateMessage, and DispatchMessage in a loop until GetMessage returns 0 or negative value.    
     *     
     * usage example:
     * @code
     * Window<BasicWindowTraits> wnd;
     * wnd.open();
     * wnd.show(SW_SHOW);
     * 
     * wnd.msgLoop();
     * @endcode
     * @see Window::processMessages
     */
    void msgLoop();
    /**
     * @brief Process queued messages.
     * @return `std::optional<int>`  return code if WM_QUIT message is received, otherwise `std::nullopt`.
     * @details
     * It calls PeekMessage with PM_REMOVE flag, TranslateMessage, and DispatchMessage in a loop    
     * until there is no more message in the queue.    
     *     
     * usage example:
     * @code
     * Window<BasicWindowTraits> wnd;
     * wnd.open();
     * wnd.show(SW_SHOW);
     * 
     * while (true) {
     *     if (auto returnCode = wnd.processMessages()) {
     *         return returnCode.value();
     *     }
     * 
     *     update();
     *     render();
     * }
     * @endcode
     * @see Window::msgLoop
     */
    std::optional<int> processMessages();
    HWND nativeHandle() const NOEXCEPT { return hWnd_; }
    /**
     * @brief Add a message handler with a specific index.    
     * If the message handler with the same index is already added, it does nothing.
     * @param index  the index of the message handler.
     * @param msgHandler  the message handler to add.
     * @details The message handlers are called in the order by their index.
     * @see MsgHandler Window::removeMsgHandler
     */
    void addMsgHandler(int index, std::unique_ptr<IMsgHandler>&& msgHandler);
    /**
     * @brief Remove the message handler with the specific index.
     * If the message handler with the specific index is not found, it does nothing.
     * @param index  the index of the message handler to remove.
     * @return `bool`  `true` if the message handler with the index existed and has been removed, otherwise `false`.
     * @see MsgHandler Window::addMsgHandler
     */
    [[maybe_unused]] bool removeMsgHandler(int index);
    MyStringView title() const NOEXCEPT { return title_; }
    void setTitle(const MyString& windowTitle)
    {
        title_ = windowTitle;
        setNativeTitle( title().data() );
    }
    void setTitle(MyString&& windowTitle)
    {
        title_ = std::move(windowTitle);
        setNativeTitle( title().data() );
    }
    /**
     * @brief Show the window with the specific command.
     * @param nCmdShow  the command to show the window.
     * @details It calls Traits::show with the window handle and the command.
     */
    void show(int nCmdShow) requires canShow<Traits, HWND, int>
    {
        Traits::show( nativeHandle(), nCmdShow );
    }
    
private:
    static LRESULT CALLBACK wndProcSetupHandler(HWND hWnd, UINT type,
        WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK wndProcCallHandler(HWND hWnd, UINT type,
        WPARAM wParam, LPARAM lParam);

    void setNativeTitle(const MyChar* title)
    {
        bool bFine = false;

        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            bFine = SetWindowTextA( nativeHandle(), title );
        }
        else /* WCHAR */ {
            bFine = SetWindowTextW( nativeHandle(), title );
        }

        if (!bFine) [[unlikely]] {
            throw WND_LAST_EXCEPT();
        }
    }

    static bool bRegist;
    static HINSTANCE hInst;

    MyString title_;
    WndFrame frame_;
    std::map< int, std::unique_ptr<IMsgHandler> > msgHandlers_;
    HWND hWnd_;
};

template <Win32Char CharT>
struct BasicWindowTraits
{
public:
    using MyWindow = Window< BasicWindowTraits >;
    using MyChar = CharT;
    using MyString = std::basic_string<MyChar>;
    using MyStringView = std::basic_string_view<MyChar>;

    static constexpr const MyStringView clsName() NOEXCEPT
    {
        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            return "WT";
        }
        else /* WCHAR */ {
            return L"WT";
        }
    }
    static constexpr const MyStringView defWndName() NOEXCEPT
    {
        if constexpr ( std::is_same_v<MyChar, CHAR> ) {
            return "Window";
        }
        else /* WCHAR */ {
            return L"Window";
        }
    }
    static constexpr const WndFrame defWndFrame() NOEXCEPT
    {
        return WndFrame{ .x=200, .y=200, .width=800, .height=600 };
    }

    static void regist(HINSTANCE hInst);
    static void unregist(HINSTANCE hInst);
    static HWND create(HINSTANCE hInst, MyWindow* pWnd);
    static HWND create(HINSTANCE hInst, MyWindow* pWnd, MyStringView wndName);
    static HWND create(HINSTANCE hInst, MyWindow* pWnd,
        const WndFrame& wndFrame);
    static HWND create(HINSTANCE hInst, MyWindow* pWnd, MyStringView wndName,
        const WndFrame& wndFrame);
    static void destroy(HWND hWnd)
    {
        auto bFine = DestroyWindow(hWnd);

        if (!bFine) {
            throw WND_LAST_EXCEPT();
        }
    }
    static void show(HWND hWnd, int nCmdShow) { ShowWindow(hWnd, nCmdShow); }
};

template <class Traits>
bool Window<Traits>::bRegist = false;

template <class Traits>
HINSTANCE Window<Traits>::hInst = nullptr;

}   // namespace Win32

#include "Window.inl"

#endif  // __Window