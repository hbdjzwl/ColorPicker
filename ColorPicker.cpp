#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <dwmapi.h>

#include "resource.h" 

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")
#pragma comment(linker, "/STACK:16777216")

using namespace std;
using namespace Gdiplus;

// ================= 全局变量 =================
ULONG_PTR gdiplusToken;

int currentH = 0; int currentS = 255; int currentV = 255;
int currentR = 255, currentG = 0, currentB = 0;
bool dragSV = false; bool dragHue = false;

bool isPicking = false;
int previewR = 0, previewG = 0, previewB = 0;
bool safeToCheckClick = false;
bool safeToCheckRightBtn = false;

HWND g_hMagnifier = nullptr;
HWND g_hMaskOverlay = nullptr;
HWND g_hMainWnd = nullptr;

HWND g_hEditHex = nullptr;
HWND g_hEditDec = nullptr;
bool g_isUpdatingEditBoxes = false;

const int MAG_SIZE = 90;
const int MAG_PIXELS = 9;
const int CELL_SIZE = MAG_SIZE / MAG_PIXELS;

HBITMAP g_hbmScreen = NULL;
DWORD* g_pScreenBits = NULL;
int g_virtualScreenX = 0;
int g_virtualScreenY = 0;
int g_virtualScreenW = 0;
int g_virtualScreenH = 0;
HDC g_hdcSnapshot = NULL;

bool hoverCopyHex = false;
bool hoverCopyDec = false;
bool hoverPick = false;
wstring statusMsg = L"点击左侧面板或输入颜色";

const int TITLE_H = 36;
bool hoverClose = false;
bool hoverMin = false;
bool isDraggingWindow = false;
POINT ptDragStart = { 0, 0 };
RECT rectWindowStart = { 0, 0 };

const int WIN_W = 700, WIN_H = 430;
const int LEFT_X = 30, LEFT_Y = 70, CARD_W = 320, CARD_H = 370;
const int SV_X = 50, SV_Y = 90, SV_W = 280, SV_H = 180;
const int HUE_X = 50, HUE_Y = 285, HUE_W = 280, HUE_H = 16;
const int TEXT_HEX_Y = 315;
const int INPUT1_Y = 335;
const int TEXT_DEC_Y = 375;
const int INPUT2_Y = 395;
const int BTN_COPY_X = 245, BTN_COPY_W = 60, BTN_COPY_H = 30;

// 拖动实时数字只重绘输入框中间的文字区域，完全不触碰边框和四个圆角。
const int LIVE_TEXT_X = 90;
const int LIVE_TEXT_W = 100;
const int LIVE_TEXT_TOP = 6;
const int LIVE_TEXT_H = 14;

// ====== 缓存的 GDI / GDI+ 资源 ======
// 每帧重建字体和图标不仅慢，图标不释放还会在长时间拖动后耗尽 USER/GDI 句柄，
// 导致父窗口 CreateCompatibleBitmap 失败、整窗变黑。这些资源只创建一次。
HICON g_hIcon20 = NULL;
HICON g_hIcon28 = NULL;
HFONT g_hEditFont = NULL;
HBRUSH g_hEditBgBrush = NULL;

HDC g_hdcBack = NULL;
HBITMAP g_hbmBack = NULL;
HGDIOBJ g_hOldBackBmp = NULL;
int g_backW = 0;
int g_backH = 0;

struct UIFonts {
    Font* titleBar;
    Font* main;
    Font* bigTitle;
    Font* rgb;
    Font* pickBtn;
    Font* status;
};
UIFonts g_fonts = {};

// 拖动调色期间为 true：两个 EDIT 被隐藏，实时数值由父窗口双缓冲绘制。
bool g_showLiveValues = false;

// 色板/色相条按下时的起点，用于区分“点击”和“拖动”。
POINT g_colorDragStart = { 0, 0 };

void DrawRoundRect(Graphics& g, Brush* brush, Pen* pen, Rect rect, int radius) {
    GraphicsPath path;
    int d = radius * 2;
    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.GetRight() - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.GetRight() - d, rect.GetBottom() - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
    if (brush) g.FillPath(brush, &path);
    if (pen) g.DrawPath(pen, &path);
}

void EnsureUIFonts() {
    if (!g_fonts.titleBar) g_fonts.titleBar = new Font(L"Microsoft YaHei", 14, FontStyleRegular, UnitPixel);
    if (!g_fonts.main) g_fonts.main = new Font(L"Microsoft YaHei", 12, FontStyleRegular, UnitPixel);
    if (!g_fonts.bigTitle) g_fonts.bigTitle = new Font(L"Microsoft YaHei", 18, FontStyleBold, UnitPixel);
    if (!g_fonts.rgb) g_fonts.rgb = new Font(L"Consolas", 14, FontStyleBold, UnitPixel);
    if (!g_fonts.pickBtn) g_fonts.pickBtn = new Font(L"Microsoft YaHei", 14, FontStyleBold, UnitPixel);
    if (!g_fonts.status) g_fonts.status = new Font(L"Microsoft YaHei", 12, FontStyleRegular, UnitPixel);
}

void ReleaseUIFonts() {
    delete g_fonts.titleBar; g_fonts.titleBar = nullptr;
    delete g_fonts.main; g_fonts.main = nullptr;
    delete g_fonts.bigTitle; g_fonts.bigTitle = nullptr;
    delete g_fonts.rgb; g_fonts.rgb = nullptr;
    delete g_fonts.pickBtn; g_fonts.pickBtn = nullptr;
    delete g_fonts.status; g_fonts.status = nullptr;
}

void ReleaseBackBuffer() {
    if (g_hdcBack) {
        if (g_hOldBackBmp) SelectObject(g_hdcBack, g_hOldBackBmp);
        if (g_hbmBack) DeleteObject(g_hbmBack);
        DeleteDC(g_hdcBack);
    }
    g_hdcBack = NULL;
    g_hbmBack = NULL;
    g_hOldBackBmp = NULL;
    g_backW = 0;
    g_backH = 0;
}

// 返回 false 表示后台缓冲创建失败，调用方应退回直接绘制。
bool EnsureBackBuffer(HDC hdcRef, int w, int h, bool& recreated) {
    recreated = false;
    if (g_hdcBack && g_backW == w && g_backH == h) return true;

    ReleaseBackBuffer();

    HDC hdcMem = CreateCompatibleDC(hdcRef);
    if (!hdcMem) return false;
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcRef, w, h);
    if (!hbmMem) {
        DeleteDC(hdcMem);
        return false;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hbmMem);
    g_hdcBack = hdcMem;
    g_hbmBack = hbmMem;
    g_hOldBackBmp = hOld;
    g_backW = w;
    g_backH = h;
    recreated = true;
    return true;
}

// 拖动调色时只让真正变化的区域失效，避免每次整窗重绘造成的卡顿。
void InvalidateColorPreview(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;

    RECT rSV = { SV_X - 12, SV_Y - 12, SV_X + SV_W + 12, SV_Y + SV_H + 12 };
    InvalidateRect(hwnd, &rSV, FALSE);

    RECT rHue = { HUE_X - 12, HUE_Y - 12, HUE_X + HUE_W + 12, HUE_Y + HUE_H + 12 };
    InvalidateRect(hwnd, &rHue, FALSE);

    int rightCenterX = LEFT_X + CARD_W + (w - (LEFT_X + CARD_W)) / 2;
    int yOffset = 30;
    int rightCardW = 290;
    int rightCardX = rightCenterX - rightCardW / 2;
    RECT rCard = { rightCardX - 2, 85 + yOffset - 2, rightCardX + rightCardW + 2, 85 + yOffset + 195 + 2 };
    InvalidateRect(hwnd, &rCard, FALSE);

    if (g_showLiveValues) {
        RECT rText1 = { LIVE_TEXT_X, INPUT1_Y + LIVE_TEXT_TOP,
                        LIVE_TEXT_X + LIVE_TEXT_W, INPUT1_Y + LIVE_TEXT_TOP + LIVE_TEXT_H };
        RECT rText2 = { LIVE_TEXT_X, INPUT2_Y + LIVE_TEXT_TOP,
                        LIVE_TEXT_X + LIVE_TEXT_W, INPUT2_Y + LIVE_TEXT_TOP + LIVE_TEXT_H };
        InvalidateRect(hwnd, &rText1, FALSE);
        InvalidateRect(hwnd, &rText2, FALSE);
    }
}

void UpdateColorFromHSV() {
    float h = currentH / 360.0f, s = currentS / 255.0f, v = currentV / 255.0f;
    float r, g, b;
    int i = (int)(h * 6); float f = h * 6 - i;
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
    currentR = (int)(r * 255); currentG = (int)(g * 255); currentB = (int)(b * 255);
}

void UpdateHSVFromColor(int r, int g, int b) {
    float rr = r / 255.0f, gg = g / 255.0f, bb = b / 255.0f;
    float max_val = max({ rr, gg, bb }), min_val = min({ rr, gg, bb });
    float d = max_val - min_val;
    currentV = (int)(max_val * 255);
    currentS = (max_val == 0) ? 0 : (int)((d / max_val) * 255);
    if (max_val == min_val) currentH = 0;
    else {
        float h;
        if (max_val == rr) h = (gg - bb) / d + (gg < bb ? 6 : 0);
        else if (max_val == gg) h = (bb - rr) / d + 2;
        else h = (rr - gg) / d + 4;
        currentH = (int)(h * 60);
    }
}

wstring GetHexStr(int r, int g, int b) {
    wchar_t buf[16]; wsprintfW(buf, L"#%02X%02X%02X", r, g, b); return buf;
}
wstring GetDecStr(int r, int g, int b) {
    unsigned long argb = (0xFFul << 24) | ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b;
    wchar_t buf[32]; wsprintfW(buf, L"%lu", argb); return buf;
}

// 只有文本真正变化时才调用 SetWindowTextW，避免多余的子控件重绘。
static void SetEditTextIfChanged(HWND hEdit, const wstring& text) {
    wchar_t buf[64];
    GetWindowTextW(hEdit, buf, 64);
    if (text != buf) SetWindowTextW(hEdit, text.c_str());
}

// forceBoth = true 用于拖动/取色过程：此时鼠标已被主窗口捕获，
// 不存在用户正在输入的情况，两个输入框都应强制同步。
void UpdateEditBoxes(bool forceBoth = false) {
    if (!g_hEditHex || !g_hEditDec || g_isUpdatingEditBoxes) return;
    g_isUpdatingEditBoxes = true;
    wstring hex = GetHexStr(currentR, currentG, currentB);
    wstring dec = GetDecStr(currentR, currentG, currentB);
    if (forceBoth || GetFocus() != g_hEditHex) SetEditTextIfChanged(g_hEditHex, hex);
    if (forceBoth || GetFocus() != g_hEditDec) SetEditTextIfChanged(g_hEditDec, dec);
    g_isUpdatingEditBoxes = false;
}

// 拖动调色时隐藏两个 EDIT，改由父窗口双缓冲绘制实时数值。
// 这样数值和色板标记在同一帧更新，不会被 EDIT 子窗口的 WM_PAINT 拖延。
void SetLiveValueMode(bool on) {
    if (g_showLiveValues == on) return;
    g_showLiveValues = on;

    if (on) {
        ShowWindow(g_hEditHex, SW_HIDE);
        ShowWindow(g_hEditDec, SW_HIDE);
    }
    else {
        UpdateEditBoxes(true);
        ShowWindow(g_hEditHex, SW_SHOW);
        ShowWindow(g_hEditDec, SW_SHOW);
    }

    if (on) {
        // 只失效中间的文字区域；边框和四个圆角保持原样，不重绘。
        RECT rText1 = { LIVE_TEXT_X, INPUT1_Y + LIVE_TEXT_TOP,
                        LIVE_TEXT_X + LIVE_TEXT_W, INPUT1_Y + LIVE_TEXT_TOP + LIVE_TEXT_H };
        RECT rText2 = { LIVE_TEXT_X, INPUT2_Y + LIVE_TEXT_TOP,
                        LIVE_TEXT_X + LIVE_TEXT_W, INPUT2_Y + LIVE_TEXT_TOP + LIVE_TEXT_H };
        InvalidateRect(g_hMainWnd, &rText1, FALSE);
        InvalidateRect(g_hMainWnd, &rText2, FALSE);
    }
}

void CopyToClipboard(HWND hwnd, const wstring& text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
        statusMsg = L"✅ 已复制: " + text;
    }
}

void CaptureScreen() {
    if (g_hbmScreen) {
        SelectObject(g_hdcSnapshot, NULL);
        DeleteObject(g_hbmScreen);
        DeleteDC(g_hdcSnapshot);
        g_hbmScreen = NULL;
        g_pScreenBits = NULL;
    }

    HDC hdcScreen = GetDC(NULL);
    g_virtualScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_virtualScreenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_virtualScreenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_virtualScreenW;
    bmi.bmiHeader.biHeight = -g_virtualScreenH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_hdcSnapshot = CreateCompatibleDC(hdcScreen);
    g_hbmScreen = CreateDIBSection(g_hdcSnapshot, &bmi, DIB_RGB_COLORS, (void**)&g_pScreenBits, NULL, 0);
    SelectObject(g_hdcSnapshot, g_hbmScreen);

    BitBlt(g_hdcSnapshot, 0, 0, g_virtualScreenW, g_virtualScreenH, hdcScreen, g_virtualScreenX, g_virtualScreenY, SRCCOPY);
    ReleaseDC(NULL, hdcScreen);
}

DWORD GetSafePixel(int x, int y) {
    int relX = x - g_virtualScreenX;
    int relY = y - g_virtualScreenY;
    if (relX < 0 || relX >= g_virtualScreenW || relY < 0 || relY >= g_virtualScreenH) return 0xFF000000;
    DWORD raw = g_pScreenBits[relY * g_virtualScreenW + relX];

    BYTE b = (BYTE)(raw & 0xFF);
    BYTE g = (BYTE)((raw >> 8) & 0xFF);
    BYTE r = (BYTE)((raw >> 16) & 0xFF);
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void UpdateMagnifier(POINT pt) {
    if (!g_hMagnifier || !g_pScreenBits) return;

    Bitmap bmp(MAG_SIZE, MAG_SIZE, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    int halfPixels = MAG_PIXELS / 2;
    for (int row = 0; row < MAG_PIXELS; ++row) {
        for (int col = 0; col < MAG_PIXELS; ++col) {
            int px = pt.x - halfPixels + col;
            int py = pt.y - halfPixels + row;
            DWORD color = GetSafePixel(px, py);
            Color c(color);
            SolidBrush cellBrush(c);
            g.FillRectangle(&cellBrush, (REAL)(col * CELL_SIZE), (REAL)(row * CELL_SIZE), (REAL)CELL_SIZE, (REAL)CELL_SIZE);
        }
    }

    Pen gridPen(Color(110, 255, 255, 255), 1.0f);
    for (int i = 1; i < MAG_PIXELS; ++i) {
        g.DrawLine(&gridPen, (REAL)(i * CELL_SIZE), 0.0f, (REAL)(i * CELL_SIZE), (REAL)MAG_SIZE);
        g.DrawLine(&gridPen, 0.0f, (REAL)(i * CELL_SIZE), (REAL)MAG_SIZE, (REAL)(i * CELL_SIZE));
    }

    Pen crossPen(Color(255, 255, 255, 255), 1.5f);
    g.DrawRectangle(&crossPen, (REAL)(halfPixels * CELL_SIZE), (REAL)(halfPixels * CELL_SIZE), (REAL)CELL_SIZE, (REAL)CELL_SIZE);

    Pen borderPen(Color(255, 255, 255, 255), 1.5f);
    g.DrawRectangle(&borderPen, 0.0f, 0.0f, (REAL)MAG_SIZE - 1.0f, (REAL)MAG_SIZE - 1.0f);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmMem;
    void* pBits;

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = MAG_SIZE;
    bmi.bmiHeader.biHeight = -MAG_SIZE;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    hbmMem = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    BitmapData bmpData;
    Rect rect(0, 0, MAG_SIZE, MAG_SIZE);
    bmp.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData);
    memcpy(pBits, bmpData.Scan0, MAG_SIZE * MAG_SIZE * 4);
    bmp.UnlockBits(&bmpData);

    HGDIOBJ hOldBmp = SelectObject(hdcMem, hbmMem);

    POINT ptSrc = { 0, 0 };
    POINT ptDst = { pt.x + 15, pt.y + 15 };

    RECT workArea;
    workArea.left = g_virtualScreenX;
    workArea.top = g_virtualScreenY;
    workArea.right = g_virtualScreenX + g_virtualScreenW;
    workArea.bottom = g_virtualScreenY + g_virtualScreenH;

    if (ptDst.x + MAG_SIZE > workArea.right) ptDst.x = pt.x - MAG_SIZE - 15;
    if (ptDst.y + MAG_SIZE > workArea.bottom) ptDst.y = pt.y - MAG_SIZE - 15;

    SIZE sizeWnd = { MAG_SIZE, MAG_SIZE };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, 0 };
    UpdateLayeredWindow(g_hMagnifier, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK MagnifierProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MOUSEMOVE:
        return 0;
    case WM_SETCURSOR: return TRUE;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void ForceTopmost(HWND hwnd) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
}

static void ForceForeground(HWND hwnd) {
    HWND hFore = GetForegroundWindow();
    DWORD tidFore = hFore ? GetWindowThreadProcessId(hFore, NULL) : 0;
    DWORD tidSelf = GetCurrentThreadId();
    if (tidFore && tidFore != tidSelf) {
        AttachThreadInput(tidFore, tidSelf, TRUE);
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);
        AttachThreadInput(tidFore, tidSelf, FALSE);
    }
    else {
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);
    }
}

void EnterPickingMode(HWND hwnd) {
    isPicking = true;
    safeToCheckClick = false;
    safeToCheckRightBtn = false;

    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);

    SetWindowPos(g_hMaskOverlay, HWND_TOPMOST, g_virtualScreenX, g_virtualScreenY, g_virtualScreenW, g_virtualScreenH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    ForceTopmost(hwnd);
}

void ExitPickingMode(HWND hwnd) {
    ShowWindow(g_hMaskOverlay, SW_HIDE);
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex & ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOSENDCHANGING);
    ForceForeground(hwnd);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOSENDCHANGING);
}

void DrawTitleBar(Graphics& g, int w) {
    SolidBrush titleBg(Color(255, 15, 15, 25));
    g.FillRectangle(&titleBg, 0, 0, w + 2, TITLE_H + 1);

    Font* fontTitleBar = g_fonts.titleBar;
    SolidBrush titleTextBrush(Color(255, 200, 200, 210));

    if (g_hIcon20) {
        HDC hdc = g.GetHDC();
        DrawIconEx(hdc, 12, 10, g_hIcon20, 20, 20, 0, NULL, DI_NORMAL);
        g.ReleaseHDC(hdc);
    }

    g.DrawString(L"取色器", -1, fontTitleBar, PointF(34, 10), &titleTextBrush);

    if (hoverMin) {
        SolidBrush hBrush(Color(255, 40, 40, 50));
        DrawRoundRect(g, &hBrush, nullptr, Rect(w - 69, 4, 32, 28), 6);
    }
    if (hoverClose) {
        SolidBrush hBrush(Color(255, 230, 80, 80));
        DrawRoundRect(g, &hBrush, nullptr, Rect(w - 38, 4, 32, 28), 6);
    }

    Pen iconPen(hoverMin ? Color(255, 255, 255, 255) : Color(255, 150, 150, 150), 1.0f);
    g.DrawLine(&iconPen, w - 60, TITLE_H / 2, w - 48, TITLE_H / 2);

    Pen closePen(hoverClose ? Color(255, 255, 255, 255) : Color(255, 150, 150, 150), 1.0f);
    g.DrawLine(&closePen, w - 28, TITLE_H / 2 - 5, w - 18, TITLE_H / 2 + 5);
    g.DrawLine(&closePen, w - 28, TITLE_H / 2 + 5, w - 18, TITLE_H / 2 - 5);
}

void DrawLeftPanel(Graphics& g, int w) {
    SolidBrush textBrush(Color(255, 240, 240, 250));
    SolidBrush cardBrush(Color(255, 20, 20, 30));
    DrawRoundRect(g, &cardBrush, nullptr, Rect(LEFT_X, LEFT_Y, CARD_W, CARD_H), 16);

    int pr, pg, pb;
    {
        float hv = currentH / 360.0f; float r, gg, b; int i = (int)(hv * 6); float f = hv * 6 - i, p = 0, q = 1 - f, t = 1 - (1 - f);
    switch (i % 6) { case 0:r = 1; gg = t; b = p; break; case 1:r = q; gg = 1; b = p; break; case 2:r = p; gg = 1; b = t; break; case 3:r = p; gg = q; b = 1; break; case 4:r = t; gg = p; b = 1; break; case 5:r = 1; gg = p; b = q; break; }
                           pr = (int)(r * 255); pg = (int)(gg * 255); pb = (int)(b * 255);
    }

    SolidBrush svBase(Color(255, pr, pg, pb));
    g.FillRectangle(&svBase, SV_X, SV_Y, SV_W, SV_H);
    LinearGradientBrush whiteGrad(Point(SV_X, 0), Point(SV_X + SV_W, 0), Color(255, 255, 255, 255), Color(0, 255, 255, 255));
    g.FillRectangle(&whiteGrad, SV_X, SV_Y, SV_W, SV_H);
    LinearGradientBrush blackGrad(Point(0, SV_Y), Point(0, SV_Y + SV_H), Color(0, 0, 0, 0), Color(255, 0, 0, 0));
    g.FillRectangle(&blackGrad, SV_X, SV_Y, SV_W, SV_H);

    int cx = SV_X + (int)((currentS / 255.0f) * SV_W);
    int cy = SV_Y + (int)((1.0f - currentV / 255.0f) * SV_H);
    Pen penWhite(Color(255, 255, 255, 255), 2.0f);
    g.DrawEllipse(&penWhite, cx - 6, cy - 6, 12, 12);

    LinearGradientBrush hueGrad(Point(HUE_X, 0), Point(HUE_X + HUE_W, 0), Color(255, 255, 0, 0), Color(255, 255, 0, 0));
    Color colors[] = { Color(255,255,0,0), Color(255,255,255,0), Color(255,0,255,0), Color(255,0,255,255), Color(255,0,0,255), Color(255,255,0,255), Color(255,255,0,0) };
    REAL positions[] = { 0.0f, 1.0f / 6.0f, 2.0f / 6.0f, 3.0f / 6.0f, 4.0f / 6.0f, 5.0f / 6.0f, 1.0f };
    hueGrad.SetInterpolationColors(colors, positions, 7);
    g.FillRectangle(&hueGrad, HUE_X, HUE_Y, HUE_W, HUE_H);

    int hx = HUE_X + (int)((currentH / 360.0f) * HUE_W);
    g.DrawEllipse(&penWhite, hx - 8, HUE_Y + HUE_H / 2 - 8, 16, 16);

    Font* fontMain = g_fonts.main;
    SolidBrush subText(Color(255, 171, 175, 208));
    g.DrawString(L"十六进制(RGB):", 9, fontMain, PointF(50, (REAL)TEXT_HEX_Y), &subText);
    g.DrawString(L"十进制(ARGB):", 9, fontMain, PointF(50, (REAL)TEXT_DEC_Y), &subText);

    SolidBrush inputBg(Color(255, 55, 55, 75));
    Pen inputPen(Color(255, 100, 100, 120), 1.0f);
    DrawRoundRect(g, &inputBg, &inputPen, Rect(50, INPUT1_Y, 180, 30), 8);
    DrawRoundRect(g, &inputBg, &inputPen, Rect(50, INPUT2_Y, 180, 30), 8);

    if (g_showLiveValues) {
        // 和 EDIT 控件使用同一个 HFONT、同一个矩形和同样的颜色绘制，
        // 这样拖动中和松手后的字体、字号、居中位置保持一致。
        wstring hexValue = GetHexStr(currentR, currentG, currentB);
        wstring decValue = GetDecStr(currentR, currentG, currentB);

        HDC hdc = g.GetHDC();
        HGDIOBJ hOldFont = SelectObject(hdc, g_hEditFont);
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(55, 55, 75));
        SetTextColor(hdc, RGB(240, 240, 250));

        // EDIT 位于 INPUT_Y+4，它的内部文字比 DrawText 的 DT_VCENTER 高 4px，
        // 因此实时文字矩形从 INPUT_Y 开始，两者文字行才能对齐。
        RECT rcHex = { 53, INPUT1_Y, 53 + 174, INPUT1_Y + 24 };
        RECT rcDec = { 53, INPUT2_Y, 53 + 174, INPUT2_Y + 24 };

        // 只允许在中间文字区域绘制：OPAQUE 背景和文字都不会碰到边框/四个圆角。
        int saved = SaveDC(hdc);
        IntersectClipRect(hdc, LIVE_TEXT_X, INPUT1_Y + LIVE_TEXT_TOP,
            LIVE_TEXT_X + LIVE_TEXT_W, INPUT1_Y + LIVE_TEXT_TOP + LIVE_TEXT_H);
        DrawTextW(hdc, hexValue.c_str(), -1, &rcHex, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RestoreDC(hdc, saved);

        saved = SaveDC(hdc);
        IntersectClipRect(hdc, LIVE_TEXT_X, INPUT2_Y + LIVE_TEXT_TOP,
            LIVE_TEXT_X + LIVE_TEXT_W, INPUT2_Y + LIVE_TEXT_TOP + LIVE_TEXT_H);
        DrawTextW(hdc, decValue.c_str(), -1, &rcDec, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RestoreDC(hdc, saved);

        SelectObject(hdc, hOldFont);
        g.ReleaseHDC(hdc);
    }

    SolidBrush btnBgHex(hoverCopyHex ? Color(255, 80, 80, 110) : Color(255, 55, 55, 75));
    DrawRoundRect(g, &btnBgHex, &inputPen, Rect(BTN_COPY_X, INPUT1_Y, BTN_COPY_W, BTN_COPY_H), 8);

    SolidBrush btnBgDec(hoverCopyDec ? Color(255, 80, 80, 110) : Color(255, 55, 55, 75));
    DrawRoundRect(g, &btnBgDec, &inputPen, Rect(BTN_COPY_X, INPUT2_Y, BTN_COPY_W, BTN_COPY_H), 8);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"拷贝", 2, fontMain, RectF((REAL)BTN_COPY_X, (REAL)INPUT1_Y, (REAL)BTN_COPY_W, (REAL)BTN_COPY_H), &sf, &textBrush);
    g.DrawString(L"拷贝", 2, fontMain, RectF((REAL)BTN_COPY_X, (REAL)INPUT2_Y, (REAL)BTN_COPY_W, (REAL)BTN_COPY_H), &sf, &textBrush);
}

void DrawRightPanel(Graphics& g, int w, int h) {
    SolidBrush textBrush(Color(255, 240, 240, 250));
    SolidBrush cardBrush(Color(255, 20, 20, 30));

    int rightCenterX = LEFT_X + CARD_W + (w - (LEFT_X + CARD_W)) / 2;
    int titleAreaX = rightCenterX + 116;
    int titleAreaY = h - 60;

    if (g_hIcon28) {
        HDC hdc = g.GetHDC();
        DrawIconEx(hdc, titleAreaX - 30, titleAreaY + 25, g_hIcon28, 28, 28, 0, NULL, DI_NORMAL);
        g.ReleaseHDC(hdc);
    }

    Font* fontBigTitle = g_fonts.bigTitle;
    SolidBrush bottomTitleBrush(Color(255, 150, 180, 255));
    g.DrawString(L"取色器", 5, fontBigTitle, PointF((REAL)(titleAreaX), (REAL)titleAreaY + 25), &bottomTitleBrush);

    int yOffset = 30;
    int rightCardW = 290;
    int rightCardX = rightCenterX - rightCardW / 2;
    DrawRoundRect(g, &cardBrush, nullptr, Rect(rightCardX, 85 + yOffset, rightCardW, 195), 16);

    int swatchSize = 100;
    int swatchX = rightCenterX - swatchSize / 2;

    int displayR = isPicking ? previewR : currentR;
    int displayG = isPicking ? previewG : currentG;
    int displayB = isPicking ? previewB : currentB;
    SolidBrush swatchBrush(Color(255, displayR, displayG, displayB));
    DrawRoundRect(g, &swatchBrush, nullptr, Rect(swatchX, 105 + yOffset, swatchSize, swatchSize), 12);

    wchar_t rgbStr[64];
    wsprintfW(rgbStr, L"RGB(%d, %d, %d)", displayR, displayG, displayB);
    Font* fontRgb = g_fonts.rgb;
    SolidBrush rgbBg(Color(255, 55, 55, 75));
    Pen inputPen(Color(255, 100, 100, 120), 1.0f);
    int rgbW = 230, rgbX = rightCenterX - rgbW / 2;
    DrawRoundRect(g, &rgbBg, &inputPen, Rect(rgbX, 225 + yOffset, rgbW, 40), 20);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(rgbStr, -1, fontRgb, RectF((REAL)rgbX, (REAL)(225 + yOffset), (REAL)rgbW, 40), &sf, &textBrush);

    Font* fontPickBtn = g_fonts.pickBtn;
    SolidBrush accent(hoverPick ? Color(255, 100, 110, 255) : Color(255, 79, 91, 255));
    int pickW = 290, pickX = rightCenterX - pickW / 2;
    DrawRoundRect(g, &accent, nullptr, Rect(pickX, 305 + yOffset, pickW, 50), 12);
    g.DrawString(L"屏幕取色", 8, fontPickBtn, RectF((REAL)pickX + 11, (REAL)(305 + yOffset), (REAL)pickW, 50), &sf, &textBrush);

    Font* fontStatus = g_fonts.status;
    SolidBrush subText(Color(255, 171, 175, 208));
    g.DrawString(statusMsg.c_str(), -1, fontStatus, RectF((REAL)(rightCenterX - 150), 390.0f, 300.0f, 30.0f), &sf, &subText);
}

void ParseHexInput() {
    wchar_t buf[64];
    GetWindowTextW(g_hEditHex, buf, 64);
    wstring str(buf);
    if (str.empty() || str[0] != L'#' || str.length() != 7) return;

    try {
        int r = std::stoi(str.substr(1, 2), nullptr, 16);
        int g = std::stoi(str.substr(3, 2), nullptr, 16);
        int b = std::stoi(str.substr(5, 2), nullptr, 16);
        currentR = r; currentG = g; currentB = b;
        UpdateHSVFromColor(currentR, currentG, currentB);
        UpdateEditBoxes();              // 同步十进制框（当前获得焦点的十六进制框会被跳过）
        InvalidateColorPreview(g_hMainWnd);
    }
    catch (...) {}
}

void ParseDecInput() {
    wchar_t buf[64];
    GetWindowTextW(g_hEditDec, buf, 64);
    wstring str(buf);
    if (str.empty()) return;

    try {
        unsigned long argb = std::stoul(str);
        currentR = (argb >> 16) & 0xFF;
        currentG = (argb >> 8) & 0xFF;
        currentB = argb & 0xFF;
        UpdateHSVFromColor(currentR, currentG, currentB);
        UpdateEditBoxes();              // 同步十六进制框（当前获得焦点的十进制框会被跳过）
        InvalidateColorPreview(g_hMainWnd);
    }
    catch (...) {}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hMainWnd = hwnd;

        // 一次性创建所有会复用的资源（图标、字体、输入框画刷），避免每帧创建/泄漏。
        EnsureUIFonts();
        g_hIcon20 = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 20, 20, LR_DEFAULTCOLOR);
        g_hIcon28 = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 28, 28, LR_DEFAULTCOLOR);
        g_hEditBgBrush = CreateSolidBrush(RGB(55, 55, 75));

        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
        if (hIcon) {
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
        SetTimer(hwnd, 2, 2000, NULL);

        WNDCLASSEXW wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.lpfnWndProc = MagnifierProc;
        wcex.hInstance = GetModuleHandle(NULL);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = NULL;
        wcex.lpszClassName = L"MagnifierClass";
        RegisterClassExW(&wcex);

        g_hMagnifier = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            L"MagnifierClass", L"", WS_POPUP, 0, 0, MAG_SIZE, MAG_SIZE, hwnd, NULL, GetModuleHandle(NULL), NULL);

        WNDCLASSEXW overlayWcex = { 0 };
        overlayWcex.cbSize = sizeof(WNDCLASSEX);
        overlayWcex.lpfnWndProc = OverlayProc;
        overlayWcex.hInstance = GetModuleHandle(NULL);
        overlayWcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        overlayWcex.hbrBackground = NULL;
        overlayWcex.lpszClassName = L"MaskOverlayClass";
        RegisterClassExW(&overlayWcex);

        g_hMaskOverlay = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            L"MaskOverlayClass", L"", WS_POPUP, 0, 0, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
        SetLayeredWindowAttributes(g_hMaskOverlay, 0, 1, LWA_ALPHA);

        g_hEditHex = CreateWindowExW(0, L"EDIT", L"#FF0000",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_CENTER,
            53, INPUT1_Y + 4, 174, 24, hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);

        g_hEditDec = CreateWindowExW(0, L"EDIT", L"0",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_CENTER,
            53, INPUT2_Y + 4, 174, 24, hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL);

        g_hEditFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        SendMessage(g_hEditHex, WM_SETFONT, (WPARAM)g_hEditFont, TRUE);
        SendMessage(g_hEditDec, WM_SETFONT, (WPARAM)g_hEditFont, TRUE);
        SendMessage(g_hEditHex, EM_SETLIMITTEXT, 7, 0);
        SendMessage(g_hEditDec, EM_SETLIMITTEXT, 10, 0);

        UpdateEditBoxes();
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(240, 240, 250));
        SetBkColor(hdc, RGB(55, 55, 75));
        return (LRESULT)(g_hEditBgBrush ? g_hEditBgBrush : (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmEvent == EN_CHANGE && !g_isUpdatingEditBoxes) {
            if (wmId == 1001) ParseHexInput();
            else if (wmId == 1002) ParseDecInput();
        }
        else if (wmEvent == EN_KILLFOCUS) {
            UpdateEditBoxes();
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == 1) {
            if (!isPicking) {
                KillTimer(hwnd, 1);
                return 0;
            }
            POINT pt; GetCursorPos(&pt);
            DWORD color = GetSafePixel(pt.x, pt.y);
            previewR = (color >> 16) & 0xFF;
            previewG = (color >> 8) & 0xFF;
            previewB = color & 0xFF;

            RECT rc;
            GetClientRect(hwnd, &rc);
            RECT rightRect = { rc.right / 2, 0, rc.right, rc.bottom };
            InvalidateRect(hwnd, &rightRect, FALSE);

            UpdateMagnifier(pt);
            ForceTopmost(hwnd);
            ForceTopmost(g_hMaskOverlay);
            if (g_hMagnifier && IsWindowVisible(g_hMagnifier)) ForceTopmost(g_hMagnifier);

            if (!safeToCheckClick) {
                if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) safeToCheckClick = true;
            }
            else {
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    currentR = previewR; currentG = previewG; currentB = previewB;
                    UpdateHSVFromColor(currentR, currentG, currentB);
                    statusMsg = L"✅ 屏幕取色成功";
                    isPicking = false; safeToCheckClick = false; safeToCheckRightBtn = false;
                    KillTimer(hwnd, 1);
                    ExitPickingMode(hwnd);
                    if (g_hMagnifier) ShowWindow(g_hMagnifier, SW_HIDE);
                    if (g_hbmScreen) {
                        SelectObject(g_hdcSnapshot, NULL);
                        DeleteObject(g_hbmScreen);
                        DeleteDC(g_hdcSnapshot);
                        g_hbmScreen = NULL; g_pScreenBits = NULL;
                    }
                    UpdateEditBoxes(true); // 取色成功强制同步两个输入框
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            if (!safeToCheckRightBtn) {
                if (!(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) safeToCheckRightBtn = true;
            }
            else {
                if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                    isPicking = false; safeToCheckClick = false; safeToCheckRightBtn = false;
                    KillTimer(hwnd, 1);
                    ExitPickingMode(hwnd);
                    statusMsg = L"右键取消取色";
                    if (g_hMagnifier) ShowWindow(g_hMagnifier, SW_HIDE);
                    if (g_hbmScreen) {
                        SelectObject(g_hdcSnapshot, NULL);
                        DeleteObject(g_hbmScreen);
                        DeleteDC(g_hdcSnapshot);
                        g_hbmScreen = NULL; g_pScreenBits = NULL;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                isPicking = false; safeToCheckClick = false; safeToCheckRightBtn = false;
                KillTimer(hwnd, 1);
                ExitPickingMode(hwnd);
                statusMsg = L"取色取消";
                if (g_hMagnifier) ShowWindow(g_hMagnifier, SW_HIDE);
                if (g_hbmScreen) {
                    SelectObject(g_hdcSnapshot, NULL);
                    DeleteObject(g_hbmScreen);
                    DeleteDC(g_hdcSnapshot);
                    g_hbmScreen = NULL; g_pScreenBits = NULL;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        else if (wParam == 2) {
            KillTimer(hwnd, 2);
            statusMsg = L"点击左侧面板或输入颜色";
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        EnsureUIFonts();

        bool recreated = false;
        if (!EnsureBackBuffer(hdc, w, h, recreated)) {
            // 极端情况下（GDI 资源不足）退回直接绘制，至少不会整窗变黑。
            Graphics gDirect(hdc);
            gDirect.SetSmoothingMode(SmoothingModeAntiAlias);
            SolidBrush bgBrush(Color(255, 35, 35, 50));
            gDirect.FillRectangle(&bgBrush, 0, 0, w, h);
            DrawTitleBar(gDirect, w);
            DrawLeftPanel(gDirect, w);
            DrawRightPanel(gDirect, w, h);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // 后台缓冲是复用的；先清掉可能残留的 DC 裁剪区，避免上一帧的 clip 影响本次重绘。
        SelectClipRgn(g_hdcBack, NULL);
        Graphics g(g_hdcBack);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // 只重绘本次真正失效的区域，其余像素沿用后台缓冲中的上一帧内容。
        // 第一次创建缓冲、或尺寸变化时必须整窗重绘。
        HRGN hClip = CreateRectRgn(0, 0, 0, 0);
        int clipType = GetClipRgn(hdc, hClip);
        if (recreated || clipType != 1) {
            g.ResetClip();
        }
        else {
            g.SetClip(hClip, CombineModeReplace);
        }
        DeleteObject(hClip);

        SolidBrush bgBrush(Color(255, 35, 35, 50));
        g.FillRectangle(&bgBrush, 0, 0, w, h);
        DrawTitleBar(g, w);
        DrawLeftPanel(g, w);
        DrawRightPanel(g, w, h);

        RECT blt = recreated ? rc : ps.rcPaint;
        if (blt.right > blt.left && blt.bottom > blt.top) {
            BitBlt(hdc, blt.left, blt.top, blt.right - blt.left, blt.bottom - blt.top,
                g_hdcBack, blt.left, blt.top, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        if (isDraggingWindow) {
            POINT pt; GetCursorPos(&pt);
            int dx = pt.x - ptDragStart.x;
            int dy = pt.y - ptDragStart.y;
            SetWindowPos(hwnd, NULL, rectWindowStart.left + dx, rectWindowStart.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return 0;
        }

        RECT rcClient; GetClientRect(hwnd, &rcClient);
        int w = rcClient.right;

        bool hClose = (x >= w - 36 && x <= w - 2 && y >= 2 && y <= TITLE_H - 2);
        bool hMin = (x >= w - 64 && x < w - 36 && y >= 2 && y <= TITLE_H - 2);

        if (hClose != hoverClose || hMin != hoverMin) {
            hoverClose = hClose; hoverMin = hMin;
            InvalidateRect(hwnd, NULL, FALSE);
        }

        if (dragSV) {
            int relX = max(0, min(SV_W, x - SV_X));
            int relY = max(0, min(SV_H, y - SV_Y));
            currentS = (int)(((float)relX / SV_W) * 255.0f);
            currentV = (int)((1.0f - (float)relY / SV_H) * 255.0f);
            UpdateColorFromHSV();
            if (!g_showLiveValues) {
                // 移动超过 3px 才算拖动，才切换到父窗口实时绘制；
                // 单纯点击时保持 EDIT 显示，避免四个圆角反复切换。
                int moved = abs(x - g_colorDragStart.x) + abs(y - g_colorDragStart.y);
                if (moved >= 3) {
                    SetLiveValueMode(true);
                }
                else {
                    UpdateEditBoxes(true);
                }
            }
            // 数值由父窗口在 DrawLeftPanel 里和色板同一帧绘制，不再等 EDIT 重绘。
            InvalidateColorPreview(hwnd);
        }
        else if (dragHue) {
            int relX = max(0, min(HUE_W, x - HUE_X));
            currentH = (int)(((float)relX / HUE_W) * 360.0f);
            if (currentH >= 360) currentH = 359;
            UpdateColorFromHSV();
            if (!g_showLiveValues) {
                int moved = abs(x - g_colorDragStart.x) + abs(y - g_colorDragStart.y);
                if (moved >= 3) {
                    SetLiveValueMode(true);
                }
                else {
                    UpdateEditBoxes(true);
                }
            }
            InvalidateColorPreview(hwnd);
        }
        else {
            int rightCenterX = LEFT_X + CARD_W + (w - (LEFT_X + CARD_W)) / 2;
            int pickX = rightCenterX - 290 / 2;
            int yOffset = 30;

            bool h1 = (x >= BTN_COPY_X && x <= BTN_COPY_X + BTN_COPY_W && y >= INPUT1_Y && y <= INPUT1_Y + BTN_COPY_H);
            bool h2 = (x >= BTN_COPY_X && x <= BTN_COPY_X + BTN_COPY_W && y >= INPUT2_Y && y <= INPUT2_Y + BTN_COPY_H);
            bool h3 = (x >= pickX && x <= pickX + 290 && y >= 305 + yOffset && y <= 355 + yOffset);

            if (h1 != hoverCopyHex || h2 != hoverCopyDec || h3 != hoverPick) {
                hoverCopyHex = h1; hoverCopyDec = h2; hoverPick = h3;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        RECT rcClient; GetClientRect(hwnd, &rcClient);
        int w = rcClient.right;

        if (y <= TITLE_H) {
            if (x >= w - 36) PostMessage(hwnd, WM_CLOSE, 0, 0);
            else if (x >= w - 68) ShowWindow(hwnd, SW_MINIMIZE);
            else {
                isDraggingWindow = true;
                SetCapture(hwnd);
                GetCursorPos(&ptDragStart);
                GetWindowRect(hwnd, &rectWindowStart);
            }
            return 0;
        }

        if (x >= SV_X && x <= SV_X + SV_W && y >= SV_Y && y <= SV_Y + SV_H) {
            dragSV = true; SetCapture(hwnd);
            // 让输入框失焦，避免光标/选中状态干扰拖动过程中的数值刷新。
            SetFocus(hwnd);
            g_colorDragStart.x = x;
            g_colorDragStart.y = y;
            int relX = max(0, min(SV_W, x - SV_X));
            int relY = max(0, min(SV_H, y - SV_Y));
            currentS = (int)(((float)relX / SV_W) * 255.0f);
            currentV = (int)((1.0f - (float)relY / SV_H) * 255.0f);
            UpdateColorFromHSV();
            // 只是点击时先直接更新 EDIT；真正拖动超过阈值后再切换到覆盖层。
            UpdateEditBoxes(true);
            InvalidateColorPreview(hwnd);
        }
        else if (x >= HUE_X && x <= HUE_X + HUE_W && y >= HUE_Y - 10 && y <= HUE_Y + HUE_H + 10) {
            dragHue = true; SetCapture(hwnd);
            SetFocus(hwnd);
            g_colorDragStart.x = x;
            g_colorDragStart.y = y;
            int relX = max(0, min(HUE_W, x - HUE_X));
            currentH = (int)(((float)relX / HUE_W) * 360.0f);
            if (currentH >= 360) currentH = 359;
            UpdateColorFromHSV();
            UpdateEditBoxes(true);
            InvalidateColorPreview(hwnd);
        }
        else if (x >= BTN_COPY_X && x <= BTN_COPY_X + BTN_COPY_W && y >= INPUT1_Y && y <= INPUT1_Y + BTN_COPY_H) {
            CopyToClipboard(hwnd, GetHexStr(currentR, currentG, currentB));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (x >= BTN_COPY_X && x <= BTN_COPY_X + BTN_COPY_W && y >= INPUT2_Y && y <= INPUT2_Y + BTN_COPY_H) {
            CopyToClipboard(hwnd, GetDecStr(currentR, currentG, currentB));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else {
            int rightCenterX = LEFT_X + CARD_W + (w - (LEFT_X + CARD_W)) / 2;
            int pickX = rightCenterX - 290 / 2;
            int yOffset = 30;
            if (x >= pickX && x <= pickX + 290 && y >= 305 + yOffset && y <= 355 + yOffset) {
                if (!isPicking) {
                    statusMsg = L"左键确认，右键/ESC 取消";
                    InvalidateRect(hwnd, NULL, FALSE);
                    CaptureScreen();
                    EnterPickingMode(hwnd);
                    if (g_hMagnifier) ShowWindow(g_hMagnifier, SW_SHOW);
                    SetTimer(hwnd, 1, 16, NULL);
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (dragSV || dragHue) {
            if (g_showLiveValues) {
                SetLiveValueMode(false);   // 拖动结束：写入最终值并重新显示 EDIT
            }
            else {
                UpdateEditBoxes(true);     // 普通点击：EDIT 一直可见，刷新最终值
            }
        }
        dragSV = false; dragHue = false;
        isDraggingWindow = false;
        ReleaseCapture();
        return 0;
    }

    case WM_CAPTURECHANGED: {
        // 拖动过程中如果捕获被系统收走（例如 Alt+Tab），也要恢复输入框。
        if (dragSV || dragHue) {
            if (g_showLiveValues) {
                SetLiveValueMode(false);
            }
            dragSV = false;
            dragHue = false;
        }
        isDraggingWindow = false;
        return 0;
    }

    case WM_ERASEBKGND: return 1;

    case WM_DESTROY: {
        if (g_hbmScreen) {
            SelectObject(g_hdcSnapshot, NULL);
            DeleteObject(g_hbmScreen);
            DeleteDC(g_hdcSnapshot);
        }
        PostQuitMessage(0);
        return 0;
    }

    case WM_NCDESTROY: {
        // 子窗口此时已销毁，可以安全释放缓存资源。
        ReleaseBackBuffer();
        ReleaseUIFonts();
        if (g_hIcon20) { DestroyIcon(g_hIcon20); g_hIcon20 = NULL; }
        if (g_hIcon28) { DestroyIcon(g_hIcon28); g_hIcon28 = NULL; }
        if (g_hEditFont) { DeleteObject(g_hEditFont); g_hEditFont = NULL; }
        if (g_hEditBgBrush) { DeleteObject(g_hEditBgBrush); g_hEditBgBrush = NULL; }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = L"SuperColorPickerD2D";
    RegisterClassExW(&wcex);

    HWND hwnd = CreateWindowW(L"SuperColorPickerD2D", L"🎨 取色器",
        // WS_CLIPCHILDREN：父窗口绘制时裁剪掉两个 EDIT 子控件的区域。
        // 否则拖动色板时整窗 BitBlt 会盖住输入框，造成高频闪烁。
        WS_POPUP | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W + 16, WIN_H + TITLE_H + 8, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}