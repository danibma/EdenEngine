#include "Window.h"
#include "Base.h"
#include "Assertions.h"
#include "Input.h"
#include "Editor/Icons/IconsFontAwesome6.h"

#include <imgui/ImguiHelper.h>
#include <imgui/backends/imgui_impl_win32.h>
#include "Editor/Editor.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WindowProc(HWND handle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(handle, uMsg, wParam, lParam))
		return true;

	Eden::Window* window = (Eden::Window*)GetWindowLongPtrA(handle, GWLP_USERDATA);

	auto ValidateIfMaximized = [&handle, window](RECT& rect)
	{
		if (!window->IsMaximized())
			return;
		HMONITOR monitor = ::MonitorFromWindow(handle, MONITOR_DEFAULTTOPRIMARY);
		if (!monitor)
			return;
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (!::GetMonitorInfoW(monitor, &monitorInfo))
			return;
		rect = monitorInfo.rcWork;
	};

	auto hitTest = [&handle, window](POINT cursor)
	{
		// identify borders and corners to allow resizing the window.
		// Note: On Windows 10, windows behave differently and
		// allow resizing outside the visible window frame.
		// This implementation does not replicate that behavior.
		const POINT border = 
		{
			::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
			::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
		};
		RECT windowRect;
		if (!::GetWindowRect(handle, &windowRect))
			return HTNOWHERE;

		enum region_mask {
			client = 0b0000,
			left = 0b0001,
			right = 0b0010,
			top = 0b0100,
			bottom = 0b1000,
		};

		const auto result =
			left   * (cursor.x  < (windowRect.left   + border.x)) |
			right  * (cursor.x >= (windowRect.right  - border.x)) |
			top    * (cursor.y  < (windowRect.top    + border.y)) |
			bottom * (cursor.y >= (windowRect.bottom - border.y));

		if (Eden::EdenEd::IsTitleBarHovered() && result != top)
			return HTCAPTION;

		switch (result) 
		{
		case left: return HTLEFT;
		case right: return HTRIGHT;
		case top: return HTTOP;
		case bottom: return HTBOTTOM;
		case top | left: return HTTOPLEFT;
		case top | right: return HTTOPRIGHT;
		case bottom | left: return HTBOTTOMLEFT;
		case bottom | right: return HTBOTTOMRIGHT;
		default: return HTCLIENT;
		}
	};

	if (window)
	{
		switch (uMsg)
		{
#if WITH_EDITOR
		case WM_NCCALCSIZE: 
		{
			if (wParam == TRUE) 
			{
				auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				ValidateIfMaximized(params.rgrc[0]);
				return 0;
			}
			break;
		}
		case WM_NCHITTEST: 
		{
			// When we have no border or title bar, we need to perform our
			// own hit testing to allow resizing and moving.
			return hitTest(POINT 
				{
					((int)(short)LOWORD(lParam)),
					((int)(short)HIWORD(lParam))
				});
			break;
		}
#endif
		case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED)
			{
				window->SetMinimized(IsIconic(window->GetHandle()));
			}
			else if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) // When the user click on the maximize/"unmaximize" button on the window
			{
				UINT width = LOWORD(lParam);
				UINT height = HIWORD(lParam);
				window->Resize(width, height);
				window->ResizeCallback();
			}
			else // When the user is resizing the window manually, wait until the user stops resizing to call the resize callback
			{
				UINT width = LOWORD(lParam);
				UINT height = HIWORD(lParam);
				window->Resize(width, height);
			}
		}
		break;
		case WM_EXITSIZEMOVE:
			if (wParam == SC_SIZE)
				window->ResizeCallback();
			break;
		case WM_DESTROY:
			window->CloseWasRequested();
			return 0;
		case WM_CHAR:
		case WM_SETCURSOR:
		case WM_DEVICECHANGE:
		case WM_MOUSEMOVE:
		case WM_INPUT:
		case WM_KEYUP: case WM_SYSKEYUP:
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
		case WM_LBUTTONUP: case WM_RBUTTONUP:
		case WM_MBUTTONUP: case WM_XBUTTONUP:
		case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
		case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
			Eden::Input::HandleInput(handle, uMsg, (uint32_t)wParam, (uint32_t)lParam);
			break;
		}
	}

	return DefWindowProc(handle, uMsg, wParam, lParam);
}

namespace Eden
{
	Window::Window(const char* title, uint32_t width, uint32_t height)
		: m_Width(width)
		, m_Height(height)
		, m_DefaultTitle(title)
	{
		WNDCLASSEX wc		= {};
		wc.cbSize			= sizeof(wc);
		wc.lpfnWndProc		= WindowProc;
		wc.hInstance		= GetModuleHandle(nullptr);
		wc.lpszClassName	= title;
		wc.hCursor			= LoadCursor(nullptr, IDC_ARROW);
		wc.hIcon			= LoadIcon(GetModuleHandle(nullptr), "IDI_ICON");
		wc.hIconSm			= LoadIcon(GetModuleHandle(nullptr), "IDI_ICON");
		RegisterClassEx(&wc);

#if WITH_EDITOR
		int windowStyle = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_BORDER;
#else
		int windowStyle = WS_OVERLAPPEDWINDOW;
#endif

		// Create the window.
		m_Handle = CreateWindowEx(0,                             // Optional window styles.
								  title,						 // Window class
								  title,						 // Window text
								  windowStyle,					 // Window style
   								  CW_USEDEFAULT, CW_USEDEFAULT,  // Position
								  m_Width, m_Height,			 // Size
								  NULL,							 // Parent window    
								  NULL,							 // Menu
								  GetModuleHandle(nullptr),		 // Instance handle
								  NULL);						 // Additional application data

		ensure(m_Handle);

		SetWindowLongPtrA(m_Handle, GWLP_USERDATA, (LONG_PTR)this);
#if WITH_EDITOR
		::SetWindowPos(m_Handle, 0, 0, 0, m_Width, m_Height, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
#endif
		Maximize();
		
		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		 // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		 // Enable Multi Viewports

		io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Bold.ttf", 19.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", 25.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-SemiMedium.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

		// merge in icons from Font Awesome
		static const ImWchar iconsRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		ImFontConfig iconsConfig; iconsConfig.MergeMode = true; iconsConfig.PixelSnapH = true;
		io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FAS, 14.0f, &iconsConfig, iconsRanges);
		// NOTE: use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		switch (UI::g_SelectedTheme)
		{
		case UI::Themes::Cherry: 
			UI::CherryTheme();
			break;
		case UI::Themes::Hazel:
			UI::HazelTheme();
			break;
		}

		ImGui_ImplWin32_Init(m_Handle);

		RAWINPUTDEVICE Rid;
		Rid.usUsagePage = 0x1 /* HID_USAGE_PAGE_GENERIC */;
		Rid.usUsage = 0x2 /* HID_USAGE_GENERIC_MOUSE */;
		Rid.dwFlags = RIDEV_INPUTSINK;
		Rid.hwndTarget = m_Handle;
		if (!RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE)))
			ensureMsg(false, "Failed to register raw input device");
	}

	Window::~Window()
	{
	}

	void Window::UpdateEvents()
	{
		// Run the message loop.
		MSG msg = {};

		Input::UpdateInput();

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	void Window::SetResizeCallback(std::function<void(uint32_t, uint32_t)> resizeCallback)
	{
		m_ResizeCallback = resizeCallback;
	}

	void Window::Maximize()
	{
		if (IsMaximized())
			::ShowWindow(m_Handle, SW_RESTORE);
		else
			::ShowWindow(m_Handle, SW_SHOWMAXIMIZED);
	}

	void Window::Minimize()
	{
		::ShowWindow(m_Handle, SW_MINIMIZE);
	}

	bool Window::IsMaximized()
	{
		WINDOWPLACEMENT placement = {};
		if (!::GetWindowPlacement(m_Handle, &placement)) {
			return false;
		}

		return placement.showCmd == SW_MAXIMIZE;
	}

	void Window::ChangeTitle(const std::string& title)
	{
		m_CurrentTitle = m_DefaultTitle + " | " + title;
		SetWindowTextA(m_Handle, m_CurrentTitle.c_str());
	}

	void Window::ResizeCallback()
	{
		if (!Renderer::IsInitialized())
			return;
		Renderer::GetCurrentScene()->AddPreparation([this]() {
			m_ResizeCallback(m_Width, m_Height);
#if !WITH_EDITOR
			Renderer::SetViewportSize(static_cast<float>(m_Width), static_cast<float>(m_Height));
#endif
		});
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;

		m_bIsMinimized = false;
	}

}
