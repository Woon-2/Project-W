#ifndef __FONT_HPP
#define __FONT_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"

struct FontHandle {
	ComPtr<IDWriteTextFormat> pTextFormat;
	float fFontSize;
	WCHAR	wchFontFamilyName[512];
};

struct TextImage {
	TextImage() = default;
	TextImage( ID3D12Device* device, UINT textWidth, UINT textHeight, DescriptorPool& srvTexPool );

	std::vector<BYTE> pData{};
	Texture texture{};
	Texture textureUpload{};
	UINT width = 0;
	UINT height = 0;
};

class Font {
public:
	Font() = default;
	Font( const Font& ) = delete;
	Font& operator=( const Font& ) = delete;
	Font( Font&& ) noexcept = delete;
	Font& operator=( Font&& ) noexcept = delete;
	~Font() = default;

	void init( ID3D12Device* device, ID3D12CommandQueue* cmdQ, UINT width, UINT height, HWND hwnd );
	FontHandle CreateFontObject( const WCHAR* fontFamilyName, float fontSize );
	void WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, FontHandle* pFontHandle, const WCHAR* wchString, DWORD dwLen );

private:
	void createD2D( ID3D12Device* device, ID3D12CommandQueue* cmdQ );
	void createDWrite( ID3D12Device* device, UINT texWidth, UINT texHeight, float dpi );
	void CreateBitmapFromText( int* piOutWidth, int* piOutHeight, IDWriteTextFormat* pTextFormat, const WCHAR* wchString, DWORD dwLen );

	ComPtr<ID2D1Device2> pD2DDevice_ = nullptr;
	ComPtr<ID2D1DeviceContext2> pD2DDeviceContext_ = nullptr;

	ComPtr<ID2D1Bitmap1> pD2DTargetBitmap_ = nullptr;
	ComPtr<ID2D1Bitmap1> pD2DTargetBitmapRead_ = nullptr;
	ComPtr<ID2D1SolidColorBrush> pWhiteBrush_ = nullptr;

	ComPtr<IDWriteFactory5> pDWFactory_ = nullptr;
	UINT	D2DBitmapWidth_ = 0;
	UINT	D2DBitmapHeight_ = 0;
};

#endif // __FONT_HPP