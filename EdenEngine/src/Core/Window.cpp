#include "Window.h"
#include "Base.h"
#include "Input.h"
#include "UI.h"

#include <imgui/backends/imgui_impl_win32.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WindowProc(HWND handle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(handle, uMsg, wParam, lParam))
		return true;

	Eden::Window* window = (Eden::Window*)GetWindowLongPtrA(handle, GWLP_USERDATA);
	if (window)
	{
		switch (uMsg)
		{
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
			Eden::Input::HandleInput(uMsg, (uint32_t)wParam, (uint32_t)lParam);
			break;
		}
	}

	return DefWindowProc(handle, uMsg, wParam, lParam);
}

namespace Eden
{
	Window::Window(const char* title, uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		WNDCLASSEX wc		= {};
		wc.cbSize			= sizeof(wc);
		wc.lpfnWndProc		= WindowProc;
		wc.hInstance		= GetModuleHandle(nullptr);
		wc.lpszClassName	= title;
		wc.hCursor			= LoadCursor(nullptr, IDC_ARROW);
		RegisterClassEx(&wc);

		// Create the window.
		m_Handle = CreateWindowEx(0,                             // Optional window styles.
								  title,						 // Window class
								  title,						 // Window text
								  WS_OVERLAPPEDWINDOW,           // Window style
   								  CW_USEDEFAULT, CW_USEDEFAULT,  // Position
								  m_Width, m_Height,			 // Size
								  NULL,							 // Parent window    
								  NULL,							 // Menu
								  GetModuleHandle(nullptr),		 // Instance handle
								  NULL);						 // Additional application data

		ED_ASSERT(m_Handle);

		SetWindowLongPtrA(m_Handle, GWLP_USERDATA, (LONG_PTR)this);

		::ShowWindow(m_Handle, SW_SHOWMAXIMIZED);
		
		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		 // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		 // Enable Multi Viewports

		io.Fonts->AddFontFromFileTTF("assets/Fonts/Roboto/Roboto-Bold.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.Fonts->AddFontFromFileTTF("assets/Fonts/Roboto/Roboto-Regular.ttf", 24.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/Fonts/Roboto/Roboto-SemiMedium.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

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
			ED_ASSERT_MB(false, "Failed to register raw input device");

		// find mouse movement
		POINT currentPos;
		GetCursorPos(&currentPos);

		// force the mouse to the center, so there's room to move
		SetCursorPos(m_Width / 2, m_Height / 2);

		Input::SetMousePos(currentPos.x - (m_Width / 2), currentPos.y - (m_Height / 2));
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

		if (!IsMinimized())
		{
			
		}
	}

	void Window::SetResizeCallback(std::function<void(uint32_t, uint32_t)> resizeCallback)
	{
		m_ResizeCallback = resizeCallback;
	}

	void Window::ResizeCallback()
	{
		m_ResizeCallback(m_Width, m_Height);
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;

		m_IsMinimized = false;
	}

}
