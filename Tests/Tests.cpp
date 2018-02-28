// Tests.cpp: ���������� ����� ����� ��� ����������.
//

#include "..\\VirtualUI\\ShapeBase.h"
#include "..\\VirtualUI\\PlatformDependent\\Direct2D.h"

#include "stdafx.h"
#include "Tests.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

using namespace Engine;
using namespace Reflection;

#define MAX_LOADSTRING 100

// ���������� ����������:
HINSTANCE hInst;                                // ������� ���������
WCHAR szTitle[MAX_LOADSTRING];                  // ����� ������ ���������
WCHAR szWindowClass[MAX_LOADSTRING];            // ��� ������ �������� ����

// ��������� ���������� �������, ���������� � ���� ������ ����:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

ID2D1DCRenderTarget * Target;
Engine::Direct2D::D2DRenderDevice * Device;
Engine::UI::FrameShape * Shape;

class test : public Reflection::ReflectableObject
{
public:
	DECLARE_PROPERTY(int, value);
	DECLARE_PROPERTY(Coordinate, coord);
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	/*auto spl = (L"abcabcabcab" + string(-1234)).Replace(L"bc", L"XYU").LowerCase().Split(L'-');
	SortArray(spl, true);
	for (int i = 0; i < spl.Length(); i++) MessageBox(0, spl[i], L"", 0);

	ObjectArray<string> safe;
	safe << new string(L"blablabla");
	safe.Append(new string(L"4epHblu Hurrep"));
	safe.Append(new string(L"kornevgen pidor"));
	safe.Append(new string(L"hui"));
	safe.Append(new string((void*) &safe));

	test t;
	auto p = t.GetProperty(L"value");
	int v = 666;
	p->Set(&v);
	MessageBox(0, string(t.value), 0, 0);

	SafePointer<string> r(new string(L"pidor"));
	MessageBox(0, *r, L"", 0);
	SafePointer<string> r2(new string(L"pidor"));
	MessageBox(0, string(r == r2), L"", 0);

	for (int i = 0; i < safe.Length(); i++) safe[i].Release();
	SortArray(safe);
	MessageBox(0, safe.ToString(), L"", 0);
	safe.Clear();
	MessageBox(0, safe.ToString(), L"", 0);*/

	auto v = string(L";123456").ToDouble(L";");


	Engine::Direct2D::InitializeFactory();
	D2D1_RENDER_TARGET_PROPERTIES rtp;
	rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	rtp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	rtp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	rtp.dpiX = 0.0f;
	rtp.dpiY = 0.0f;
	rtp.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
	rtp.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
	if (Engine::Direct2D::Factory->CreateDCRenderTarget(&rtp, &Target) != S_OK) MessageBox(0, L"XYU.", 0, MB_OK | MB_ICONSTOP);
	Device = new Engine::Direct2D::D2DRenderDevice(Target);

	Array<UI::GradientPoint> ps;
	ps << UI::GradientPoint(UI::Color(0, 255, 0), 0.0);
	ps << UI::GradientPoint(UI::Color(0, 0, 255), 1.0);
	::Shape = new FrameShape(UI::Rectangle(100, 100, 600, 600), FrameShape::FrameRenderMode::Layering, 0.5);
	auto s2 = new BarShape(UI::Rectangle(100, 100, 300, 300), ps, 3.141592 / 3.0);
	::Shape->Children.Append(s2);
	s2->Release();
	
	s2 = new BarShape(UI::Rectangle(0, -50, 200, 200), UI::Color(255, 0, 255));
	::Shape->Children.Append(s2);
	s2->Release();

    // TODO: ���������� ��� �����.

    // ������������� ���������� �����
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TESTS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // ��������� ������������� ����������:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TESTS));

    MSG msg;

    // ���� ��������� ���������:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  �������: MyRegisterClass()
//
//  ����������: ������������ ����� ����.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   �������: InitInstance(HINSTANCE, int)
//
//   ����������: ��������� ��������� ���������� � ������� ������� ����.
//
//   �����������:
//
//        � ������ ������� ���������� ���������� ����������� � ���������� ����������, � �����
//        ��������� � ��������� �� ����� ������� ���� ���������.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // ��������� ���������� ���������� � ���������� ����������

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  �������: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ����������:  ������������ ��������� � ������� ����.
//
//  WM_COMMAND � ���������� ���� ����������
//  WM_PAINT � ���������� ������� ����
//  WM_DESTROY � ��������� ��������� � ������ � ���������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // ��������� ����� � ����:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
			RECT Rect;
			GetClientRect(hWnd, &Rect);
            HDC hdc = GetDC(hWnd);
			FillRect(hdc, &Rect, (HBRUSH) GetStockObject(LTGRAY_BRUSH));

			ValidateRect(hWnd, 0);

			Target->BindDC(hdc, &Rect);
			Target->BeginDraw();
			::Shape->Render(Device, Box(Rect.left, Rect.top, Rect.right, Rect.bottom));
			Target->EndDraw();

            ReleaseDC(hWnd, hdc);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ���������� ��������� ��� ���� "� ���������".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
