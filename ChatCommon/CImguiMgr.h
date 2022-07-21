#pragma once
class CImguiMgr
{
public:
	~CImguiMgr();

public:
	void Init(HWND _hWnd, ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
	void Render();

protected:
	virtual void Update() = 0;
};

