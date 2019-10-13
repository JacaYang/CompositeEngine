#include "CefMain.h"

#include "cef/browser/UIAppBrowser.h"
#include "cef/browser/UIBrowserProcessHandler.h"
#include "cef/browser/UIExternalMessagePump.h"
#include "cef/browser/UIQueryHandler.h"
#include "cef/browser/UIQueryResponder.h"
#include "cef/client/UIClient.h"
#include "cef/client/UILifeSpanHandler.h"
#include "cef/client/UIRenderHandler.h"
#include "cef/client/UIRequestHandler.h"
#include "event/SdlEvent.h"
#include "event/WindowsMessageEvent.h"
#include "event/core/EventSystem.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"
#include <SDL_syswm.h>

#include <iostream>

#ifdef __APPLE__
#    include "cef/WindowContentView.h"
#    include "include/wrapper/cef_library_loader.h"
#    include <CoreFoundation/CoreFoundation.h>
#endif

namespace CE
{
    static const int REMOTE_DEBUGGING_PORT = 3469;

    CefMain::CefMain(EventSystem* eventSystem, SDL_Window* window)
        : eventSystem(eventSystem)
        , window(window)
        , externalMessagePump(new UIExternalMessagePump())
        , queryHandler(new UIQueryHandler(eventSystem, new UIQueryResponder(eventSystem)))
    {
        eventSystem->RegisterListener(this, EventType::SDL);
        eventSystem->RegisterListener(this, EventType::WINDOWS_MESSAGE);
    }

    // NOLINTNEXTLINE(misc-unused-parameters, cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays)
    bool CefMain::StartCef(int argc, char* argv[], uint32_t screenWidth, uint32_t screenHeight)
    {
#ifdef __APPLE__
        CefScopedLibraryLoader libraryLoader;
        if (!libraryLoader.LoadInMain())
        {
            std::cout << "CEF failed to load framework in engine.\n";
            return false;
        }
#endif

        CefMainArgs mainArgs;
#ifdef _WIN32
        mainArgs = CefMainArgs(::GetModuleHandle(nullptr));
#elif __APPLE__
        mainArgs = CefMainArgs(argc, argv);
#endif

        CefRefPtr<UIBrowserProcessHandler> browserProcessHandler = new UIBrowserProcessHandler(externalMessagePump);
        CefRefPtr<UIAppBrowser> app = new UIAppBrowser(browserProcessHandler);

        CefSettings settings;
        settings.no_sandbox = 1;
        settings.external_message_pump = 1;
        settings.windowless_rendering_enabled = 1;
        settings.remote_debugging_port = REMOTE_DEBUGGING_PORT;
#ifdef _WIN32
        CefString(&settings.browser_subprocess_path).FromASCII("CompositeCefSubprocess.exe");
#elif __APPLE__
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        // TODO: Free? See
        // https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFMemoryMgmt/Concepts/Ownership.html#//apple_ref/doc/uid/20001148-103029
        CFURLRef privateFrameworksUrl = CFBundleCopyPrivateFrameworksURL(mainBundle);
        UInt8 privateFrameworksDirectoryName[1024];
        CFURLGetFileSystemRepresentation(privateFrameworksUrl, true, privateFrameworksDirectoryName, 1024);

        std::string subprocessFile = (char*) privateFrameworksDirectoryName;
        subprocessFile += "/CompositeCefSubprocess.app/Contents/MacOS/CompositeCefSubprocess";
        CefString(&settings.browser_subprocess_path).FromString(subprocessFile);
#endif

        if (!CefInitialize(mainArgs, settings, app, nullptr))
        {
            std::cout << "CEF failed to initialize.\n";
            return false;
        }

        CefMessageRouterConfig messageRouterConfig;
        CefRefPtr<CefMessageRouterBrowserSide> messageRouterBrowserSide =
                CefMessageRouterBrowserSide::Create(messageRouterConfig);
        messageRouterBrowserSide->AddHandler(queryHandler, true);

        CefRefPtr<UIContextMenuHandler> contextMenuHandler = new UIContextMenuHandler();
        CefRefPtr<UIRenderHandler> renderHandler = new UIRenderHandler(screenWidth, screenHeight);
        CefRefPtr<UILifeSpanHandler> lifeSpanHandler = new UILifeSpanHandler(messageRouterBrowserSide);
        CefRefPtr<UILoadHandler> loadHandler = new UILoadHandler();
        CefRefPtr<UIRequestHandler> requestHandler = new UIRequestHandler(messageRouterBrowserSide);
        uiClient = new UIClient(
                contextMenuHandler,
                renderHandler,
                lifeSpanHandler,
                loadHandler,
                requestHandler,
                messageRouterBrowserSide);

        SDL_SysWMinfo sysInfo;
        SDL_VERSION(&sysInfo.version);
        if (SDL_GetWindowWMInfo(window, &sysInfo) == SDL_FALSE)
        {
            return false;
        }

        CefBrowserSettings browserSettings;
        CefWindowInfo windowInfo;
#ifdef _WIN32
        windowInfo.SetAsWindowless(sysInfo.info.win.window); // NOLINT(cppcoreguidelines-pro-type-union-access)
#elif __APPLE__
        NSView* view = (NSView*) CE::GetWindowContentView(sysInfo.info.cocoa.window);
        windowInfo.SetAsWindowless(view);
#endif

        browser = CefBrowserHost::CreateBrowserSync(
                windowInfo,
                uiClient,
                "http://localhost:3000", // "about:blank"
                browserSettings,
                nullptr);

        return true;
    }

    void CefMain::StopCef()
    {
        externalMessagePump->Shutdown();

        browser->GetHost()->CloseBrowser(true);

        browser = nullptr;
        uiClient = nullptr;

        CefShutdown();
    }

    const std::byte* CefMain::GetViewBuffer()
    {
        return dynamic_cast<UIRenderHandler*>(uiClient->GetRenderHandler().get())->GetViewBuffer();
    }

    const std::byte* CefMain::GetPopupBuffer()
    {
        return dynamic_cast<UIRenderHandler*>(uiClient->GetRenderHandler().get())->GetPopupBuffer();
    }

    const CefRect& CefMain::GetPopupRect()
    {
        return dynamic_cast<UIRenderHandler*>(uiClient->GetRenderHandler().get())->GetPopupRect();
    }

    bool CefMain::HasPopup()
    {
        return dynamic_cast<UIRenderHandler*>(uiClient->GetRenderHandler().get())->HasPopup();
    }

    void CefMain::OnEvent(const Event& event)
    {
        switch (event.type)
        {
            case EventType::SDL:
            {
                HandleSdlEvent(event);
                break;
            }
#ifdef _WIN32
            case EventType::WINDOWS_MESSAGE:
            {
                HandleWindowsMessageEvent(event);
                break;
            }
#endif
            default:
                break;
        }
    }

    void CefMain::HandleSdlEvent(const Event& event)
    {
        const SdlEvent& wrappedEvent = dynamic_cast<const SdlEvent&>(event);

        const SDL_Event& nativeEvent = wrappedEvent.event;

        externalMessagePump->ProcessEvent(nativeEvent);

        switch (nativeEvent.type)
        {
            case SDL_KEYDOWN:
            {
#ifdef _WIN32
                switch (nativeEvent.key.keysym.sym)
                {
                    case SDLK_F11:
                    case SDLK_F12:
                    {
                        ToggleDevToolsWindow();
                        break;
                    }
                }
#endif
                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEMOTION:
            {
                // TODO: CEF also gets all of the right-click camera movement events, which are unnecessary.
                CefMouseEvent mouseEvent;
                mouseEvent.x = nativeEvent.motion.x;
                mouseEvent.y = nativeEvent.motion.y;
                mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                browser->GetHost()->SendMouseMoveEvent(mouseEvent, false);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEBUTTONDOWN:
            {
                CefBrowserHost::MouseButtonType mouseButtonType;
                if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_LEFT))
                {
                    mouseButtonType = MBT_LEFT;
                }
                else if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_MIDDLE))
                {
                    mouseButtonType = MBT_MIDDLE;
                }
                else if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_RIGHT))
                {
                    mouseButtonType = MBT_RIGHT;
                }
                else
                {
                    break;
                }

                CefMouseEvent mouseEvent;
                mouseEvent.x = nativeEvent.button.x;
                mouseEvent.y = nativeEvent.button.y;
                mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, false, nativeEvent.button.clicks);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEBUTTONUP:
            {
                CefBrowserHost::MouseButtonType mouseButtonType;
                if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_LEFT))
                {
                    mouseButtonType = MBT_LEFT;
                }
                else if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_MIDDLE))
                {
                    mouseButtonType = MBT_MIDDLE;
                }
                else if (IsMouseButton(nativeEvent.button.button, SDL_BUTTON_RIGHT))
                {
                    mouseButtonType = MBT_RIGHT;
                }
                else
                {
                    break;
                }

                CefMouseEvent mouseEvent;
                mouseEvent.x = nativeEvent.button.x;
                mouseEvent.y = nativeEvent.button.y;
                mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, true, nativeEvent.button.clicks);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEWHEEL:
            {
                CefMouseEvent mouseEvent;
                SDL_GetMouseState(&mouseEvent.x, &mouseEvent.y);
                mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                browser->GetHost()->SendMouseWheelEvent(mouseEvent, nativeEvent.wheel.x, nativeEvent.wheel.y);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_WINDOWEVENT:
            {
                switch (nativeEvent.window.event)
                {
                    case SDL_WINDOWEVENT_LEAVE:
                    {
                        CefMouseEvent mouseEvent;
                        SDL_GetMouseState(&mouseEvent.x, &mouseEvent.y);
                        mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                        browser->GetHost()->SendMouseMoveEvent(mouseEvent, true);

                        break;
                    }
                }

                break;
            }
        }
    }

    // osr_window_win.cc
    // browser_window_osr_mac.mm
    uint32_t CefMain::GetSdlCefInputModifiers(const SDL_Event& event)
    {
        uint32_t modifiers = 0;

        SDL_Keymod keymod = SDL_GetModState();

        if (IsKeyModActive(keymod, KMOD_CTRL))
        {
            modifiers |= AsUnsigned(EVENTFLAG_CONTROL_DOWN);
        }

        if (IsKeyModActive(keymod, KMOD_SHIFT))
        {
            modifiers |= AsUnsigned(EVENTFLAG_SHIFT_DOWN);
        }

        if (IsKeyModActive(keymod, KMOD_ALT))
        {
            modifiers |= AsUnsigned(EVENTFLAG_ALT_DOWN);
        }

        if (IsKeyModActive(keymod, KMOD_NUM))
        {
            modifiers |= AsUnsigned(EVENTFLAG_NUM_LOCK_ON);
        }

        if (IsKeyModActive(keymod, KMOD_CAPS))
        {
            modifiers |= AsUnsigned(EVENTFLAG_CAPS_LOCK_ON);
        }

#ifdef __APPLE__
        if (IsKeyModActive(keymod, KMOD_GUI))
        {
            modifiers |= AsUnsigned(EVENTFLAG_COMMAND_DOWN);
        }
#endif

        // todo: if mouse-only, still keep these two if's?
        if (IsKeyModActive(keymod, KMOD_LSHIFT) || IsKeyModActive(keymod, KMOD_LCTRL)
            || IsKeyModActive(keymod, KMOD_LALT) || IsKeyModActive(keymod, KMOD_LGUI))
        {
            modifiers |= AsUnsigned(EVENTFLAG_IS_LEFT);
        }

        if (IsKeyModActive(keymod, KMOD_RSHIFT) || IsKeyModActive(keymod, KMOD_RCTRL)
            || IsKeyModActive(keymod, KMOD_RALT) || IsKeyModActive(keymod, KMOD_RGUI))
        {
            modifiers |= AsUnsigned(EVENTFLAG_IS_RIGHT);
        }

        switch (event.type)
        {
            // todo: remove and make this function mouse-only?
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym)
                {
                    case SDLK_KP_DIVIDE:
                    case SDLK_KP_MULTIPLY:
                    case SDLK_KP_MINUS:
                    case SDLK_KP_PLUS:
                    case SDLK_KP_ENTER:
                    case SDLK_KP_1:
                    case SDLK_KP_2:
                    case SDLK_KP_3:
                    case SDLK_KP_4:
                    case SDLK_KP_5:
                    case SDLK_KP_6:
                    case SDLK_KP_7:
                    case SDLK_KP_8:
                    case SDLK_KP_9:
                    case SDLK_KP_0:
                    case SDLK_KP_PERIOD:
                    case SDLK_KP_EQUALS:
                    case SDLK_KP_COMMA:
                    case SDLK_KP_EQUALSAS400:
                    case SDLK_KP_00:
                    case SDLK_KP_000:
                    case SDLK_KP_LEFTPAREN:
                    case SDLK_KP_RIGHTPAREN:
                    case SDLK_KP_LEFTBRACE:
                    case SDLK_KP_RIGHTBRACE:
                    case SDLK_KP_TAB:
                    case SDLK_KP_BACKSPACE:
                    case SDLK_KP_A:
                    case SDLK_KP_B:
                    case SDLK_KP_C:
                    case SDLK_KP_D:
                    case SDLK_KP_E:
                    case SDLK_KP_F:
                    case SDLK_KP_XOR:
                    case SDLK_KP_POWER:
                    case SDLK_KP_PERCENT:
                    case SDLK_KP_LESS:
                    case SDLK_KP_GREATER:
                    case SDLK_KP_AMPERSAND:
                    case SDLK_KP_DBLAMPERSAND:
                    case SDLK_KP_VERTICALBAR:
                    case SDLK_KP_DBLVERTICALBAR:
                    case SDLK_KP_COLON:
                    case SDLK_KP_HASH:
                    case SDLK_KP_SPACE:
                    case SDLK_KP_AT:
                    case SDLK_KP_EXCLAM:
                    case SDLK_KP_MEMSTORE:
                    case SDLK_KP_MEMRECALL:
                    case SDLK_KP_MEMCLEAR:
                    case SDLK_KP_MEMADD:
                    case SDLK_KP_MEMSUBTRACT:
                    case SDLK_KP_MEMMULTIPLY:
                    case SDLK_KP_MEMDIVIDE:
                    case SDLK_KP_PLUSMINUS:
                    case SDLK_KP_CLEAR:
                    case SDLK_KP_CLEARENTRY:
                    case SDLK_KP_BINARY:
                    case SDLK_KP_OCTAL:
                    case SDLK_KP_DECIMAL:
                    case SDLK_KP_HEXADECIMAL:
                    {
                        modifiers |= AsUnsigned(EVENTFLAG_IS_KEY_PAD);
                        break;
                    }
                }

                break;
            }

            case SDL_MOUSEMOTION:
            {
                if (IsMouseButtonActive(event.motion.state, SDL_BUTTON_LMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_LEFT_MOUSE_BUTTON);
                }

                if (IsMouseButtonActive(event.motion.state, SDL_BUTTON_MMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_MIDDLE_MOUSE_BUTTON);
                }

                if (IsMouseButtonActive(event.motion.state, SDL_BUTTON_RMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_RIGHT_MOUSE_BUTTON);
                }

                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                if (IsMouseButton(event.button.button, SDL_BUTTON_LEFT))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_LEFT_MOUSE_BUTTON);
                }

                if (IsMouseButton(event.button.button, SDL_BUTTON_MIDDLE))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_MIDDLE_MOUSE_BUTTON);
                }

                if (IsMouseButton(event.button.button, SDL_BUTTON_RIGHT))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_RIGHT_MOUSE_BUTTON);
                }

                break;
            }

            case SDL_MOUSEWHEEL:
            {
                uint32_t state = SDL_GetMouseState(nullptr, nullptr);

                if (IsMouseButtonActive(state, SDL_BUTTON_LMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_LEFT_MOUSE_BUTTON);
                }

                if (IsMouseButtonActive(state, SDL_BUTTON_MMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_MIDDLE_MOUSE_BUTTON);
                }

                if (IsMouseButtonActive(state, SDL_BUTTON_RMASK))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_RIGHT_MOUSE_BUTTON);
                }

                break;
            }

            case SDL_WINDOWEVENT:
            {
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_LEAVE:
                    {
                        uint32_t state = SDL_GetMouseState(nullptr, nullptr);

                        if (IsMouseButtonActive(state, SDL_BUTTON_LMASK))
                        {
                            modifiers |= AsUnsigned(EVENTFLAG_LEFT_MOUSE_BUTTON);
                        }

                        if (IsMouseButtonActive(state, SDL_BUTTON_MMASK))
                        {
                            modifiers |= AsUnsigned(EVENTFLAG_MIDDLE_MOUSE_BUTTON);
                        }

                        if (IsMouseButtonActive(state, SDL_BUTTON_RMASK))
                        {
                            modifiers |= AsUnsigned(EVENTFLAG_RIGHT_MOUSE_BUTTON);
                        }

                        break;
                    }
                }

                break;
            }
        }

        return modifiers;
    }

#ifdef _WIN32
    void CefMain::HandleWindowsMessageEvent(const Event& event)
    {
        const WindowsMessageEvent& windowsMessageEvent = dynamic_cast<const WindowsMessageEvent&>(event);

        switch (windowsMessageEvent.message)
        {
            case WM_SYSCHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            {
                CefKeyEvent keyEvent;
                keyEvent.windows_key_code = static_cast<int>(windowsMessageEvent.wParam);
                keyEvent.native_key_code = static_cast<int>(windowsMessageEvent.lParam);
                keyEvent.is_system_key = windowsMessageEvent.message == WM_SYSCHAR
                        || windowsMessageEvent.message == WM_SYSKEYDOWN || windowsMessageEvent.message == WM_SYSKEYUP;
                if (windowsMessageEvent.message == WM_KEYDOWN || windowsMessageEvent.message == WM_SYSKEYDOWN)
                {
                    keyEvent.type = KEYEVENT_RAWKEYDOWN;
                }
                else if (windowsMessageEvent.message == WM_KEYUP || windowsMessageEvent.message == WM_SYSKEYUP)
                {
                    keyEvent.type = KEYEVENT_KEYUP;
                }
                else
                {
                    keyEvent.type = KEYEVENT_CHAR;
                }
                keyEvent.modifiers = GetNativeCefKeyboardModifiers(
                        static_cast<WPARAM>(windowsMessageEvent.wParam),
                        static_cast<LPARAM>(windowsMessageEvent.lParam));

                browser->GetHost()->SendKeyEvent(keyEvent);

                break;
            }
        }
    }

    // util_win.cc
    bool CefMain::IsKeyDown(WPARAM wParam)
    {
        return (::GetKeyState(static_cast<int>(wParam)) & 0x8000) != 0;
    }

    // util_win.cc
    uint32_t CefMain::GetNativeCefKeyboardModifiers(WPARAM wParam, LPARAM lParam)
    {
        uint32_t modifiers = 0;

        if (IsKeyDown(VK_SHIFT))
        {
            modifiers |= AsUnsigned(EVENTFLAG_SHIFT_DOWN);
        }
        if (IsKeyDown(VK_CONTROL))
        {
            modifiers |= AsUnsigned(EVENTFLAG_CONTROL_DOWN);
        }
        if (IsKeyDown(VK_MENU))
        {
            modifiers |= AsUnsigned(EVENTFLAG_ALT_DOWN);
        }

        // Low bit set from GetKeyState indicates "toggled".
        if (::GetKeyState(VK_NUMLOCK) & 1)
        {
            modifiers |= AsUnsigned(EVENTFLAG_NUM_LOCK_ON);
        }
        if (::GetKeyState(VK_CAPITAL) & 1)
        {
            modifiers |= AsUnsigned(EVENTFLAG_CAPS_LOCK_ON);
        }

        switch (wParam)
        {
            case VK_RETURN:
            {
                if ((lParam >> 16) & KF_EXTENDED)
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_KEY_PAD);
                }
                break;
            }

            case VK_INSERT:
            case VK_DELETE:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR:
            case VK_NEXT:
            case VK_UP:
            case VK_DOWN:
            case VK_LEFT:
            case VK_RIGHT:
            {
                if (!((lParam >> 16) & KF_EXTENDED))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_KEY_PAD);
                }
                break;
            }

            case VK_NUMLOCK:
            case VK_NUMPAD0:
            case VK_NUMPAD1:
            case VK_NUMPAD2:
            case VK_NUMPAD3:
            case VK_NUMPAD4:
            case VK_NUMPAD5:
            case VK_NUMPAD6:
            case VK_NUMPAD7:
            case VK_NUMPAD8:
            case VK_NUMPAD9:
            case VK_DIVIDE:
            case VK_MULTIPLY:
            case VK_SUBTRACT:
            case VK_ADD:
            case VK_DECIMAL:
            case VK_CLEAR:
            {
                modifiers |= AsUnsigned(EVENTFLAG_IS_KEY_PAD);
                break;
            }

            case VK_SHIFT:
            {
                if (IsKeyDown(VK_LSHIFT))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_LEFT);
                }
                else if (IsKeyDown(VK_RSHIFT))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_RIGHT);
                }
                break;
            }

            case VK_CONTROL:
            {
                if (IsKeyDown(VK_LCONTROL))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_LEFT);
                }
                else if (IsKeyDown(VK_RCONTROL))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_RIGHT);
                }
                break;
            }

            case VK_MENU:
            {
                if (IsKeyDown(VK_LMENU))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_LEFT);
                }
                else if (IsKeyDown(VK_RMENU))
                {
                    modifiers |= AsUnsigned(EVENTFLAG_IS_RIGHT);
                }
                break;
            }

            case VK_LWIN:
            {
                modifiers |= AsUnsigned(EVENTFLAG_IS_LEFT);
                break;
            }

            case VK_RWIN:
            {
                modifiers |= AsUnsigned(EVENTFLAG_IS_RIGHT);
                break;
            }
        }

        return modifiers;
    }

    void CefMain::ToggleDevToolsWindow()
    {
        if (browser->GetHost()->HasDevTools())
        {
            browser->GetHost()->CloseDevTools();
        }
        else
        {
            SDL_SysWMinfo sysInfo;
            SDL_VERSION(&sysInfo.version);
            if (SDL_GetWindowWMInfo(window, &sysInfo) == SDL_FALSE)
            {
                return;
            }

            CefBrowserSettings browserSettings;
            CefWindowInfo windowInfo;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            windowInfo.SetAsPopup(sysInfo.info.win.window, "DevTools");
            browser->GetHost()->ShowDevTools(windowInfo, uiClient, browserSettings, CefPoint(0, 0));
        }
    }
#endif

    bool CefMain::IsKeyModActive(SDL_Keymod keymod, int modifier)
    {
        return (static_cast<uint32_t>(keymod) & static_cast<uint32_t>(modifier)) != 0;
    }

    uint32_t CefMain::AsUnsigned(cef_event_flags_t flag)
    {
        return static_cast<uint32_t>(flag);
    }

    bool CefMain::IsMouseButton(uint8_t button, int type)
    {
        return button == type;
    }

    bool CefMain::IsMouseButtonActive(uint32_t state, int mask)
    {
        return (state & static_cast<uint32_t>(mask)) != 0;
    }
}
