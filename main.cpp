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
float fontSize{ 16.f }, padding{ 16.f };
std::string font{ "SimHei" };
std::string text{ "Hello" };
int angle{ -20 };
int colorR{ 66 }, colorG{ 88 }, colorB{ 188 },opacity{ 30 };
float dpi;
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
    auto& temp = tokens[0];
    temp = temp.substr(1, temp.length() - 2);
    text = wstringToUtf8(temp);

    temp = tokens[1];
    temp = temp.substr(1, temp.length() - 2);
    font = wstringToUtf8(temp);

    temp = tokens[2];
    wchar_t* end;
    angle = std::wcstol(temp.data(), &end, 10);

    temp = tokens[3];
    colorR = std::wcstol(temp.data(), &end, 10);
    temp = tokens[4];
    colorG = std::wcstol(temp.data(), &end, 10);
    temp = tokens[5];
    colorB = std::wcstol(temp.data(), &end, 10);
    temp = tokens[6];
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
    textShape->size(fontSize);
    textShape->font(font.data());
    textShape->fill(colorR, colorG, colorB);
    textShape->rotate(angle);
    textShape->opacity(opacity);
    return textShape;
}

inline void loadFont()
{
    std::string fontPath;
    {
        wchar_t path[MAX_PATH];
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, 0, path))) {
            return;
        }
        fontPath = wstringToUtf8(path);
        fontPath += "\\" + font + ".ttf";
    }
    auto r = tvg::Text::load(fontPath.data());
}

void paintCanvas() {
    auto r = tvg::Initializer::init(std::thread::hardware_concurrency());
    loadFont();
    tvg::SwCanvas* canvas = tvg::SwCanvas::gen();
    tvg::Scene* scene = tvg::Scene::gen();
    scene->scale(dpi);
    std::vector<uint32_t> buffer;
    buffer.resize(wScale * hScale);
    canvas->target(buffer.data(), wScale, wScale, hScale, tvg::ColorSpace::ARGB8888);
    canvas->push(scene);

    float x, y, w, h;
    auto textShape = createText();
    textShape->bounds(&x, &y, &w, &h);
    w += 2*padding; h += 2 * padding;
    float thetaRad = std::abs(angle) * std::numbers::pi / 180.0;
    float hh = w * std::sin(thetaRad);
    float ww = w * std::cos(thetaRad);
    textShape->translate(padding, hh);
    scene->push(textShape);

    float tempW{ ww + padding }, tempH{hh+padding};
    while (tempH < hScale) {
        while (tempW < wScale)
        {
            auto textShape = createText();
            textShape->translate(tempW, tempH);
            scene->push(textShape);
            tempW += ww;
        }
        tempW = padding;
        tempH += hh+padding;
    }

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
    hwnd = CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_LAYERED, //|WS_EX_TOPMOST
        wcex.lpszClassName, wcex.lpszClassName, WS_POPUP,
        x, y, w, h, nullptr, nullptr, hInstance, nullptr);
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