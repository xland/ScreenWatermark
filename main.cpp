#include <Windows.h>
#include <cmath>
#include <numbers>
#include <thread>
#include <thorvg.h>

int x, y, w, h,wScale,hScale;
float dpi;
HWND hwnd;
std::vector<uint32_t> buffer;
tvg::SwCanvas* canvas;

void winPaint() {
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
            //scaleFactor = LOWORD(wParam) / 96.0f;
            //dpiChanged(reinterpret_cast<RECT*>(lParam));
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

void initDPI()
{
    dpi = GetDpiForWindow(hwnd) / 96.0f;
    wScale = w * dpi;
    hScale = h * dpi;
}

void initCanvas() {
    tvg::Scene* scene = tvg::Scene::gen();
    scene->scale(dpi);
    canvas = tvg::SwCanvas::gen();
    buffer.resize(wScale * hScale);
    canvas->target(buffer.data(), wScale, wScale, hScale, tvg::ColorSpace::ARGB8888);
    canvas->push(scene);

    float fontSize{ 16.f }, padding{ 16.f }, angle{20.f};
    float x, y, w, h;
    auto textShape = tvg::Text::gen();
    textShape->text("hello");
    textShape->size(fontSize);
    textShape->font("SimHei");    
    textShape->bounds(&x, &y, &w, &h);
    w += 2*padding; h += 2 * padding;
    textShape->fill(22, 88, 188);
    textShape->rotate(-angle);
    float thetaRad = angle * std::numbers::pi / 180.0;
    float hh = w * std::sin(thetaRad);
    float ww = w * std::cos(thetaRad);
    textShape->translate(padding, hh);
    scene->push(textShape);

    float tempW{ ww + padding }, tempH{hh+padding};
    while (tempH < hScale) {
        while (tempW < wScale)
        {
            auto textShape = tvg::Text::gen();
            textShape->text("hello");
            textShape->size(fontSize);
            textShape->font("SimHei");
            textShape->fill(22, 88, 188);
            textShape->rotate(-angle);
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
}

void showWindow() {
    winPaint();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void initWindow(HINSTANCE hInstance)
{
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
    wcex.lpszClassName = L"ScreenWatermark";
    wcex.hIconSm = LoadIcon(hInstance, (LPCTSTR)IDI_WINLOGO);
    RegisterClassExW(&wcex);
    hwnd = CreateWindowEx(WS_EX_TRANSPARENT|WS_EX_LAYERED, //|WS_EX_TOPMOST
        wcex.lpszClassName, wcex.lpszClassName, WS_POPUP,
        x, y, w, h, nullptr, nullptr, hInstance, nullptr);
}

void initPosSize()
{
    x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

void initTVG() {
    auto r = tvg::Initializer::init(std::thread::hardware_concurrency());
    r = tvg::Text::load("C:\\Windows\\Fonts\\SimHei.ttf");
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    initTVG();
    initPosSize();
    initWindow(hInstance);
    initDPI();
    initCanvas();
    showWindow();
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    tvg::Initializer::term();
	return 0;
}