#include <condition_variable>
#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>

const char g_szClassName[] = "SimpleColourGrabber";
const int g_box_size = 30;

std::condition_variable mouse_moved;
std::mutex mouse_moved_m;

bool g_window_shown = false;

HWND g_hwnd;

COLORREF g_color_at_cursor = RGB(0,0,0);

void WriteStringToClipboard(const std::string &s)
{
	OpenClipboard(nullptr);
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

void CloseAndClipboard()
{
	POINT cursor_pos;
	GetCursorPos(&cursor_pos);

	HDC null_hdc = GetDC(NULL);
	COLORREF global_color_at_cursor = GetPixel(null_hdc, cursor_pos.x, cursor_pos.y);
	ReleaseDC(NULL, null_hdc);

	SetWindowPos(g_hwnd,
		HWND_TOPMOST,
		cursor_pos.x + 10,
		cursor_pos.y - g_box_size - 10,
		g_box_size,
		g_box_size,
		SWP_SHOWWINDOW
	);

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

	SendMessage(g_hwnd, WM_CLOSE, 0, NULL);
	DestroyWindow(g_hwnd);
	PostQuitMessage(0);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		if(wParam == WM_LBUTTONDOWN)
		{
			CloseAndClipboard();
		} else if (wParam == WM_MOUSEMOVE)
		{
			mouse_moved.notify_all();
			PostMessage(g_hwnd, WM_USER, NULL,NULL);
		}
		
	}

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void cool()
{
	static POINT last_cursor_pos = { 0, 0 };

	POINT cursor_pos;
	GetCursorPos(&cursor_pos);

	if (cursor_pos.x == last_cursor_pos.x && cursor_pos.y == last_cursor_pos.y)
		return;

	last_cursor_pos = cursor_pos;

	SetWindowPos(g_hwnd,
		HWND_TOPMOST,
		cursor_pos.x + 10,
		cursor_pos.y - g_box_size - 10,
		g_box_size,
		g_box_size,
		SWP_SHOWWINDOW
	);

	InvalidateRect(g_hwnd, NULL, FALSE);

	HDC current_dc = GetDC(g_hwnd);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(g_hwnd, &ps);
	FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(0, 0, 0)));
	++ps.rcPaint.top;
	--ps.rcPaint.bottom;
	--ps.rcPaint.right;
	++ps.rcPaint.left;
	FillRect(hdc, &ps.rcPaint, CreateSolidBrush(g_color_at_cursor));
	//FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255)));
	EndPaint(g_hwnd, &ps);

	ReleaseDC(g_hwnd, current_dc);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_USER)
	{
		cool();
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void UpdateColour()
{
	while(true) {
		std::unique_lock<std::mutex> lk(mouse_moved_m);
		mouse_moved.wait(lk);

		HDC null_hdc = GetDC(NULL);
		POINT cursor_pos;
		GetCursorPos(&cursor_pos);

		g_color_at_cursor = GetPixel(null_hdc, cursor_pos.x, cursor_pos.y);
		ReleaseDC(NULL, null_hdc);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	MSG Msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WindowProc;
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

	const HHOOK hhkLowLevelMouse = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, 0, 0);

	std::thread t1(UpdateColour);

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	UnhookWindowsHookEx(hhkLowLevelMouse);

	return Msg.wParam;
}