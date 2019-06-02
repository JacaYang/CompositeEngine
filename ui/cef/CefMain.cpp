#include "CefMain.h"

#include "event/core/EventSystem.h"
#include "event/SdlEvent.h"

#include "include/cef_app.h"
#include "cef/client/UIClient.h"
#include "cef/client/UIRenderHandler.h"
#include "cef/browser/UIAppBrowser.h"
#include "cef/browser/UIBrowserProcessHandler.h"
#include "cef/client/UIRequestHandler.h"
#include "cef/client/UILifeSpanHandler.h"
#include "cef/browser/UIQueryResponder.h"
#include "cef/browser/UIExternalMessagePump.h"
#include "include/wrapper/cef_message_router.h"
#include "cef/browser/UIQueryHandler.h"

#include <SDL_syswm.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>

#include "cef/WindowContentView.h"

#include "include/wrapper/cef_library_loader.h"
#endif

namespace CE
{
	CefMain::CefMain(
            EventSystem* eventSystem,
            SDL_Window* g_window)
		: eventSystem(eventSystem)
        , g_window(g_window)
	{
		eventSystem->RegisterListener(this, EventType::SDL);
	}

    bool CefMain::StartCef(
            int argc,
            char* argv[],
            uint32_t SCREEN_WIDTH,
            uint32_t SCREEN_HEIGHT)
    {
#ifdef __APPLE__
        CefScopedLibraryLoader libraryLoader;
        if (!libraryLoader.LoadInMain())
        {
            printf("CEF failed to load framework in engine.\n");
            return false;
        }
#endif

        CefMainArgs main_args;
#ifdef _WIN32
        main_args = CefMainArgs(::GetModuleHandle(NULL));
#elif __APPLE__
        main_args = CefMainArgs(argc, argv);
#endif

	    queryHandler = new UIQueryHandler(eventSystem, new UIQueryResponder(eventSystem));
        externalMessagePump = new UIExternalMessagePump();
        CefRefPtr<UIBrowserProcessHandler> browserProcessHandler = new UIBrowserProcessHandler(externalMessagePump);
        CefRefPtr<UIAppBrowser> app = new UIAppBrowser(browserProcessHandler);

        CefSettings settings;
        settings.no_sandbox = true;
        settings.external_message_pump = true;
        settings.windowless_rendering_enabled = true;
        settings.remote_debugging_port = 3469;
#ifdef _WIN32
        CefString(&settings.browser_subprocess_path).FromASCII("CompositeCefSubprocess.exe");
#elif __APPLE__
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        // TODO: Free? See https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFMemoryMgmt/Concepts/Ownership.html#//apple_ref/doc/uid/20001148-103029
        CFURLRef privateFrameworksUrl = CFBundleCopyPrivateFrameworksURL(mainBundle);
        UInt8 privateFrameworksDirectoryName[1024];
        CFURLGetFileSystemRepresentation(privateFrameworksUrl, true, privateFrameworksDirectoryName, 1024);

        std::string subprocessFile = (char*) privateFrameworksDirectoryName;
        subprocessFile += "/CompositeCefSubprocess.app/Contents/MacOS/CompositeCefSubprocess";
        CefString(&settings.browser_subprocess_path).FromString(subprocessFile);
#endif

        if (!CefInitialize(main_args, settings, app, NULL))
        {
            printf("CEF failed to initialize.\n");
            return false;
        }

        CefMessageRouterConfig messageRouterConfig;
        CefRefPtr<CefMessageRouterBrowserSide> messageRouterBrowserSide = CefMessageRouterBrowserSide::Create(messageRouterConfig);
        messageRouterBrowserSide->AddHandler(queryHandler, true);

        CefRefPtr<UIContextMenuHandler> contextMenuHandler = new UIContextMenuHandler();
        CefRefPtr<UIRenderHandler> renderHandler = new UIRenderHandler(SCREEN_WIDTH, SCREEN_HEIGHT);
        CefRefPtr<UILifeSpanHandler> lifeSpanHandler = new UILifeSpanHandler(messageRouterBrowserSide);
        CefRefPtr<UILoadHandler> loadHandler = new UILoadHandler();
        CefRefPtr<UIRequestHandler> requestHandler = new UIRequestHandler(messageRouterBrowserSide);
        g_uiClient = new UIClient(
            contextMenuHandler,
            renderHandler,
            lifeSpanHandler,
            loadHandler,
            requestHandler,
            messageRouterBrowserSide);

        SDL_SysWMinfo sysInfo;
        SDL_VERSION(&sysInfo.version);
        if (!SDL_GetWindowWMInfo(g_window, &sysInfo))
        {
            return false;
        }

        CefBrowserSettings browserSettings;
        CefWindowInfo windowInfo;
#ifdef _WIN32
        windowInfo.SetAsWindowless(sysInfo.info.win.window);
#elif __APPLE__
        NSView* view = (NSView*) CE::GetWindowContentView(sysInfo.info.cocoa.window);
        windowInfo.SetAsWindowless(view);
#endif

        g_browser = CefBrowserHost::CreateBrowserSync(
            windowInfo,
            g_uiClient,
            "http://localhost:3000", // "about:blank"
            browserSettings,
            nullptr);
        
        return true;
    }

    void CefMain::StopCef()
    {
        externalMessagePump->Shutdown();

        g_browser->GetHost()->CloseBrowser(true);

        g_browser = nullptr;
        g_uiClient = nullptr;

        CefShutdown();
    }

    const char* CefMain::GetViewBuffer()
    {
        return ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetViewBuffer();
    }

    const char* CefMain::GetPopupBuffer()
    {
        return ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetPopupBuffer();
    }

    const CefRect& CefMain::GetPopupRect()
    {
        return ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetPopupRect();
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
		}
	}

	void CefMain::HandleSdlEvent(const Event& event)
    {
		const SdlEvent& wrappedEvent = reinterpret_cast<const SdlEvent&>(event);

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

                g_browser->GetHost()->SendMouseMoveEvent(mouseEvent, false);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEBUTTONDOWN:
            {
                CefBrowserHost::MouseButtonType mouseButtonType;
                if (nativeEvent.button.button == SDL_BUTTON_LEFT)
                {
                    mouseButtonType = MBT_LEFT;
                }
                else if (nativeEvent.button.button == SDL_BUTTON_MIDDLE)
                {
                    mouseButtonType = MBT_MIDDLE;
                }
                else if (nativeEvent.button.button == SDL_BUTTON_RIGHT)
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

                g_browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, false, nativeEvent.button.clicks);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEBUTTONUP:
            {
                CefBrowserHost::MouseButtonType mouseButtonType;
                if (nativeEvent.button.button == SDL_BUTTON_LEFT)
                {
                    mouseButtonType = MBT_LEFT;
                }
                else if (nativeEvent.button.button == SDL_BUTTON_MIDDLE)
                {
                    mouseButtonType = MBT_MIDDLE;
                }
                else if (nativeEvent.button.button == SDL_BUTTON_RIGHT)
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

                g_browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, true, nativeEvent.button.clicks);

                break;
            }

            // osr_window_win.cc
            // browser_window_osr_mac.mm
            case SDL_MOUSEWHEEL:
            {
                CefMouseEvent mouseEvent;
                SDL_GetMouseState(&mouseEvent.x, &mouseEvent.y);
                mouseEvent.modifiers = GetSdlCefInputModifiers(nativeEvent);

                g_browser->GetHost()->SendMouseWheelEvent(mouseEvent, nativeEvent.wheel.x, nativeEvent.wheel.y);

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

                        g_browser->GetHost()->SendMouseMoveEvent(mouseEvent, true);
                        
                        break;
                    }
                }

                break;
            }
        }
    }

    // osr_window_win.cc
    // browser_window_osr_mac.mm
    unsigned CefMain::GetSdlCefInputModifiers(const SDL_Event& event)
    {
        unsigned modifiers = 0;

        SDL_Keymod keymod = SDL_GetModState();

        if (keymod & KMOD_CTRL)
        {
            modifiers |= EVENTFLAG_CONTROL_DOWN;
        }

        if (keymod & KMOD_SHIFT)
        {
            modifiers |= EVENTFLAG_SHIFT_DOWN;
        }

        if (keymod & KMOD_ALT)
        {
            modifiers |= EVENTFLAG_ALT_DOWN;
        }

        if (keymod & KMOD_NUM)
        {
            modifiers |= EVENTFLAG_NUM_LOCK_ON;
        }

        if (keymod & KMOD_CAPS)
        {
            modifiers |= EVENTFLAG_CAPS_LOCK_ON;
        }

#ifdef __APPLE__
        if (keymod & KMOD_GUI)
        {
            modifiers |= EVENTFLAG_COMMAND_DOWN;
        }
#endif

        // todo: if mouse-only, still keep these two if's?
        if (keymod & KMOD_LSHIFT
            || keymod & KMOD_LCTRL
            || keymod & KMOD_LALT
            || keymod & KMOD_LGUI)
        {
            modifiers |= EVENTFLAG_IS_LEFT;
        }

        if (keymod & KMOD_RSHIFT
            || keymod & KMOD_RCTRL
            || keymod & KMOD_RALT
            || keymod & KMOD_RGUI)
        {
            modifiers |= EVENTFLAG_IS_RIGHT;
        }

        switch (event.type)
        {
            // todo: remove and make this function mouse-only?
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                switch(event.key.keysym.sym)
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
                        modifiers |= EVENTFLAG_IS_KEY_PAD;
                        break;
                    }
                }

                break;
            }

            case SDL_MOUSEMOTION:
            {
                if (event.motion.state & SDL_BUTTON_LMASK)
                {
                    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
                }

                if (event.motion.state & SDL_BUTTON_MMASK)
                {
                    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
                }

                if (event.motion.state & SDL_BUTTON_RMASK)
                {
                    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
                }

                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
                }

                if (event.button.button == SDL_BUTTON_MIDDLE)
                {
                    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
                }

                if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
                }

                break;
            }

            case SDL_MOUSEWHEEL:
            {
                unsigned state = SDL_GetMouseState(NULL, NULL);

                if (state & SDL_BUTTON_LMASK)
                {
                    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
                }

                if (state & SDL_BUTTON_MMASK)
                {
                    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
                }

                if (state & SDL_BUTTON_RMASK)
                {
                    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
                }

                break;
            }

            case SDL_WINDOWEVENT:
            {
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_LEAVE:
                    {
                        unsigned state = SDL_GetMouseState(NULL, NULL);

                        if (state & SDL_BUTTON_LMASK)
                        {
                            modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
                        }

                        if (state & SDL_BUTTON_MMASK)
                        {
                            modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
                        }

                        if (state & SDL_BUTTON_RMASK)
                        {
                            modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
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
    void CefMain::ToggleDevToolsWindow()
    {
        if (g_browser->GetHost()->HasDevTools())
        {
            g_browser->GetHost()->CloseDevTools();
        }
        else
        {
            SDL_SysWMinfo sysInfo;
            SDL_VERSION(&sysInfo.version);
            if (!SDL_GetWindowWMInfo(g_window, &sysInfo))
            {
                return;
            }

            CefBrowserSettings browserSettings;
            CefWindowInfo windowInfo;
            windowInfo.SetAsPopup(sysInfo.info.win.window, "DevTools");
            g_browser->GetHost()->ShowDevTools(windowInfo, g_uiClient, browserSettings, CefPoint(0, 0));
        }
    }
#endif
}
