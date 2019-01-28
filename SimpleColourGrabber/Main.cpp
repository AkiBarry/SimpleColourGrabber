#include <windows.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>

const char g_szClassName[] = "SimpleColourGrabber";
const int g_box_size = 30;

bool g_ctrl_key_down = false;
bool g_shift_key_down = false;

bool g_window_shown = false;

HWND g_hwnd;

void WriteStringToClipboard(const std::string &s) {
	OpenClipboard(0);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
	if (!hg) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hg), s.c_str(), s.size() + 1);
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
}

void Test()
{
	POINT cursor_pos;
	GetCursorPos(&cursor_pos);

	HDC null_hdc = GetDC(NULL);
	COLORREF global_color_at_cursor = GetPixel(null_hdc, cursor_pos.x, cursor_pos.y);
	ReleaseDC(NULL, null_hdc);

	if (g_ctrl_key_down && g_shift_key_down)
	{
		SetWindowPos(g_hwnd,
			HWND_TOPMOST,
			cursor_pos.x + 10,
			cursor_pos.y - g_box_size - 10,
			g_box_size,
			g_box_size,
			SWP_SHOWWINDOW
		);

		g_window_shown = true;
	}
	else if (g_window_shown)
	{
		g_window_shown = false;

		int red = GetRValue(global_color_at_cursor);
		int green = GetGValue(global_color_at_cursor);
		int blue = GetBValue(global_color_at_cursor);

		std::stringstream ss;

		ss	<< "#"
			<< std::setfill('0') << std::setw(2)
			<< std::hex << red << green << blue;

		WriteStringToClipboard(ss.str());

		SetWindowPos(g_hwnd,
			HWND_BOTTOM,
			-g_box_size - 10,
			-g_box_size - 10,
			g_box_size,
			g_box_size,
			SWP_HIDEWINDOW | SWP_NOSIZE
		);
	}

	HDC current_dc = GetDC(g_hwnd);

	InvalidateRect(g_hwnd, NULL, FALSE);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(g_hwnd, &ps);
	FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(0, 0, 0)));
	++ps.rcPaint.top;
	--ps.rcPaint.bottom;
	--ps.rcPaint.right;
	++ps.rcPaint.left;
	FillRect(hdc, &ps.rcPaint, CreateSolidBrush(global_color_at_cursor));
	EndPaint(g_hwnd, &ps);

	ReleaseDC(g_hwnd, current_dc);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		switch (wParam)
		{
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
			if (p->vkCode == VK_LCONTROL)
			{
				g_ctrl_key_down = true;
				Test();
			}
			else if (p->vkCode == VK_LSHIFT)
			{
				g_shift_key_down = true;
				Test();
			}
		}
			break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
			if (p->vkCode == VK_LCONTROL)
			{
				g_ctrl_key_down = false;
				Test();
			}
			else if (p->vkCode == VK_LSHIFT)
			{
				g_shift_key_down = false;
				Test();
			}
		}
			break;
		default:
			break;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	MSG Msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		return 0;
	}

	g_hwnd = CreateWindowEx(
		NULL,
		g_szClassName,
		nullptr,
		WS_EX_TOOLWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, g_box_size, g_box_size,
		NULL, NULL, hInstance, NULL);

	LONG lStyle = GetWindowLong(g_hwnd, GWL_STYLE);
	lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_EX_APPWINDOW);
	SetWindowLong(g_hwnd, GWL_STYLE, lStyle);

	if (g_hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(g_hwnd, SW_HIDE);
	UpdateWindow(g_hwnd);

	const HHOOK hhkLowLevelKybd = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	UnhookWindowsHookEx(hhkLowLevelKybd);

	return Msg.wParam;
}