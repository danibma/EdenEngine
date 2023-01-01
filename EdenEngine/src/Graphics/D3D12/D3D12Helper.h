#pragma once

#define SAFE_RELEASE(x) if (x) x->Release();

#define DX_CHECK(call) \
	{ \
		HRESULT hr = (call); \
		ensureMsgf(hr == S_OK, "D3D12 call \"%s\" failed", #call); \
	}
