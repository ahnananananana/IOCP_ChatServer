#include "pch2.h"
#include "CDirectxMgr.h"

CDirectxMgr::~CDirectxMgr()
{
}

void CDirectxMgr::Init(HWND _hWnd, UINT _iWidth, UINT _iHeight)
{
	UINT iFlag = 0;
#ifdef _DEBUG
	iFlag = D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL eLevel = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE
		, nullptr, iFlag, 0, 0
		, D3D11_SDK_VERSION
		, m_pDevice.GetAddressOf(), &eLevel, m_pContext.GetAddressOf());

	//GFX_MSG_ONLY(hr);
	if (FAILED(hr))
	{
		iFlag &= (~D3D11_CREATE_DEVICE_DEBUG);
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE
			, nullptr, iFlag, 0, 0
			, D3D11_SDK_VERSION
			, m_pDevice.GetAddressOf(), &eLevel, m_pContext.GetAddressOf())))
		{
			throw;
		}
	}

	UINT iMultiSampleLevel = 0;
	m_pDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &iMultiSampleLevel);

	if (0 == iMultiSampleLevel)
	{
		throw;
	}

	{
		DXGI_SWAP_CHAIN_DESC desc = {};

		// 버퍼 개수(front, back 포함 2 아닌가?)
		desc.BufferCount = 1;

		// 만들어질 백버퍼, 프론트 버퍼의 해상도 설정
		desc.BufferDesc.Width = _iWidth;
		desc.BufferDesc.Height = _iHeight;

		// 프론트버퍼 출력 비율
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;

		// 버퍼의 픽셀 포맷
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		// 스캔라이닝
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER::DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING::DXGI_MODE_SCALING_UNSPECIFIED;

		// 버퍼의 용도
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

		// 렌더링 출력 대상 윈도우
		desc.OutputWindow = _hWnd;

		// 샘플 수준
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		// 창모드 인지 전체화면 모드인지
		desc.Windowed = true;

		// SWAP 할 때 방식
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		// SwapChain 을 생성하기 위한 객체 얻기
		IDXGIDevice* pIDXGIDevice = nullptr;
		IDXGIAdapter* pAdapter = nullptr;
		IDXGIFactory* pFactory = nullptr;

		m_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pIDXGIDevice);
		pIDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pAdapter);
		pAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory);

		// 스왑체인 생성
		if (FAILED(pFactory->CreateSwapChain(m_pDevice.Get(), &desc, m_pSwapChain.GetAddressOf())))
		{
			throw;
		}

		pIDXGIDevice->Release();
		pAdapter->Release();
		pFactory->Release();
	}

	// ViewPort 설정
	{
		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		vp.Width = _iWidth;
		vp.Height = _iHeight;
		vp.MinDepth = 0;
		vp.MaxDepth = 1;

		m_pContext->RSSetViewports(1, &vp);
	}

	//Create View
	{
		// RenderTarget View
		// SwapChain 이 가지고 있는 버퍼를 이용해서 만든다.
		{
			ComPtr<ID3D11Texture2D> pBackBuffer = nullptr;
			m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

			D3D11_TEXTURE2D_DESC desc = {};

			pBackBuffer->GetDesc(&desc);
			// Texture 를 이용해서 RTV 생성
			m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, m_pRTV.ReleaseAndGetAddressOf());
		}

		// DepthStencil 용 Texture 생성
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = _iWidth;
			desc.Height = _iHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.CPUAccessFlags = 0;
			desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_DEPTH_STENCIL;

			ComPtr<ID3D11Texture2D> pTex;
			m_pDevice->CreateTexture2D(&desc, nullptr, pTex.GetAddressOf());
			m_pDevice->CreateDepthStencilView(pTex.Get(), nullptr, m_pDSV.ReleaseAndGetAddressOf());
		}

		m_pContext->OMSetRenderTargets(1, m_pRTV.GetAddressOf(), m_pDSV.Get());
	}

	{
		RECT rt = { 0, 0, (long)_iWidth, (long)_iHeight };
		AdjustWindowRect(&rt, WS_OVERLAPPEDWINDOW, false);
		SetWindowPos(_hWnd, nullptr, 0, 0, rt.right - rt.left, rt.bottom - rt.top, 0);
	}
}

void CDirectxMgr::Present()
{
	m_pSwapChain->Present(0, 0);

	float clear[4] = {0,0,0,0};
	m_pContext->ClearRenderTargetView(m_pRTV.Get(), clear);
	m_pContext->ClearDepthStencilView(m_pDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	m_pContext->OMSetRenderTargets(1, m_pRTV.GetAddressOf(), m_pDSV.Get());
}