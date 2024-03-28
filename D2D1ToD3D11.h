// ----------------------------------------------------------------------------
// Hacking Direct2D over Direct3D 11 sample code
// Author: @lx/Alexandre Mutel,  blog: http://code4k.blogspot.com
// The software is provided "as is", without warranty of any kind.
// ----------------------------------------------------------------------------
#pragma once

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>

// ----------------------------------------------------------------------------
// Code4kCreateD3D10CompatibleSurface function
// Returns a surface from a D3D11 Texture2D compatible with Direct2D
// ----------------------------------------------------------------------------
IDXGISurface* Code4kCreateD3D10CompatibleSurface(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture);

// ----------------------------------------------------------------------------
// Code4kSaveTextureToFile function
// Helper function to save a texture supporting BGRA format
// ----------------------------------------------------------------------------
HRESULT Code4kSaveTextureToFile(ID3D11DeviceContext *pContext, ID3D11Texture2D* pSrcTexture, D3DX11_IMAGE_FILE_FORMAT DestFormat, LPCTSTR pDestFile);
