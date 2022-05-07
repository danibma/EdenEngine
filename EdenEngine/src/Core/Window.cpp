#include "Window.h"
#include "Base.h"
#include "Input.h"

static LRESULT CALLBACK WindowProc(HWND handle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
				else
				{
					UINT width = LOWORD(lParam);
					UINT height = HIWORD(lParam);
					window->Resize(width, height);
				}
			}
			break;
		case WM_DESTROY:
			window->CloseWasRequested();
			return 0;
		case WM_CHAR:
		case WM_SETCURSOR:
		case WM_DEVICECHANGE:
		case WM_MOUSEMOVE:
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

		ShowWindow(m_Handle, SW_SHOWMAXIMIZED);
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

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;

		ED_LOG_TRACE("Window was resized: {}x{}", m_Width, m_Height);
	}

}