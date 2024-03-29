// ----------------------------------------------------------------------------
// Hacking Direct2D over Direct3D 11 sample code
// Author: @lx/Alexandre Mutel,  blog: http://code4k.blogspot.com
// The software is provided "as is", without warranty of any kind.
// ----------------------------------------------------------------------------

#pragma GCC diagnostic ignored "-Wnonportable-include-path"
#pragma GCC diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"

#include "D2D1ToD3D11.h"
#include <d3d10_1.h>

#pragma GCC diagnostic warning "-Wnonportable-include-path"

// ----------------------------------------------------------------------------
// Debug macros
// ----------------------------------------------------------------------------
#define CHECK_HRESULT(result) { if (FAILED(result)) { __debugbreak();  } }

#define CHECK_RETURN(result) CHECK_HRESULT(result); return result;

// ----------------------------------------------------------------------------
// IUnknown proxy AddRef/Release
// ----------------------------------------------------------------------------
#define ProxyIUnknownNoQueryInterface \
	IUnknown* backend; \
	virtual ULONG STDMETHODCALLTYPE AddRef( void) { \
	return backend->AddRef(); \
		} \
		virtual ULONG STDMETHODCALLTYPE Release( void) { \
		ULONG count = backend->Release(); \
		if ( count == 0) { \
		delete this; \
		} \
		return count; \
		}

// ----------------------------------------------------------------------------
// IUnknown proxy QueryIntreface
// ----------------------------------------------------------------------------
#define ProxyIUnknown \
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(  \
	/* [in] */ REFIID riid, \
	/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) { \
	HRESULT result = backend->QueryInterface(riid, ppvObject); \
	return result; \
		} \
	ProxyIUnknownNoQueryInterface

class ProxyID3D10Device;

// ----------------------------------------------------------------------------
// ID3D10DeviceChild proxy
// ----------------------------------------------------------------------------
#define ProxyID3D10DeviceChild \
	ID3D10Device* device; \
	ProxyID3D10Device* deviceProxy() { \
	return (ProxyID3D10Device*)device; \
} \
	virtual void STDMETHODCALLTYPE GetDevice(  \
	/* [annotation] */  \
	__out  ID3D10Device **ppDevice) { \
	*ppDevice = (ID3D10Device*)device; \
	device->AddRef(); \
} \
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(  \
	/* [annotation] */  \
	__in  REFGUID guid, \
	/* [annotation] */  \
	__inout  UINT *pDataSize, \
	/* [annotation] */  \
	__out_bcount_opt(*pDataSize)  void *pData) { \
	HRESULT result = ((ID3D11DeviceChild*)backend)->GetPrivateData(guid, pDataSize, pData); \
	CHECK_RETURN(result); \
} \
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(  \
	/* [annotation] */  \
	__in  REFGUID guid, \
	/* [annotation] */  \
	__in  UINT DataSize, \
	/* [annotation] */  \
	__in_bcount_opt(DataSize)  const void *pData) { \
	HRESULT result = ((ID3D11DeviceChild*)backend)->SetPrivateData(guid, DataSize, pData); \
	CHECK_RETURN(result); \
} \
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(  \
	/* [annotation] */  \
	__in  REFGUID guid, \
	/* [annotation] */  \
	__in_opt  const IUnknown *pData) { \
	HRESULT hresult = ((ID3D11DeviceChild*)backend)->SetPrivateDataInterface(guid, pData); \
	return hresult; \
}

// ----------------------------------------------------------------------------
// ID3D10Resource proxy
// ----------------------------------------------------------------------------
#define ProxyID3D10Resource \
	ProxyID3D10DeviceChild \
	virtual void STDMETHODCALLTYPE GetType(  \
	/* [annotation] */  \
	__out  D3D10_RESOURCE_DIMENSION *rType) { \
	((ID3D11Resource*)backend)->GetType((D3D11_RESOURCE_DIMENSION*)rType); \
} \
	virtual void STDMETHODCALLTYPE SetEvictionPriority(  \
	/* [annotation] */  \
	__in  UINT EvictionPriority) { \
	((ID3D11Resource*)backend)->SetEvictionPriority(EvictionPriority); \
} \
	virtual UINT STDMETHODCALLTYPE GetEvictionPriority( void) { \
	return ((ID3D11Resource*)backend)->GetEvictionPriority(); \
}


// ----------------------------------------------------------------------------
// ID3D10Resource proxy
// ----------------------------------------------------------------------------
class ProxyID3D10ResourceImpl : public ID3D10Resource {
public:
	ProxyIUnknown;
	ProxyID3D10Resource;
};

// ----------------------------------------------------------------------------
// ID3D10Buffer proxy
// ----------------------------------------------------------------------------
class ProxyID3D10Buffer : public ID3D10Buffer {
public:
	ProxyIUnknown;
	ProxyID3D10Resource;

	virtual HRESULT STDMETHODCALLTYPE Map( 
		/* [annotation] */ 
		__in  D3D10_MAP MapType,
		/* [annotation] */ 
		__in  UINT MapFlags,
		/* [annotation] */ 
		__out  void **ppData);

	virtual void STDMETHODCALLTYPE Unmap( void);

	virtual void STDMETHODCALLTYPE GetDesc( 
		/* [annotation] */ 
		__out  D3D10_BUFFER_DESC *pDesc);
};

// ----------------------------------------------------------------------------
// ID3D10Texture2D proxy
// ----------------------------------------------------------------------------
class ProxyID3D10Texture2D : public ID3D10Texture2D
{
public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) {
			HRESULT result = backend->QueryInterface(riid, ppvObject);
			CHECK_RETURN(result);
	}

	ProxyIUnknownNoQueryInterface
	ProxyID3D10Resource

	virtual HRESULT STDMETHODCALLTYPE Map( 
	/* [annotation] */ 
	__in  UINT Subresource,
	/* [annotation] */ 
	__in  D3D10_MAP MapType,
	/* [annotation] */ 
	__in  UINT MapFlags,
	/* [annotation] */ 
	__out  D3D10_MAPPED_TEXTURE2D *pMappedTex2D);

	virtual void STDMETHODCALLTYPE Unmap( 
		/* [annotation] */ 
		__in  UINT Subresource);

	virtual void STDMETHODCALLTYPE GetDesc( 
		/* [annotation] */ 
		__out  D3D10_TEXTURE2D_DESC *pDesc);
};


// ----------------------------------------------------------------------------
// Cast a ID3D10Resource to a ID3D11Resource
// If ID3D10Resource is a proxy, then get the underlying ID3D11Resource
// else return the input ID3D10Resource as it should be a ID3D11Resource
// ----------------------------------------------------------------------------
ID3D11Resource* ProxyCast(ID3D10Resource* buffer) {
	static ProxyID3D10Buffer ProxyID3D10BufferInstance;
	static ProxyID3D10Texture2D ProxyID3D10Texture2DInstance;
	if ( buffer == 0 )
		return 0;
	if (*((void**)&ProxyID3D10BufferInstance) == *((void**)buffer)) {
		return (ID3D11Resource*)((ProxyID3D10Buffer*)buffer)->backend;
	}
	if (*((void**)&ProxyID3D10Texture2DInstance) == *((void**)buffer)) {
		return (ID3D11Resource*)((ProxyID3D10Texture2D*)buffer)->backend;
	}
	return (ID3D11Resource*)buffer;
}


// ----------------------------------------------------------------------------
// Need to add this after Windows Update http://support.microsoft.com/kb/2454826
// ----------------------------------------------------------------------------
class ProxyID3D10Multithread : public ID3D10Multithread {
protected:
	ULONG counter;
	BOOL _bMTProtect;

public:
		virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) {
			//HRESULT result = backend->QueryInterface(riid, ppvObject);
			HRESULT result = E_NOINTERFACE;
			CHECK_RETURN(result);
		}

		virtual ULONG STDMETHODCALLTYPE AddRef( void) {
			counter++;
			return counter;
		}

		virtual ULONG STDMETHODCALLTYPE Release( void) {
			counter--;
			if ( counter == 0) {
				delete this;
			}
			return counter;
		}

		virtual void STDMETHODCALLTYPE Enter( void)  {
		
		}
        
		virtual void STDMETHODCALLTYPE Leave( void)  {
		
		}
        
        virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected( 
            /* [annotation] */ 
            __in  BOOL bMTProtect) {
				_bMTProtect = bMTProtect;
				return bMTProtect;
		}
        
        virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected( void)  {
			return _bMTProtect ;
		}
};



// ----------------------------------------------------------------------------
// ID3D10Device1 proxy
// ----------------------------------------------------------------------------
class ProxyID3D10Device : public ID3D10Device1
{
protected:
	__inline ID3D11Device* device() {
		return (ID3D11Device*)backend;
	}
public:
	IUnknown* backend;
	ID3D11DeviceContext* context;
	ProxyID3D10Multithread proxyMultithread;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) {
			HRESULT result;
			if (riid == __uuidof(ID3D10InfoQueue)) {
				result = backend->QueryInterface(__uuidof(ID3D11InfoQueue), ppvObject);
			} else if (riid == __uuidof(IUnknown)) {
				*ppvObject = this;
				this->AddRef();
				result = S_OK;
			} else if (riid == __uuidof(ID3D10Multithread) ) {
				*ppvObject = &proxyMultithread;
				proxyMultithread.AddRef();
				result = S_OK;
			} else {
				result = backend->QueryInterface(riid, ppvObject);
			}
			return result;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef( void) {
		context->AddRef();
		return backend->AddRef();
	}

	virtual ULONG STDMETHODCALLTYPE Release( void) {
		context->Release();
		ULONG count = backend->Release();
		if ( count == 0) {
			delete this;
		}
		return count;
	}

	virtual void STDMETHODCALLTYPE VSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D10Buffer *const *ppConstantBuffers) { 
			ID3D11Buffer* buffers[16];
			for(int i = 0; i < NumBuffers; i++)
				buffers[i] = (ID3D11Buffer*)ProxyCast(ppConstantBuffers[i]);
			context->VSSetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)(ppConstantBuffers==0)?0:buffers);	
	}

	virtual void STDMETHODCALLTYPE PSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D10ShaderResourceView *const *ppShaderResourceViews) { 
			context->PSSetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);	
	}

	virtual void STDMETHODCALLTYPE PSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D10PixelShader *pPixelShader) {
			context->PSSetShader((ID3D11PixelShader*)pPixelShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE PSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D10SamplerState *const *ppSamplers) {
			context->PSSetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);	
	}

	virtual void STDMETHODCALLTYPE VSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D10VertexShader *pVertexShader) {
			context->VSSetShader((ID3D11VertexShader*)pVertexShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE DrawIndexed( 
		/* [annotation] */ 
		__in  UINT IndexCount,
		/* [annotation] */ 
		__in  UINT StartIndexLocation,
		/* [annotation] */ 
		__in  INT BaseVertexLocation) {
			context->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);	
	}

	virtual void STDMETHODCALLTYPE Draw( 
		/* [annotation] */ 
		__in  UINT VertexCount,
		/* [annotation] */ 
		__in  UINT StartVertexLocation) {
			context->Draw(VertexCount, StartVertexLocation);
	}

	virtual void STDMETHODCALLTYPE PSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D10Buffer *const *ppConstantBuffers) { 
			ID3D11Buffer* buffers[16];
			for(int i = 0; i < NumBuffers; i++)
				buffers[i] = (ID3D11Buffer*)ProxyCast(ppConstantBuffers[i]);
			context->PSSetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)(ppConstantBuffers==0)?0:buffers);
	}

	virtual void STDMETHODCALLTYPE IASetInputLayout( 
		/* [annotation] */ 
		__in_opt  ID3D10InputLayout *pInputLayout) {
			context->IASetInputLayout((ID3D11InputLayout*)pInputLayout);
	}

	virtual void STDMETHODCALLTYPE IASetVertexBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D10Buffer *const *ppVertexBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  const UINT *pStrides,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  const UINT *pOffsets) {
			ID3D11Buffer* buffers[16];
			for(int i = 0; i < NumBuffers; i++)
				buffers[i] = (ID3D11Buffer*)ProxyCast(ppVertexBuffers[i]);
			context->IASetVertexBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)(ppVertexBuffers==0)?0:buffers, pStrides, pOffsets);
	}

	virtual void STDMETHODCALLTYPE IASetIndexBuffer( 
		/* [annotation] */ 
		__in_opt  ID3D10Buffer *pIndexBuffer,
		/* [annotation] */ 
		__in  DXGI_FORMAT Format,
		/* [annotation] */ 
		__in  UINT Offset) {
			context->IASetIndexBuffer((ID3D11Buffer*)ProxyCast(pIndexBuffer), Format, Offset);
	}

	virtual void STDMETHODCALLTYPE DrawIndexedInstanced( 
		/* [annotation] */ 
		__in  UINT IndexCountPerInstance,
		/* [annotation] */ 
		__in  UINT InstanceCount,
		/* [annotation] */ 
		__in  UINT StartIndexLocation,
		/* [annotation] */ 
		__in  INT BaseVertexLocation,
		/* [annotation] */ 
		__in  UINT StartInstanceLocation) {
			context->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	virtual void STDMETHODCALLTYPE DrawInstanced( 
		/* [annotation] */ 
		__in  UINT VertexCountPerInstance,
		/* [annotation] */ 
		__in  UINT InstanceCount,
		/* [annotation] */ 
		__in  UINT StartVertexLocation,
		/* [annotation] */ 
		__in  UINT StartInstanceLocation) {
			context->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	virtual void STDMETHODCALLTYPE GSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D10Buffer *const *ppConstantBuffers) {
			ID3D11Buffer* buffers[16];
			for(int i = 0; i < NumBuffers; i++)
				buffers[i] = (ID3D11Buffer*)ProxyCast(ppConstantBuffers[i]);
			context->GSSetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)(ppConstantBuffers==0)?0:buffers);	
	}

	virtual void STDMETHODCALLTYPE GSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D10GeometryShader *pShader) { 
			context->GSSetShader((ID3D11GeometryShader*)pShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE IASetPrimitiveTopology( 
		/* [annotation] */ 
		__in  D3D10_PRIMITIVE_TOPOLOGY Topology) { 
			context->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)Topology);
	}

	virtual void STDMETHODCALLTYPE VSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D10ShaderResourceView *const *ppShaderResourceViews) { 
			context->VSSetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);
	}

	virtual void STDMETHODCALLTYPE VSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D10SamplerState *const *ppSamplers) {
			context->VSSetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);
	}

	virtual void STDMETHODCALLTYPE SetPredication( 
		/* [annotation] */ 
		__in_opt  ID3D10Predicate *pPredicate,
		/* [annotation] */ 
		__in  BOOL PredicateValue) { 
			context->SetPredication((ID3D11Predicate*)pPredicate, PredicateValue);
	}

	virtual void STDMETHODCALLTYPE GSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D10ShaderResourceView *const *ppShaderResourceViews) {
			context->GSSetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);
	}

	virtual void STDMETHODCALLTYPE GSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D10SamplerState *const *ppSamplers) { 
			context->GSSetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);
	}

	virtual void STDMETHODCALLTYPE OMSetRenderTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount_opt(NumViews)  ID3D10RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */ 
		__in_opt  ID3D10DepthStencilView *pDepthStencilView) { 
			context->OMSetRenderTargets(NumViews, (ID3D11RenderTargetView**)ppRenderTargetViews, (ID3D11DepthStencilView*)pDepthStencilView);
	}

	virtual void STDMETHODCALLTYPE OMSetBlendState( 
		/* [annotation] */ 
		__in_opt  ID3D10BlendState *pBlendState,
		/* [annotation] */ 
		__in  const FLOAT BlendFactor[ 4 ],
		/* [annotation] */ 
		__in  UINT SampleMask) { 
			context->OMSetBlendState((ID3D11BlendState*)pBlendState, BlendFactor, SampleMask);
	}

	virtual void STDMETHODCALLTYPE OMSetDepthStencilState( 
		/* [annotation] */ 
		__in_opt  ID3D10DepthStencilState *pDepthStencilState,
		/* [annotation] */ 
		__in  UINT StencilRef) { 
			context->OMSetDepthStencilState((ID3D11DepthStencilState*)pDepthStencilState, StencilRef);
	}

	virtual void STDMETHODCALLTYPE SOSetTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount_opt(NumBuffers)  ID3D10Buffer *const *ppSOTargets,
		/* [annotation] */ 
		__in_ecount_opt(NumBuffers)  const UINT *pOffsets) { 
			context->SOSetTargets(NumBuffers, (ID3D11Buffer**)ppSOTargets, pOffsets);
	}

	virtual void STDMETHODCALLTYPE DrawAuto( void) { 
		context->DrawAuto();
	}

	virtual void STDMETHODCALLTYPE RSSetState( 
		/* [annotation] */ 
		__in_opt  ID3D10RasterizerState *pRasterizerState) { 
			context->RSSetState((ID3D11RasterizerState*)pRasterizerState);
	}

	virtual void STDMETHODCALLTYPE RSSetViewports( 
		/* [annotation] */ 
		__in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */ 
		__in_ecount_opt(NumViewports)  const D3D10_VIEWPORT *pViewports) {

			D3D11_VIEWPORT viewports[16];
			for(int i = 0; i < NumViewports; i++) {
				viewports[i].TopLeftX = pViewports[i].TopLeftX;
				viewports[i].TopLeftY = pViewports[i].TopLeftY;
				viewports[i].Width = pViewports[i].Width;
				viewports[i].Height = pViewports[i].Height;
				viewports[i].MinDepth = pViewports[i].MinDepth;
				viewports[i].MaxDepth = pViewports[i].MaxDepth;
			}
			context->RSSetViewports(NumViewports, (D3D11_VIEWPORT*)viewports);
	}

	virtual void STDMETHODCALLTYPE RSSetScissorRects( 
		/* [annotation] */ 
		__in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */ 
		__in_ecount_opt(NumRects)  const D3D10_RECT *pRects) { 
			context->RSSetScissorRects(NumRects, (D3D11_RECT*)pRects);
	}

	virtual void STDMETHODCALLTYPE CopySubresourceRegion( 
		/* [annotation] */ 
		__in  ID3D10Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in  UINT DstX,
		/* [annotation] */ 
		__in  UINT DstY,
		/* [annotation] */ 
		__in  UINT DstZ,
		/* [annotation] */ 
		__in  ID3D10Resource *pSrcResource,
		/* [annotation] */ 
		__in  UINT SrcSubresource,
		/* [annotation] */ 
		__in_opt  const D3D10_BOX *pSrcBox) { 
			ID3D11Resource* fromResource = ProxyCast((ID3D10Buffer*)pSrcResource);
			ID3D11Resource* toResource = ProxyCast((ID3D10Buffer*)pDstResource);
			context->CopySubresourceRegion(toResource, DstSubresource, DstX, DstY, DstZ, fromResource, SrcSubresource, (D3D11_BOX*)pSrcBox);
	}

	virtual void STDMETHODCALLTYPE CopyResource( 
		/* [annotation] */ 
		__in  ID3D10Resource *pDstResource,
		/* [annotation] */ 
		__in  ID3D10Resource *pSrcResource) { 
			ID3D11Resource* fromResource = ProxyCast((ID3D10Buffer*)pSrcResource);
			ID3D11Resource* toResource = ProxyCast((ID3D10Buffer*)pDstResource);
			context->CopyResource(fromResource, toResource);
	}

	virtual void STDMETHODCALLTYPE UpdateSubresource( 
		/* [annotation] */ 
		__in  ID3D10Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in_opt  const D3D10_BOX *pDstBox,
		/* [annotation] */ 
		__in  const void *pSrcData,
		/* [annotation] */ 
		__in  UINT SrcRowPitch,
		/* [annotation] */ 
		__in  UINT SrcDepthPitch) { 
			ID3D11Resource* buffer = ProxyCast((ID3D10Buffer*)pDstResource);
			context->UpdateSubresource(buffer, DstSubresource, (D3D11_BOX*)pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);	
	}

	virtual void STDMETHODCALLTYPE ClearRenderTargetView( 
		/* [annotation] */ 
		__in  ID3D10RenderTargetView *pRenderTargetView,
		/* [annotation] */ 
		__in  const FLOAT ColorRGBA[ 4 ]) { 
			context->ClearRenderTargetView((ID3D11RenderTargetView*)pRenderTargetView, ColorRGBA);
	}

	virtual void STDMETHODCALLTYPE ClearDepthStencilView( 
		/* [annotation] */ 
		__in  ID3D10DepthStencilView *pDepthStencilView,
		/* [annotation] */ 
		__in  UINT ClearFlags,
		/* [annotation] */ 
		__in  FLOAT Depth,
		/* [annotation] */ 
		__in  UINT8 Stencil) { 
			context->ClearDepthStencilView((ID3D11DepthStencilView*)pDepthStencilView, ClearFlags, Depth, Stencil);
	}

	virtual void STDMETHODCALLTYPE GenerateMips( 
		/* [annotation] */ 
		__in  ID3D10ShaderResourceView *pShaderResourceView) { 
			context->GenerateMips((ID3D11ShaderResourceView*)pShaderResourceView);
	}

	virtual void STDMETHODCALLTYPE ResolveSubresource( 
		/* [annotation] */ 
		__in  ID3D10Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in  ID3D10Resource *pSrcResource,
		/* [annotation] */ 
		__in  UINT SrcSubresource,
		/* [annotation] */ 
		__in  DXGI_FORMAT Format) { 
			ID3D11Resource* resource = ProxyCast((ID3D10Buffer*)pDstResource);
			context->ResolveSubresource(resource, DstSubresource, (ID3D11Resource*)pSrcResource, SrcSubresource, Format);
	}

	virtual void STDMETHODCALLTYPE VSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D10Buffer **ppConstantBuffers) { 
			context->VSGetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)ppConstantBuffers);
	}

	virtual void STDMETHODCALLTYPE PSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D10ShaderResourceView **ppShaderResourceViews) { 
			context->PSGetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);
	}

	virtual void STDMETHODCALLTYPE PSGetShader( 
		/* [annotation] */ 
		__out  ID3D10PixelShader **ppPixelShader) { 
			context->PSGetShader((ID3D11PixelShader**)ppPixelShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE PSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D10SamplerState **ppSamplers) { 
			context->PSGetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);
	}

	virtual void STDMETHODCALLTYPE VSGetShader( 
		/* [annotation] */ 
		__out  ID3D10VertexShader **ppVertexShader) { 
			context->VSGetShader((ID3D11VertexShader**)ppVertexShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE PSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D10Buffer **ppConstantBuffers) { 
			context->PSGetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)ppConstantBuffers);
	}

	virtual void STDMETHODCALLTYPE IAGetInputLayout( 
		/* [annotation] */ 
		__out  ID3D10InputLayout **ppInputLayout) { 
			context->IAGetInputLayout((ID3D11InputLayout**)ppInputLayout);
	}

	virtual void STDMETHODCALLTYPE IAGetVertexBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  ID3D10Buffer **ppVertexBuffers,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  UINT *pStrides,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  UINT *pOffsets) { 
			context->IAGetVertexBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)ppVertexBuffers, pStrides, pOffsets);
	}

	virtual void STDMETHODCALLTYPE IAGetIndexBuffer( 
		/* [annotation] */ 
		__out_opt  ID3D10Buffer **pIndexBuffer,
		/* [annotation] */ 
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */ 
		__out_opt  UINT *Offset) { 
			context->IAGetIndexBuffer((ID3D11Buffer**)pIndexBuffer, Format, Offset);
	}

	virtual void STDMETHODCALLTYPE GSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D10Buffer **ppConstantBuffers) { 
			context->GSGetConstantBuffers(StartSlot, NumBuffers, (ID3D11Buffer**)ppConstantBuffers);
	}

	virtual void STDMETHODCALLTYPE GSGetShader( 
		/* [annotation] */ 
		__out  ID3D10GeometryShader **ppGeometryShader) { 
			context->GSGetShader((ID3D11GeometryShader**)ppGeometryShader, 0, 0);
	}

	virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology( 
		/* [annotation] */ 
		__out  D3D10_PRIMITIVE_TOPOLOGY *pTopology) { 
			context->IAGetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY*)pTopology);
	}

	virtual void STDMETHODCALLTYPE VSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D10ShaderResourceView **ppShaderResourceViews) { 
			context->VSGetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);
	}

	virtual void STDMETHODCALLTYPE VSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D10SamplerState **ppSamplers) { 
			context->VSGetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);
	}

	virtual void STDMETHODCALLTYPE GetPredication( 
		/* [annotation] */ 
		__out_opt  ID3D10Predicate **ppPredicate,
		/* [annotation] */ 
		__out_opt  BOOL *pPredicateValue) { 
			context->GetPredication((ID3D11Predicate**)ppPredicate, pPredicateValue);
	}

	virtual void STDMETHODCALLTYPE GSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D10ShaderResourceView **ppShaderResourceViews) { 
			context->GSGetShaderResources(StartSlot, NumViews, (ID3D11ShaderResourceView**)ppShaderResourceViews);
	}

	virtual void STDMETHODCALLTYPE GSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D10SamplerState **ppSamplers) { 
			context->PSGetSamplers(StartSlot, NumSamplers, (ID3D11SamplerState**)ppSamplers);
	}

	virtual void STDMETHODCALLTYPE OMGetRenderTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount_opt(NumViews)  ID3D10RenderTargetView **ppRenderTargetViews,
		/* [annotation] */ 
		__out_opt  ID3D10DepthStencilView **ppDepthStencilView) { 
			context->OMGetRenderTargets(NumViews, (ID3D11RenderTargetView**)ppRenderTargetViews, (ID3D11DepthStencilView**)ppDepthStencilView);
	}

	virtual void STDMETHODCALLTYPE OMGetBlendState( 
		/* [annotation] */ 
		__out_opt  ID3D10BlendState **ppBlendState,
		/* [annotation] */ 
		__out_opt  FLOAT BlendFactor[ 4 ],
		/* [annotation] */ 
		__out_opt  UINT *pSampleMask) { 
			context->OMGetBlendState((ID3D11BlendState**)ppBlendState, BlendFactor, pSampleMask);
	}

	virtual void STDMETHODCALLTYPE OMGetDepthStencilState( 
		/* [annotation] */ 
		__out_opt  ID3D10DepthStencilState **ppDepthStencilState,
		/* [annotation] */ 
		__out_opt  UINT *pStencilRef) { 
			context->OMGetDepthStencilState((ID3D11DepthStencilState**)ppDepthStencilState, pStencilRef);
	}

	virtual void STDMETHODCALLTYPE SOGetTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  ID3D10Buffer **ppSOTargets,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  UINT *pOffsets) { 
			context->SOGetTargets(NumBuffers, (ID3D11Buffer**)ppSOTargets);
	}

	virtual void STDMETHODCALLTYPE RSGetState( 
		/* [annotation] */ 
		__out  ID3D10RasterizerState **ppRasterizerState) { 

			context->RSGetState((ID3D11RasterizerState**)ppRasterizerState);
	}

	virtual void STDMETHODCALLTYPE RSGetViewports( 
		/* [annotation] */ 
		__inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumViewports,
		/* [annotation] */ 
		__out_ecount_opt(*NumViewports)  D3D10_VIEWPORT *pViewports) { 
			D3D11_VIEWPORT viewports[16];
			context->RSGetViewports(NumViewports, (D3D11_VIEWPORT*)viewports);
			for(int i = 0; i < *NumViewports; i++) {
				pViewports[i].TopLeftX = viewports[i].TopLeftX;
				pViewports[i].TopLeftY = viewports[i].TopLeftY;
				pViewports[i].Width = viewports[i].Width;
				pViewports[i].Height = viewports[i].Height;
				pViewports[i].MinDepth = viewports[i].MinDepth;
				pViewports[i].MaxDepth = viewports[i].MaxDepth;
			}
	}

	virtual void STDMETHODCALLTYPE RSGetScissorRects( 
		/* [annotation] */ 
		__inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumRects,
		/* [annotation] */ 
		__out_ecount_opt(*NumRects)  D3D10_RECT *pRects) { 
			context->RSGetScissorRects(NumRects, (D3D11_RECT*)pRects);
	}

	virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason( void) { 
		return device()->GetDeviceRemovedReason();		
	}

	virtual HRESULT STDMETHODCALLTYPE SetExceptionMode( 
		UINT RaiseFlags) { 
			HRESULT result = device()->SetExceptionMode(RaiseFlags);
			CHECK_RETURN(result);
	}

	virtual UINT STDMETHODCALLTYPE GetExceptionMode( void) { 
		return device()->GetExceptionMode(); 
	}

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__inout  UINT *pDataSize,
		/* [annotation] */ 
		__out_bcount_opt(*pDataSize)  void *pData) { 
			HRESULT result = device()->GetPrivateData(guid, pDataSize, pData);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__in  UINT DataSize,
		/* [annotation] */ 
		__in_bcount_opt(DataSize)  const void *pData) { 
			HRESULT result = device()->SetPrivateData(guid, DataSize, pData);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__in_opt  const IUnknown *pData) { 
			HRESULT result = device()->SetPrivateDataInterface(guid, pData);
			CHECK_RETURN(result);
	}

	virtual void STDMETHODCALLTYPE ClearState( void) { 
		context->ClearState();	
	}

	virtual void STDMETHODCALLTYPE Flush( void) { 
		context->Flush();
	}

	virtual HRESULT STDMETHODCALLTYPE CreateBuffer( 
		/* [annotation] */ 
		__in  const D3D10_BUFFER_DESC *pDesc,
		/* [annotation] */ 
		__in_opt  const D3D10_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */ 
		__out_opt  ID3D10Buffer **ppBuffer) {  
			D3D11_BUFFER_DESC desc11;

			*((D3D10_BUFFER_DESC*)&desc11) = *pDesc;
			desc11.StructureByteStride = 0;

			ProxyID3D10Buffer* buffer = new ProxyID3D10Buffer();
			buffer->device = this;
			*ppBuffer = buffer;
			HRESULT result = device()->CreateBuffer(&desc11, (D3D11_SUBRESOURCE_DATA*)pInitialData, (ID3D11Buffer**)&buffer->backend);

			CHECK_RETURN(result);

			//			return S_OK; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture1D( 
		/* [annotation] */ 
		__in  const D3D10_TEXTURE1D_DESC *pDesc,
		/* [annotation] */ 
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */ 
		__out  ID3D10Texture1D **ppTexture1D) {
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture2D( 
		/* [annotation] */ 
		__in  const D3D10_TEXTURE2D_DESC *pDesc,
		/* [annotation] */ 
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */ 
		__out  ID3D10Texture2D **ppTexture2D) {  
			ProxyID3D10Texture2D* texture2D = new ProxyID3D10Texture2D();
			texture2D->device = this;

			HRESULT result = device()->CreateTexture2D((D3D11_TEXTURE2D_DESC*)pDesc, (D3D11_SUBRESOURCE_DATA*)pInitialData, (ID3D11Texture2D**)&texture2D->backend);
			*ppTexture2D = texture2D;

			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture3D( 
		/* [annotation] */ 
		__in  const D3D10_TEXTURE3D_DESC *pDesc,
		/* [annotation] */ 
		__in_xcount_opt(pDesc->MipLevels)  const D3D10_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */ 
		__out  ID3D10Texture3D **ppTexture3D) { 
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView( 
		/* [annotation] */ 
		__in  ID3D10Resource *pResource,
		/* [annotation] */ 
		__in_opt  const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
		/* [annotation] */ 
		__out_opt  ID3D10ShaderResourceView **ppSRView) {  
			ProxyID3D10ResourceImpl* pResourceImpl = (ProxyID3D10ResourceImpl*)pResource;
			HRESULT result = device()->CreateShaderResourceView((ID3D11Resource*)pResourceImpl->backend, (D3D11_SHADER_RESOURCE_VIEW_DESC*)pDesc, (ID3D11ShaderResourceView**)ppSRView);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView( 
		/* [annotation] */ 
		__in  ID3D10Resource *pResource,
		/* [annotation] */ 
		__in_opt  const D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
		/* [annotation] */ 
		__out_opt  ID3D10RenderTargetView **ppRTView) {  
			ProxyID3D10ResourceImpl* pResourceImpl = (ProxyID3D10ResourceImpl*)pResource;
			HRESULT result = device()->CreateRenderTargetView((ID3D11Resource*)pResourceImpl->backend, (D3D11_RENDER_TARGET_VIEW_DESC*)pDesc, (ID3D11RenderTargetView**)ppRTView);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView( 
		/* [annotation] */ 
		__in  ID3D10Resource *pResource,
		/* [annotation] */ 
		__in_opt  const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
		/* [annotation] */ 
		__out_opt  ID3D10DepthStencilView **ppDepthStencilView) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateInputLayout( 
		/* [annotation] */ 
		__in_ecount(NumElements)  const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
		/* [annotation] */ 
		__in_range( 0, D3D10_1_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
		/* [annotation] */ 
		__in  const void *pShaderBytecodeWithInputSignature,
		/* [annotation] */ 
		__in  SIZE_T BytecodeLength,
		/* [annotation] */ 
		__out_opt  ID3D10InputLayout **ppInputLayout) {  
			HRESULT result = device()->CreateInputLayout((D3D11_INPUT_ELEMENT_DESC*)pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, (ID3D11InputLayout**)ppInputLayout);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateVertexShader( 
		/* [annotation] */ 
		__in  const void *pShaderBytecode,
		/* [annotation] */ 
		__in  SIZE_T BytecodeLength,
		/* [annotation] */ 
		__out_opt  ID3D10VertexShader **ppVertexShader) {  
			HRESULT result = device()->CreateVertexShader(pShaderBytecode, BytecodeLength, 0, (ID3D11VertexShader**)ppVertexShader);
			CHECK_RETURN(result); 
	}


	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShader( 
		/* [annotation] */ 
		__in  const void *pShaderBytecode,
		/* [annotation] */ 
		__in  SIZE_T BytecodeLength,
		/* [annotation] */ 
		__out_opt  ID3D10GeometryShader **ppGeometryShader) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput( 
		/* [annotation] */ 
		__in  const void *pShaderBytecode,
		/* [annotation] */ 
		__in  SIZE_T BytecodeLength,
		/* [annotation] */ 
		__in_ecount_opt(NumEntries)  const D3D10_SO_DECLARATION_ENTRY *pSODeclaration,
		/* [annotation] */ 
		__in_range( 0, D3D10_SO_SINGLE_BUFFER_COMPONENT_LIMIT )  UINT NumEntries,
		/* [annotation] */ 
		__in  UINT OutputStreamStride,
		/* [annotation] */ 
		__out_opt  ID3D10GeometryShader **ppGeometryShader) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreatePixelShader( 
		/* [annotation] */ 
		__in  const void *pShaderBytecode,
		/* [annotation] */ 
		__in  SIZE_T BytecodeLength,
		/* [annotation] */ 
		__out_opt  ID3D10PixelShader **ppPixelShader) {  
			HRESULT result = device()->CreatePixelShader(pShaderBytecode, BytecodeLength, 0, (ID3D11PixelShader**)ppPixelShader);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateBlendState( 
		/* [annotation] */ 
		__in  const D3D10_BLEND_DESC *pBlendStateDesc,
		/* [annotation] */ 
		__out_opt  ID3D10BlendState **ppBlendState) {  
			D3D11_BLEND_DESC desc;
			desc.AlphaToCoverageEnable = pBlendStateDesc->AlphaToCoverageEnable;
			desc.IndependentBlendEnable = false;
			for(int i = 0; i < 8; i++) {
				desc.RenderTarget[i].BlendEnable = pBlendStateDesc->BlendEnable[i];
				desc.RenderTarget[i].SrcBlend = (D3D11_BLEND)pBlendStateDesc->SrcBlend;
				desc.RenderTarget[i].DestBlend = (D3D11_BLEND)pBlendStateDesc->DestBlend;
				desc.RenderTarget[i].BlendOp = (D3D11_BLEND_OP)pBlendStateDesc->BlendOp;
				desc.RenderTarget[i].SrcBlendAlpha = (D3D11_BLEND)pBlendStateDesc->SrcBlendAlpha;
				desc.RenderTarget[i].DestBlendAlpha = (D3D11_BLEND)pBlendStateDesc->DestBlendAlpha;
				desc.RenderTarget[i].BlendOpAlpha = (D3D11_BLEND_OP)pBlendStateDesc->BlendOpAlpha;
				desc.RenderTarget[i].RenderTargetWriteMask = pBlendStateDesc->RenderTargetWriteMask[i];
			}
			HRESULT result = device()->CreateBlendState(&desc, (ID3D11BlendState**)ppBlendState);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilState( 
		/* [annotation] */ 
		__in  const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		/* [annotation] */ 
		__out_opt  ID3D10DepthStencilState **ppDepthStencilState) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState( 
		/* [annotation] */ 
		__in  const D3D10_RASTERIZER_DESC *pRasterizerDesc,
		/* [annotation] */ 
		__out_opt  ID3D10RasterizerState **ppRasterizerState) {  

			HRESULT result = device()->CreateRasterizerState((D3D11_RASTERIZER_DESC*)pRasterizerDesc, (ID3D11RasterizerState**)ppRasterizerState);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSamplerState( 
		/* [annotation] */ 
		__in  const D3D10_SAMPLER_DESC *pSamplerDesc,
		/* [annotation] */ 
		__out_opt  ID3D10SamplerState **ppSamplerState) {  
			HRESULT result = device()->CreateSamplerState((D3D11_SAMPLER_DESC*)pSamplerDesc, (ID3D11SamplerState**)ppSamplerState);
			CHECK_RETURN(result); 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateQuery( 
		/* [annotation] */ 
		__in  const D3D10_QUERY_DESC *pQueryDesc,
		/* [annotation] */ 
		__out_opt  ID3D10Query **ppQuery) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreatePredicate( 
		/* [annotation] */ 
		__in  const D3D10_QUERY_DESC *pPredicateDesc,
		/* [annotation] */ 
		__out_opt  ID3D10Predicate **ppPredicate) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCounter( 
		/* [annotation] */ 
		__in  const D3D10_COUNTER_DESC *pCounterDesc,
		/* [annotation] */ 
		__out_opt  ID3D10Counter **ppCounter) { 
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CheckFormatSupport( 
		/* [annotation] */ 
		__in  DXGI_FORMAT Format,
		/* [annotation] */ 
		__out  UINT *pFormatSupport) {  
			HRESULT result = device()->CheckFormatSupport(Format, pFormatSupport);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels( 
		/* [annotation] */ 
		__in  DXGI_FORMAT Format,
		/* [annotation] */ 
		__in  UINT SampleCount,
		/* [annotation] */ 
		__out  UINT *pNumQualityLevels) {  
			HRESULT result = device()->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
			CHECK_RETURN(result);
	}

	virtual void STDMETHODCALLTYPE CheckCounterInfo( 
		/* [annotation] */ 
		__out  D3D10_COUNTER_INFO *pCounterInfo) {
			device()->CheckCounterInfo((D3D11_COUNTER_INFO*)pCounterInfo);	
	}

	virtual HRESULT STDMETHODCALLTYPE CheckCounter( 
		/* [annotation] */ 
		__in  const D3D10_COUNTER_DESC *pDesc,
		/* [annotation] */ 
		__out  D3D10_COUNTER_TYPE *pType,
		/* [annotation] */ 
		__out  UINT *pActiveCounters,
		/* [annotation] */ 
		__out_ecount_opt(*pNameLength)  LPSTR szName,
		/* [annotation] */ 
		__inout_opt  UINT *pNameLength,
		/* [annotation] */ 
		__out_ecount_opt(*pUnitsLength)  LPSTR szUnits,
		/* [annotation] */ 
		__inout_opt  UINT *pUnitsLength,
		/* [annotation] */ 
		__out_ecount_opt(*pDescriptionLength)  LPSTR szDescription,
		/* [annotation] */ 
		__inout_opt  UINT *pDescriptionLength) {  
			__debugbreak();
			return S_FALSE; 
	}

	virtual UINT STDMETHODCALLTYPE GetCreationFlags( void) { 
		return device()->GetCreationFlags();
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedResource( 
		/* [annotation] */ 
		__in  HANDLE hResource,
		/* [annotation] */ 
		__in  REFIID ReturnedInterface,
		/* [annotation] */ 
		__out_opt  void **ppResource) { 
			__debugbreak();
			return S_FALSE; 
	}

	virtual void STDMETHODCALLTYPE SetTextFilterSize( 
		/* [annotation] */ 
		__in  UINT Width,
		/* [annotation] */ 
		__in  UINT Height) {
			__debugbreak();
	}

	virtual void STDMETHODCALLTYPE GetTextFilterSize( 
		/* [annotation] */ 
		__out_opt  UINT *pWidth,
		/* [annotation] */ 
		__out_opt  UINT *pHeight) {
			__debugbreak();
	}        

	virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView1( 
		/* [annotation] */ 
		__in  ID3D10Resource *pResource,
		/* [annotation] */ 
		__in_opt  const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
		/* [annotation] */ 
		__out_opt  ID3D10ShaderResourceView1 **ppSRView) {
			__debugbreak();
			return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE CreateBlendState1( 
		/* [annotation] */ 
		__in  const D3D10_BLEND_DESC1 *pBlendStateDesc,
		/* [annotation] */ 
		__out_opt  ID3D10BlendState1 **ppBlendState) {
			__debugbreak();
			return S_FALSE; 
	}

	virtual D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE GetFeatureLevel( void) {
		D3D_FEATURE_LEVEL level = device()->GetFeatureLevel();
		// If LEVEL11 than mask it to a LEVEL10_1
		if ( level == D3D_FEATURE_LEVEL_11_0 ) 
			return D3D10_FEATURE_LEVEL_10_1;
		return (D3D10_FEATURE_LEVEL1)level;
	}
};

// --------------------------------------------------------------------------------
// ProxyID3D10Buffer methods
// --------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE ProxyID3D10Buffer::Map( 
	/* [annotation] */ 
	__in  D3D10_MAP MapType,
	/* [annotation] */ 
	__in  UINT MapFlags,
	/* [annotation] */ 
	__out  void **ppData) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT result = deviceProxy()->context->Map((ID3D11Resource*)backend, 0, (D3D11_MAP)MapType, MapFlags, &map);
		*ppData = map.pData;
		CHECK_RETURN(result);
}

void STDMETHODCALLTYPE ProxyID3D10Buffer::Unmap( void) {
	deviceProxy()->context->Unmap((ID3D11Resource*)backend, 0);
}

void STDMETHODCALLTYPE ProxyID3D10Buffer::GetDesc( 
	/* [annotation] */ 
	__out  D3D10_BUFFER_DESC *pDesc) {
		__debugbreak();
}

// --------------------------------------------------------------------------------
// ProxyID3D10Texture2D methods
// --------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE ProxyID3D10Texture2D::Map( 
	/* [annotation] */ 
	__in  UINT Subresource,
	/* [annotation] */ 
	__in  D3D10_MAP MapType,
	/* [annotation] */ 
	__in  UINT MapFlags,
	/* [annotation] */ 
	__out  D3D10_MAPPED_TEXTURE2D *pMappedTex2D) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT result = deviceProxy()->context->Map((ID3D11Resource*)backend, 0, (D3D11_MAP)MapType, MapFlags, &map);
		pMappedTex2D->pData = map.pData;
		pMappedTex2D->RowPitch = map.RowPitch;
		CHECK_RETURN(result);
}

void STDMETHODCALLTYPE ProxyID3D10Texture2D::Unmap( 
	/* [annotation] */ 
	__in  UINT Subresource) {
		deviceProxy()->context->Unmap((ID3D11Resource*)backend, 0);
}

void STDMETHODCALLTYPE ProxyID3D10Texture2D::GetDesc( 
	/* [annotation] */ 
	__out  D3D10_TEXTURE2D_DESC *pDesc) {
		((ID3D11Texture2D*)backend)->GetDesc((D3D11_TEXTURE2D_DESC*)pDesc);
}        


// ----------------------------------------------------------------------------
// IDXGIObject proxy
// ----------------------------------------------------------------------------
#define ProxyIDXGIObject \
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData( \
	/* [annotation][in] */ \
	__in  REFGUID Name, \
	/* [in] */ UINT DataSize, \
	/* [annotation][in] */  \
	__in_bcount(DataSize)  const void *pData) { \
	HRESULT result = ((IDXGIObject*)backend)->SetPrivateData(Name, DataSize, pData); \
	CHECK_RETURN(result); \
} \
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(  \
	/* [annotation][in] */  \
	__in  REFGUID Name, \
	/* [annotation][in] */  \
	__in  const IUnknown *pUnknown) { \
	HRESULT result = ((IDXGIObject*)backend)->SetPrivateDataInterface(Name, pUnknown); \
	CHECK_RETURN(result); \
} \
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(  \
	/* [annotation][in] */  \
	__in  REFGUID Name, \
	/* [annotation][out][in] */  \
	__inout  UINT *pDataSize, \
	/* [annotation][out] */  \
	__out_bcount(*pDataSize)  void *pData) { \
	HRESULT result = ((IDXGIObject*)backend)->GetPrivateData(Name, pDataSize, pData); \
	CHECK_RETURN(result); \
} \
	virtual HRESULT STDMETHODCALLTYPE GetParent(  \
	/* [annotation][in] */  \
	__in  REFIID riid, \
	/* [annotation][retval][out] */  \
	__out  void **ppParent) { \
	HRESULT result = ((IDXGIObject*)backend)->GetParent(riid, ppParent); \
	CHECK_RETURN(result); \
} \

// ----------------------------------------------------------------------------
// IDXGIDevice proxy
// ----------------------------------------------------------------------------
class ProxyIDXGIDevice : public IDXGIDevice
{
public:
	ProxyID3D10Device* device;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) {
			// TODO: CHECK riid
			HRESULT result = S_OK;
			if ( riid == __uuidof(ID3D10Device) ) {
				*ppvObject = (void*)device;
				device->AddRef();
			} else {
				result = backend->QueryInterface(riid, ppvObject);
			}
			CHECK_RETURN(result);
	}

	ProxyIUnknownNoQueryInterface;
	ProxyIDXGIObject;

	virtual HRESULT STDMETHODCALLTYPE GetAdapter( 
		/* [annotation][out] */ 
		__out  IDXGIAdapter **pAdapter) {
			HRESULT result = ((IDXGIDevice*)backend)->GetAdapter(pAdapter);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSurface( 
		/* [annotation][in] */ 
		__in  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */ 
		__in_opt  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */ 
		__out  IDXGISurface **ppSurface) {
			HRESULT result = ((IDXGIDevice*)backend)->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency( 
		/* [annotation][size_is][in] */ 
		__in_ecount(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */ 
		__out_ecount(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources) {
			HRESULT result = ((IDXGIDevice*)backend)->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority( 
		/* [in] */ INT Priority)  {
			HRESULT result = ((IDXGIDevice*)backend)->SetGPUThreadPriority(Priority);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority( 
		/* [annotation][retval][out] */ 
		__out  INT *pPriority) {
			HRESULT result = ((IDXGIDevice*)backend)->GetGPUThreadPriority(pPriority);
			CHECK_RETURN(result);
	}        
};

// ----------------------------------------------------------------------------
// IDXGIObject proxy
// ----------------------------------------------------------------------------
#define ProxyIDXGIDeviceSubObject \
	ProxyIDXGIObject; \
	IDXGIDevice* device; \
	virtual HRESULT STDMETHODCALLTYPE GetDevice(  \
	/* [annotation][in] */  \
	__in  REFIID riid, \
	/* [annotation][retval][out] */  \
	__out  void **ppDevice)  { \
	*ppDevice = device; \
	device->AddRef(); \
	return S_OK; \
} \

// ----------------------------------------------------------------------------
// IDXGISurface proxy
// ----------------------------------------------------------------------------
class ProxyIDXGISurface : public IDXGISurface
{
public:
	ProxyID3D10Texture2D* texture;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) {
			HRESULT result = S_OK;
			if ( riid == __uuidof(ID3D10Texture2D) ) {
				*ppvObject = (void*)texture;
				texture->AddRef();
			} else {
				result = backend->QueryInterface(riid, ppvObject);
			}
			CHECK_RETURN(result);
	}

	ProxyIUnknownNoQueryInterface;
	ProxyIDXGIDeviceSubObject;

	virtual HRESULT STDMETHODCALLTYPE GetDesc( 
		/* [annotation][out] */ 
		__out  DXGI_SURFACE_DESC *pDesc) {
			HRESULT result = ((IDXGISurface*)backend)->GetDesc(pDesc);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE Map( 
		/* [annotation][out] */ 
		__out  DXGI_MAPPED_RECT *pLockedRect,
		/* [in] */ UINT MapFlags)  {
			HRESULT result = ((IDXGISurface*)backend)->Map(pLockedRect, MapFlags);
			CHECK_RETURN(result);
	}

	virtual HRESULT STDMETHODCALLTYPE Unmap( void)  {
		HRESULT result = ((IDXGISurface*)backend)->Unmap();
		CHECK_RETURN(result);
	}
};

// ----------------------------------------------------------------------------
// Code4kCreateD3D10CompatibleSurface function
// Returns a surface from a D3D11 Texture2D compatible with Direct2D
// ----------------------------------------------------------------------------
IDXGISurface* Code4kCreateD3D10CompatibleSurface(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture2D) {

	ProxyID3D10Device* d3d10Device = new ProxyID3D10Device();
	ProxyID3D10Texture2D* d3d10Texture2D = new ProxyID3D10Texture2D();
	ProxyIDXGIDevice* dxgiDeviceProxy = new ProxyIDXGIDevice();
	ProxyIDXGISurface* dxgiSurfaceProxy = new ProxyIDXGISurface();
	HRESULT result;

	result = texture2D->QueryInterface(__uuidof(IDXGISurface), (void**)&dxgiSurfaceProxy->backend);
	CHECK_HRESULT(result);

	result = ((IDXGISurface*)dxgiSurfaceProxy->backend)->GetDevice(__uuidof(IDXGIDevice), (void**)&dxgiDeviceProxy->backend);
	CHECK_HRESULT(result);

	d3d10Device->backend = device;
	d3d10Device->context = context;

	dxgiDeviceProxy->device = d3d10Device;

	d3d10Texture2D->backend = texture2D;
	d3d10Texture2D->device = d3d10Device;

	dxgiSurfaceProxy->device = dxgiDeviceProxy;
	dxgiSurfaceProxy->texture = d3d10Texture2D;

	//D3D10_STATE_BLOCK_MASK mask;
	//result = D3D10StateBlockMaskEnableAll(&mask);

	//ID3D10StateBlock* block;
	//result = D3D10CreateStateBlock(d3d10Device, &mask, &block);

	//result = block->Capture();

	return dxgiSurfaceProxy;
}

// ----------------------------------------------------------------------------
// Code4kSaveTextureToFile function
// Helper function to save a texture supporting BGRA format
// ----------------------------------------------------------------------------
HRESULT Code4kSaveTextureToFile(ID3D11DeviceContext *pContext, ID3D11Texture2D* pSrcTexture, D3DX11_IMAGE_FILE_FORMAT DestFormat, LPCTSTR pDestFile) {
	HRESULT result;

	D3D11_TEXTURE2D_DESC desc;
	pSrcTexture->GetDesc(&desc);

	// If texture is using BGRA format and saving to anything else then DDS
	// perform an additionnal save in-memory to be able to save to a different format than DDS
	if ( desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM && DestFormat != D3DX11_IMAGE_FILE_FORMAT::D3DX11_IFF_DDS) {

		ID3D10Blob* blob;
		result = D3DX11SaveTextureToMemory(pContext, pSrcTexture, D3DX11_IMAGE_FILE_FORMAT::D3DX11_IFF_DDS, &blob, 0);
		CHECK_HRESULT(result);

		D3DX11_IMAGE_INFO info;
		HRESULT outresult;
		result = D3DX11GetImageInfoFromMemory(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &info, &outresult);
		CHECK_HRESULT(result);
		D3DX11_IMAGE_LOAD_INFO loadInfo;
		loadInfo.Width = info.Width;
		loadInfo.Height= info.Height;
		loadInfo.Depth = info.Depth;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.FirstMipLevel = D3DX11_DEFAULT;
		loadInfo.MipLevels = D3DX11_DEFAULT;
		loadInfo.Usage = (D3D11_USAGE)D3DX11_DEFAULT;
		loadInfo.BindFlags = D3DX11_DEFAULT;
		loadInfo.CpuAccessFlags = D3DX11_DEFAULT;
		loadInfo.MiscFlags = D3DX11_DEFAULT;
		loadInfo.Filter= D3DX11_DEFAULT;
		loadInfo.MipFilter = D3DX11_DEFAULT;
		loadInfo.pSrcInfo = NULL;
		ID3D11Texture2D* pTexture;
		ID3D11Device* pDevice;
		pContext->GetDevice(&pDevice);
		result = D3DX11CreateTextureFromMemory(pDevice, blob->GetBufferPointer(), blob->GetBufferSize(), &loadInfo, 0, (ID3D11Resource**)&pTexture, &outresult);
		CHECK_HRESULT(result);

		result = D3DX11SaveTextureToFile(pContext, pTexture, DestFormat, pDestFile);

		pDevice->Release();
		pTexture->Release();
	} else {
		result = D3DX11SaveTextureToFile(pContext, pSrcTexture, DestFormat, pDestFile);		
	}
	return result;
}

#pragma GCC diagnostic warning "-Wdelete-non-abstract-non-virtual-dtor"
