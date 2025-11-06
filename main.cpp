#include <Windows.h>
#include <shlobj.h>
#include <cmath>
#include <numbers>
#include <thread>
#include <iostream>
#include <string>
#include <sstream>
#include <format>
#include <thorvg.h>

int w, h,wScale,hScale;
int size{ 16 }, padding{ 16 };
int angle{ -45 }, colorR{ 66 }, colorG{ 88 }, colorB{ 188 }, opacity{ 30 };
int border{ 1 }, borderR{ 255 }, borderG{ 122 }, borderB{ 0 };
float dpi;
std::wstring fontw;
std::string font, text;
HWND hwnd;

std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8,0,wstr.data(),
        static_cast<int>(wstr.size()),nullptr,0, nullptr, nullptr);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, '\0');
    WideCharToMultiByte(CP_UTF8,0,wstr.data(),static_cast<int>(wstr.size()),
        &result[0],sizeNeeded,nullptr,nullptr);
    return result;
}

inline void initCmd(LPTSTR cmdLine)
{
    std::wistringstream wiss(cmdLine);
    std::vector<std::wstring> tokens;
    {
        std::wstring token;
        while (wiss >> token) {
            tokens.push_back(token);
        }
    }
    if (tokens.size() < 13) {
        MessageBox(NULL, L"cmd params error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }

    auto& temp = tokens[0];
    if (temp.length() <= 2) {
        MessageBox(NULL, L"mark text error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }
    temp = temp.substr(1, temp.length() - 2);
    text = wstringToUtf8(temp);
    if (text.empty()) {
        MessageBox(NULL, L"mark text error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }

    temp = tokens[1];    
    if (temp.length() <= 2) {
        MessageBox(NULL, L"font name error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }
    temp = temp.substr(1, temp.length() - 2);
    fontw = temp;
    font = wstringToUtf8(temp);
    if (font.empty()) {
        MessageBox(NULL, L"font name error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }

    temp = tokens[2];
    wchar_t* end;
    size = std::wcstol(temp.data(), &end, 10);
    if (size <= 0) {
        MessageBox(NULL, L"font size error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }
    
    temp = tokens[3];
    padding = std::wcstol(temp.data(), &end, 10);
    if (padding <= 0) {
        MessageBox(NULL, L"text padding error", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
        return;
    }

    temp = tokens[4];
    angle = std::wcstol(temp.data(), &end, 10);

    temp = tokens[5];
    colorR = std::wcstol(temp.data(), &end, 10);

    temp = tokens[6];
    colorG = std::wcstol(temp.data(), &end, 10);

    temp = tokens[7];
    colorB = std::wcstol(temp.data(), &end, 10);
    
    temp = tokens[8];
    border = std::wcstol(temp.data(), &end, 10);

    temp = tokens[9];
    borderR = std::wcstol(temp.data(), &end, 10);

    temp = tokens[10];
    borderG = std::wcstol(temp.data(), &end, 10);

    temp = tokens[11];
    borderB = std::wcstol(temp.data(), &end, 10);

    temp = tokens[12];
    opacity = std::wcstol(temp.data(), &end, 10);
}

inline void paintWindow(const std::vector<uint32_t>& buffer) {
    HDC hdc = GetDC(hwnd);
    auto compDC = CreateCompatibleDC(hdc);
    auto bitmap = CreateCompatibleBitmap(hdc, wScale, hScale);
    DeleteObject(SelectObject(compDC, bitmap));
    BITMAPINFO info = { sizeof(BITMAPINFOHEADER), wScale, -hScale, 1, 32, BI_RGB, wScale * 4 * hScale, 0, 0, 0, 0 };
    SetDIBits(hdc, bitmap, 0, hScale, buffer.data(), &info, DIB_RGB_COLORS);
    BLENDFUNCTION blend = { .BlendOp{AC_SRC_OVER}, .SourceConstantAlpha{255}, .AlphaFormat{AC_SRC_ALPHA} };
    POINT pSrc = { 0, 0 };
    SIZE sizeWnd = { w, h };
    UpdateLayeredWindow(hwnd, hdc, NULL, &sizeWnd, compDC, &pSrc, NULL, &blend, ULW_ALPHA);
    ReleaseDC(hwnd, hdc);
    DeleteDC(compDC);
    DeleteObject(bitmap);
}

tvg::Text* createText()
{
    auto textShape = tvg::Text::gen();
    textShape->text(text.data());
    textShape->size(size);
    textShape->font(font.data());
    textShape->fill(colorR, colorG, colorB);
    if (border > 0) {
        textShape->outline(border, borderR, borderG, borderB);
    }
    textShape->opacity(opacity);
    return textShape;
}

inline void loadFont()
{
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPath(nullptr, CSIDL_FONTS, nullptr, 0, path))) {
        MessageBox(NULL, L"can not find font path", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
    }
    auto fontPath = std::format(L"{}\\{}.ttf", path, fontw);
    DWORD attrs = GetFileAttributes(fontPath.c_str());
    auto flag = (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    if (!flag) {
        MessageBox(NULL, L"can not find font file", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
    }
    auto fontStr = wstringToUtf8(fontPath);
    auto r = tvg::Text::load(fontStr.data());
    if (r != tvg::Result::Success) {
        MessageBox(NULL, L"can not load font file", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
    }
}

void paintCanvas() {
    auto r = tvg::Initializer::init(std::thread::hardware_concurrency());
    if (r != tvg::Result::Success) {
        MessageBox(NULL, L"can not init canvas", L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
    }
    loadFont();
    tvg::SwCanvas* canvas = tvg::SwCanvas::gen();
    tvg::Scene* scene = tvg::Scene::gen();
    scene->scale(dpi);
    std::vector<uint32_t> buffer;
    buffer.resize(wScale * hScale);
    canvas->target(buffer.data(), wScale, wScale, hScale, tvg::ColorSpace::ARGB8888);
    canvas->push(scene);

    double rSize = std::hypot(wScale, hScale);
    float xT, yT, wT, hT;
    int tempW{ 0 }, tempH{ 0 };
    bool flag{ true };
    while (tempH < rSize) {
        while (tempW < rSize) {
            auto textShape = createText();
            if (flag) {
                textShape->bounds(&xT, &yT, &wT, &hT);
                flag = false;
            }
            textShape->translate(tempW, tempH);
            scene->push(textShape);
            tempW = tempW + padding + wT;
        }
        tempW = 0;
        tempH = tempH + padding + hT;
    }
    scene->rotate(angle);    

    /*
        已知一个矩形A的宽和高：wScale,hScale，与一个正方形B的边长rSize。 
        一开始：矩形和正方形左上角的点重合，此点为坐标系原点：0,0 
        随后正方形B，绕着原点旋转一个角度angle, 
        请问：该如何移动B，才能让A和B的中心点重合
    */
    float angleRad = angle * std::numbers::pi / 180.0;
    float half = rSize / 2.0;
    float bx = half * std::cos(angleRad) - half * std::sin(angleRad);
    float by = half * std::sin(angleRad) + half * std::cos(angleRad);
    float tx = wScale / 2.0 - bx;
    float ty = hScale / 2.0 - by;
    scene->translate(tx, ty);

    canvas->update();
    canvas->draw();
    canvas->sync();

    delete canvas;
    tvg::Initializer::term();

    paintWindow(buffer);
}

LRESULT winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
    {
        return 1;
    }
    case WM_DPICHANGED:
    {
        dpi = LOWORD(wParam) / 96.0f;
        wScale = w * dpi;
        hScale = h * dpi;
        paintCanvas();
        return 0;
    }
    case WM_TIMER:
    {
        if (wParam == WM_APP+1) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void initWindow(HINSTANCE hInstance)
{
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = &winProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_WINLOGO);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"ScreenWM";
    wcex.hIconSm = LoadIcon(hInstance, (LPCTSTR)IDI_WINLOGO);
    RegisterClassExW(&wcex);
    hwnd = CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,
        wcex.lpszClassName, wcex.lpszClassName, WS_POPUP,
        x, y, w, h, nullptr, nullptr, hInstance, nullptr);
    SetTimer(hwnd, WM_APP+1, 1000, NULL);
    dpi = GetDpiForWindow(hwnd) / 96.0f;
    wScale = w * dpi;
    hScale = h * dpi;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    initCmd(lpCmdLine);
    initWindow(hInstance);
    paintCanvas();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
	return 0;
}