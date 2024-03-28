// ----------------------------------------------------------------------------
// Hacking Direct2D over Direct3D 11 sample code
// Author: @lx/Alexandre Mutel,  blog: http://code4k.blogspot.com
// The software is provided "as is", without warranty of any kind.
// ----------------------------------------------------------------------------

#include "D2D1ToD3D11.h"
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3D11SDKLayers.h>
#include <d3d10_1.h>
#include <D2D1.h>
#include <DWrite.h>

// ----------------------------------------------------------------------------
// Debug macros
// ----------------------------------------------------------------------------
#ifdef _DEBUG
#include <dxerr.h>
#pragma comment(lib, "dxerr.lib")
#define CHECK_HRESULT(result) { if (FAILED(result)) { DXTraceW(__FILE__,__LINE__, result, L"Error", true); __asm _emit 0xF1 }  }
#else
#define CHECK_HRESULT(result)
#endif

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* texture2D;
    ID2D1Factory* d2dFactory;
    IDXGISurface* surface;
    ID2D1RenderTarget *d2dRenderTarget;
    IDWriteFactory *dWriteFactory;
    IDWriteTextFormat *dWriteTextFormat;
    IDWriteTextLayout* pTextLayout;
    ID2D1PathGeometry* pathGeometry;
    D3D_FEATURE_LEVEL feature;
    HRESULT result;

    // Create D3D11 device with FEATURE_LEVEL 10
    D3D_FEATURE_LEVEL levels[1] = { D3D_FEATURE_LEVEL_10_0 };
    result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_DEBUG, levels, 1, D3D11_SDK_VERSION, &device, &feature, &context);
    CHECK_HRESULT(result);

    // Create Direct2D
    //D2D1_FACTORY_OPTIONS options;
    //options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
    result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory), (const D2D1_FACTORY_OPTIONS*)0, (void**)&d2dFactory); 
    CHECK_HRESULT(result);

    // Create DirectWrite Factory
    result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dWriteFactory);
    CHECK_HRESULT(result);

    // Create offline renderable texture2D to use by Direct2D
    static D3D11_TEXTURE2D_DESC offlineTextureDesc =  
    {
        800,					// width
        600,					// height
        1,						// MipLevels;
        1,						// ArraySize;
        DXGI_FORMAT_R8G8B8A8_UNORM,		// Format;
        {1, 0},					// SampleDesc
        D3D11_USAGE_DEFAULT,	// Usage
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,	// BindFlags;
        (D3D11_CPU_ACCESS_FLAG)0,	// CPUAccessFlags;
        (D3D11_RESOURCE_MISC_FLAG)0 // MiscFlags;
    };

    result = device->CreateTexture2D(&offlineTextureDesc, 0, &texture2D);
    CHECK_HRESULT(result);

    // Create a Proxy DXGISurface from Texture2D compatible with Direct2D
    surface = Code4kCreateD3D10CompatibleSurface(device, context, texture2D);

    // Create a D2D render target which can draw into our offscreen D3D
    // surface. Given that we use a constant size for the texture, we
    // fix the DPI at 96.
    static D2D1_RENDER_TARGET_PROPERTIES props = {
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96,
            96
    };
    result = d2dFactory->CreateDxgiSurfaceRenderTarget(
        surface,
        &props,
        &d2dRenderTarget
        );
    CHECK_HRESULT(result);

    // Create Gradient
    D2D1_GRADIENT_STOP gradientStops[2];
    gradientStops[0].color = D2D1::ColorF(1.0f,1.0f,1.0f,1.0f);
    gradientStops[0].position = 0.0f;
    gradientStops[1].color = D2D1::ColorF(0.5f,0.1f,0.0f,1.0f);
    gradientStops[1].position = 1.0f;

    ID2D1GradientStopCollection* pGradientStops;
    // Create the ID2D1GradientStopCollection from a previously
    // declared array of D2D1_GRADIENT_STOP structs.
    result = d2dRenderTarget->CreateGradientStopCollection(
        gradientStops,
        2,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &pGradientStops
        );

    // Create Gradient Brush
    ID2D1LinearGradientBrush *m_pLinearGradientBrush;

    result = d2dRenderTarget->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(300, 400),
            D2D1::Point2F(500, 600)),
        pGradientStops,
        &m_pLinearGradientBrush
        );
    CHECK_HRESULT(result);

    // Create Solid Color Brush
    ID2D1SolidColorBrush *d2dSolidColorBrush;
    result = d2dRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0xFFFFFF,1),
            &d2dSolidColorBrush
            );
    CHECK_HRESULT(result);

    // Create TextFormat
    result = dWriteFactory->CreateTextFormat(L"Calibri", 0, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 128, L"", &dWriteTextFormat);
    CHECK_HRESULT(result);

    dWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    dWriteTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Create TextLayout
    result = dWriteFactory->CreateTextLayout(
            L"D2D1  D3D11",
            ARRAYSIZE(L"D2D1  D3D11") - 1,
            dWriteTextFormat,
            800,         // The width of the layout box.
            600,        // The height of the layout box.
            &pTextLayout  // The IDWriteTextLayout interface pointer.
            );
    CHECK_HRESULT(result);

    // Create Rounded Stroke Style
    ID2D1StrokeStyle* strokeStyle;
    result = d2dFactory->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_FLAT,
            D2D1_LINE_JOIN_ROUND,
            10.0f,
            D2D1_DASH_STYLE_SOLID,
            0.0f),
        NULL,
        0,
        &strokeStyle
        );
    CHECK_HRESULT(result);



    // Create PathGeometry
    result = d2dFactory->CreatePathGeometry(&pathGeometry);
    CHECK_HRESULT(result);

    ID2D1GeometrySink* pSink;
    result = pathGeometry->Open(&pSink);
    CHECK_HRESULT(result);
    pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

    pSink->BeginFigure(
        D2D1::Point2F(346,255),
        D2D1_FIGURE_BEGIN_FILLED
        );
    D2D1_POINT_2F points[5] = {
        D2D1::Point2F(267, 177),
        D2D1::Point2F(236, 192),
        D2D1::Point2F(212, 160),
        D2D1::Point2F(156, 255),
        D2D1::Point2F(346, 255), 
        };
    pSink->AddLines(points, ARRAYSIZE(points));
    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pSink->Close();

    // Begin Direct2D rendering
    d2dRenderTarget->BeginDraw();

    D2D_COLOR_F color = { 0.0f, 0.0f, 0.0f, 1.0f};
    d2dRenderTarget->Clear(color);

    D2D_POINT_2F fromPt = { 0,0 };
    D2D_POINT_2F bottomPt = { 0,600 };
    D2D_POINT_2F toPt = { 400,310 };

    // Draw lines
    d2dRenderTarget->DrawLine(fromPt, toPt, d2dSolidColorBrush, 14.0f, strokeStyle);
    d2dRenderTarget->DrawLine(bottomPt, toPt, d2dSolidColorBrush, 14.0f, strokeStyle);

    // Draw Mountain Geometry
    d2dRenderTarget->SetTransform(D2D1::Matrix3x2F::Translation(150, -70));
    d2dRenderTarget->DrawGeometry(pathGeometry, d2dSolidColorBrush, 10);
    d2dRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // Draw a filled Ellipse
    static D2D1_ELLIPSE ellipse = { { 400, 460}, 100, 100 };
    d2dRenderTarget->FillEllipse(ellipse, m_pLinearGradientBrush);
    d2dRenderTarget->DrawEllipse(ellipse, d2dSolidColorBrush, 4);

    // Draw some text
    d2dRenderTarget->DrawTextLayout(fromPt, pTextLayout, d2dSolidColorBrush);

    // End Direct2D rendering
    result = d2dRenderTarget->EndDraw();
    CHECK_HRESULT(result);

    // Save Direct2D rendering to a file
    result = Code4kSaveTextureToFile(context, texture2D, D3DX11_IMAGE_FILE_FORMAT::D3DX11_IFF_PNG, "test.png");
    CHECK_HRESULT(result);

    return 0;
}

