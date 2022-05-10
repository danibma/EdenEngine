#include "Window.h"
#include "Base.h"
#include "Input.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

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
	// Based on https://github.com/ocornut/imgui/issues/707#issuecomment-430613104
	void CherryTheme() 
	{
	#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
	#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
	#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
	#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
	#define TEXT_COLOR(v) ImVec4(1.0f, 1.0f, 1.0f, v)

		auto& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_Text] = TEXT_COLOR(0.78f);
		style.Colors[ImGuiCol_TextDisabled] = TEXT_COLOR(0.28f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = BG(0.9f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = BG(1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = MED(0.78f);
		style.Colors[ImGuiCol_FrameBgActive] = MED(1.00f);
		style.Colors[ImGuiCol_TitleBg] = LOW(1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = HI(1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = BG(0.75f);
		style.Colors[ImGuiCol_MenuBarBg] = BG(0.47f);
		style.Colors[ImGuiCol_ScrollbarBg] = BG(1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = MED(0.78f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = MED(1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_ButtonHovered] = MED(0.86f);
		style.Colors[ImGuiCol_ButtonActive] = MED(1.00f);
		style.Colors[ImGuiCol_Header] = MED(0.76f);
		style.Colors[ImGuiCol_HeaderHovered] = MED(0.86f);
		style.Colors[ImGuiCol_HeaderActive] = HI(1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
		style.Colors[ImGuiCol_ResizeGripHovered] = MED(0.78f);
		style.Colors[ImGuiCol_ResizeGripActive] = MED(1.00f);
		style.Colors[ImGuiCol_PlotLines] = TEXT_COLOR(0.63f);
		style.Colors[ImGuiCol_PlotLinesHovered] = MED(1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = TEXT_COLOR(0.63f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = MED(1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = MED(0.43f);
		// [...]

		style.WindowPadding = ImVec2(6, 4);
		style.WindowRounding = 0.0f;
		style.FramePadding = ImVec2(5, 2);
		style.FrameRounding = 3.0f;
		style.ItemSpacing = ImVec2(7, 1);
		style.ItemInnerSpacing = ImVec2(1, 1);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 6.0f;
		style.ScrollbarSize = 12.0f;
		style.ScrollbarRounding = 16.0f;
		style.GrabMinSize = 20.0f;
		style.GrabRounding = 2.0f;

		style.WindowTitleAlign.x = 0.50f;

		style.Colors[ImGuiCol_Border] = ImVec4(0.539f, 0.479f, 0.255f, 0.162f);
		style.FrameBorderSize = 0.0f;
		style.WindowBorderSize = 1.0f;
	}

	Window::Window(const char* title, uint32_t width, uint32_t height)
		: m_width(width), m_height(height)
	{
		WNDCLASSEX wc		= {};
		wc.cbSize			= sizeof(wc);
		wc.lpfnWndProc		= WindowProc;
		wc.hInstance		= GetModuleHandle(nullptr);
		wc.lpszClassName	= title;
		wc.hCursor			= LoadCursor(nullptr, IDC_ARROW);
		RegisterClassEx(&wc);

		// Create the window.
		m_handle = CreateWindowEx(0,                             // Optional window styles.
								  title,						 // Window class
								  title,						 // Window text
								  WS_OVERLAPPEDWINDOW,           // Window style
   								  CW_USEDEFAULT, CW_USEDEFAULT,  // Position
								  m_width, m_height,			 // Size
								  NULL,							 // Parent window    
								  NULL,							 // Menu
								  GetModuleHandle(nullptr),		 // Instance handle
								  NULL);						 // Additional application data

		ED_ASSERT(m_handle);

		SetWindowLongPtrA(m_handle, GWLP_USERDATA, (LONG_PTR)this);

		::ShowWindow(m_handle, SW_SHOWMAXIMIZED);
		
		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		 // Enable Multi Viewports

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		CherryTheme();

		ImGui_ImplWin32_Init(m_handle);

		RAWINPUTDEVICE Rid;
		Rid.usUsagePage = 0x1 /* HID_USAGE_PAGE_GENERIC */;
		Rid.usUsage = 0x2 /* HID_USAGE_GENERIC_MOUSE */;
		Rid.dwFlags = RIDEV_INPUTSINK;
		Rid.hwndTarget = m_handle;
		if (!RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE)))
			ED_ASSERT_MB(false, "Failed to register raw input device");

		// find mouse movement
		POINT currentPos;
		GetCursorPos(&currentPos);

		// force the mouse to the center, so there's room to move
		SetCursorPos(m_width / 2, m_height / 2);

		Input::SetMousePos(currentPos.x - (m_width / 2), currentPos.y - (m_height / 2));
	}

	Window::~Window()
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
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

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void Window::SetResizeCallback(std::function<void(uint32_t, uint32_t)> resizeCallback)
	{
		m_resizeCallback = resizeCallback;
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;

		m_resizeCallback(width, height);
	}

}
