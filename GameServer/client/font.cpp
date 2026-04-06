#include "pch.hpp"
#include "font.hpp"
#include "errorHandling.hpp"

using namespace D2D1;

void Font::init( ID3D12Device* device, ID3D12CommandQueue* cmdQ, UINT width, UINT height, HWND hwnd )
{
	createD2D( device, cmdQ );
	float dpi = GetDpiForWindow( hwnd );
	createDWrite( device, width, height, dpi );
}

void Font::createD2D( ID3D12Device* device, ID3D12CommandQueue* cmdQ )
{
	// Create D3D11 on D3D12
	// 12와 호환되는 11 디바이스를 만든다.

	UINT flag = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	D2D1_FACTORY_OPTIONS factoryOptions = {};

	ComPtr<ID2D1Factory3> pD2DFactory = nullptr;
	ComPtr<ID3D11Device> pD3D11Device = nullptr;
	ComPtr<ID3D11DeviceContext> pD3D11DeviceContext = nullptr;
	ComPtr<ID3D11On12Device> pD3D11On12Device = nullptr;

#ifdef DXGI_DEBUG_INFO
	flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_NONE;

	DISPLAY_ERROR_DX_HR(
		D3D11On12CreateDevice(
			device,
			flag,
			nullptr,
			0,
			reinterpret_cast<IUnknown**>( &cmdQ ),
			1,
			0,
			&pD3D11Device,
			&pD3D11DeviceContext,
			nullptr
		),
		true
	);

	setDXName( pD3D11Device.Get(), "D3D11Device" );
	setDXName( pD3D11DeviceContext.Get(), "D3D11DeviceContext" );

	DISPLAY_ERROR_DX_HR( pD3D11Device->QueryInterface( IID_PPV_ARGS( &pD3D11On12Device ) ), true );

	// Create D2D/DWrite components.
	D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;

	DISPLAY_ERROR_DX_HR( 
		D2D1CreateFactory( D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &factoryOptions, (void**)&pD2DFactory ),
		true );

	IDXGIDevice* pDXGIDevice = nullptr;
	DISPLAY_ERROR_DX_HR( pD3D11On12Device->QueryInterface( IID_PPV_ARGS( &pDXGIDevice ) ), true );

	DISPLAY_ERROR_DX_HR( pD2DFactory->CreateDevice( pDXGIDevice, &pD2DDevice_ ), true );

	DISPLAY_ERROR_DX_HR( pD2DDevice_->CreateDeviceContext( deviceOptions, &pD2DDeviceContext_ ), true );
}

void Font::createDWrite( ID3D12Device* device, UINT TexWidth, UINT TexHeight, float dpi )
{
	D2D1_SIZE_U	size;
	size.width = TexWidth;
	size.height = TexHeight;

	D2DBitmapWidth_ = TexWidth;
	D2DBitmapHeight_ = TexHeight;

	D2D1_BITMAP_PROPERTIES1 bitmapProperties =
		BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat( DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ),
			dpi,
			dpi
		);

	DISPLAY_ERROR_DX_HR( pD2DDeviceContext_->CreateBitmap( size, nullptr, 0, &bitmapProperties, &pD2DTargetBitmap_ ), true );

	bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ;

	DISPLAY_ERROR_DX_HR( pD2DDeviceContext_->CreateBitmap( size, nullptr, 0, &bitmapProperties, &pD2DTargetBitmapRead_ ), true );

	DISPLAY_ERROR_DX_HR( pD2DDeviceContext_->CreateSolidColorBrush( ColorF( ColorF::White ), &pWhiteBrush_ ), true );
	DISPLAY_ERROR_DX_HR( pD2DDeviceContext_->CreateSolidColorBrush( ColorF( ColorF::Black ), &pBlackBrush_ ), true );

	DISPLAY_ERROR_DX_HR( DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), (IUnknown**)&pDWFactory_ ), true );
}

void Font::CreateBitmapFromText( int* piOutWidth, int* piOutHeight, IDWriteTextFormat* pTextFormat, const WCHAR* wchString, DWORD dwLen )
{
	BOOL	bResult = FALSE;

	ID2D1DeviceContext* pD2DDeviceContext = pD2DDeviceContext_.Get();
	IDWriteFactory5* pDWFactory = pDWFactory_.Get();
	D2D1_SIZE_F max_size = pD2DDeviceContext->GetSize();
	max_size.width = (float)D2DBitmapWidth_;
	max_size.height = (float)D2DBitmapHeight_;

	IDWriteTextLayout* pTextLayout = nullptr;
	if ( pDWFactory && pTextFormat )
	{
		DISPLAY_ERROR_DX_HR( pDWFactory->CreateTextLayout( wchString, dwLen, pTextFormat, max_size.width, max_size.height, &pTextLayout ), true );
	}
	DWRITE_TEXT_METRICS metrics = {};

	if ( pTextLayout )
	{
		pTextLayout->GetMetrics( &metrics );

		// 타겟설정
		pD2DDeviceContext->SetTarget( pD2DTargetBitmap_.Get() );

		// 안티앨리어싱모드 설정
		pD2DDeviceContext->SetTextAntialiasMode( D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE );

		// 텍스트 렌더링
		pD2DDeviceContext->BeginDraw();

		pD2DDeviceContext->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
		pD2DDeviceContext->SetTransform( D2D1::Matrix3x2F::Identity() );

		DISPLAY_ERROR_DX_VOID( pD2DDeviceContext->DrawTextLayout( D2D1::Point2F( 0.0f, 0.0f ), pTextLayout, pWhiteBrush_.Get() ), true);

		// We ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
		// is lost. It will be handled during the next call to Present.
		DISPLAY_ERROR_DX_HR( pD2DDeviceContext->EndDraw(), true);

		// 안티앨리어싱 모드 복구    
		pD2DDeviceContext->SetTextAntialiasMode( D2D1_TEXT_ANTIALIAS_MODE_DEFAULT );
		pD2DDeviceContext->SetTarget( nullptr );

		// 레이아웃 오브젝트 필요없으니 Release
		pTextLayout->Release();
		pTextLayout = nullptr;
	}
	int width = (int)ceil( metrics.width );
	int height = (int)ceil( metrics.height );

	D2D1_POINT_2U	destPos = {};
	D2D1_RECT_U		srcRect = { 0, 0, width, height };

	DISPLAY_ERROR_DX_HR( pD2DTargetBitmapRead_->CopyFromBitmap( &destPos, pD2DTargetBitmap_.Get(), &srcRect ), true );

	*piOutWidth = width;
	*piOutHeight = height;
}

FontHandle Font::CreateFontObject( const WCHAR* fontFamilyName, float fontSize )
{
	FontHandle ret{};

	IDWriteTextFormat* pTextFormat = nullptr;
	IDWriteFactory5* pDWFactory = pDWFactory_.Get();
	IDWriteFontCollection1* pFontCollection = nullptr;

	// The logical size of the font in DIP("device-independent pixel") units.A DIP equals 1 / 96 inch.

	if ( pDWFactory )
	{
		DISPLAY_ERROR_DX_HR( pDWFactory->CreateTextFormat(
			fontFamilyName,
			pFontCollection,                       // Font collection (nullptr sets it to use the system font collection).
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			fontSize,
			L"ko-kr",
			&pTextFormat
		), true );
	}

	wcscpy_s( ret.wchFontFamilyName, fontFamilyName );
	ret.fFontSize = fontSize;

	if ( pTextFormat )
	{
		DISPLAY_ERROR_DX_HR( pTextFormat->SetTextAlignment( DWRITE_TEXT_ALIGNMENT_LEADING ), true );
		DISPLAY_ERROR_DX_HR( pTextFormat->SetParagraphAlignment( DWRITE_PARAGRAPH_ALIGNMENT_NEAR ), true );
	}

	ret.pTextFormat = pTextFormat;

	return ret;
}

void Font::WriteTextToBitmap( TextImage* pDestImage, UINT DestWidth, UINT DestHeight, UINT DestPitch, int* piOutWidth, int* piOutHeight, FontHandle* pFontHandle, const WCHAR* wchString, DWORD dwLen )
{
	int iTextWidth = 0;
	int iTextHeight = 0;

	CreateBitmapFromText( &iTextWidth, &iTextHeight, pFontHandle->pTextFormat.Get(), wchString, dwLen);

	if ( iTextWidth > (int)DestWidth )
		iTextWidth = (int)DestWidth;

	if ( iTextHeight > (int)DestHeight )
		iTextHeight = (int)DestHeight;

	D2D1_MAPPED_RECT	mappedRect;
	DISPLAY_ERROR_HR( pD2DTargetBitmapRead_->Map( D2D1_MAP_OPTIONS_READ, &mappedRect ), true );

	BYTE* pDest = pDestImage->pData.data();
	char* pSrc = (char*)mappedRect.bits;

	for ( DWORD y = 0; y < (DWORD)iTextHeight; y++ )
	{
		memcpy( pDest, pSrc, iTextWidth * 4 );
		pDest += DestPitch;
		pSrc += mappedRect.pitch;
	}
	pD2DTargetBitmapRead_->Unmap();
	
	*piOutWidth = iTextWidth;
	*piOutHeight = iTextHeight;
}

TextImage::TextImage( ID3D12Device* device, UINT textWidth, UINT textHeight, DescriptorPool& srvTexPool )
{
	DXGI_FORMAT TexFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	width = textWidth;
	height = textHeight;

	const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;

	pData.resize( bytes );

	createResourcePair( &texture.res, &textureUpload.res, device, width, height, TexFormat );

	texture.uploadInfo = std::make_shared<TextureUploadInfo>();
	texture.uploadInfo->resDesc = texture.res->GetDesc();

	if ( texture.uploadInfo->resDesc.MipLevels > static_cast<UINT16>( texture.uploadInfo->footprints.size() ) )
		__debugbreak();

	device->GetCopyableFootprints( &texture.uploadInfo->resDesc, 0, texture.uploadInfo->resDesc.MipLevels,
		0, texture.uploadInfo->footprints.data(), texture.uploadInfo->rowCounts.data(),
		texture.uploadInfo->rowSizes.data(), &texture.uploadInfo->totalSize
	);

	// default heap에 대한 리소스만 srv 생성
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = TexFormat;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	createSRV( device, texture, SRVDesc, srvTexPool );
	texture.idxSrv.idxSampler = etoi(Samplers::BilinearClamp);
	texture.idxSrv.idxRange = etoi( Texture::Type::Tex2D );
}
