#pragma once

class CDirectxMgr
{
	ComPtr<ID3D11Device>			m_pDevice;
	ComPtr<ID3D11DeviceContext>		m_pContext;
	ComPtr<IDXGISwapChain>			m_pSwapChain;

	ComPtr<ID3D11RenderTargetView>	m_pRTV;
	ComPtr<ID3D11DepthStencilView>	m_pDSV;

public:
	ID3D11Device* GetDevice() const { return m_pDevice.Get(); }
	ID3D11DeviceContext* GetContext() const { return m_pContext.Get(); }

public:
	~CDirectxMgr();

public:
	void Init(HWND _hWnd, UINT _iWidth, UINT _iHeight);
	void Present();
};

