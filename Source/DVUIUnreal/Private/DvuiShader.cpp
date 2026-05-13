#include "DvuiShader.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PixelShaderUtils.h"

IMPLEMENT_GLOBAL_SHADER(FDvuiVS, "/DVUIUnreal/Private/DvuiShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FDvuiPS, "/DVUIUnreal/Private/DvuiShader.usf", "MainPS", SF_Pixel);

void FDvuiVertexDeclaration::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;
	const uint16 Stride = sizeof(FDvuiHostVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDvuiHostVertex, Position),    VET_Float2, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDvuiHostVertex, UV),          VET_Float2, 1, Stride));
	// VET_Color reads memory as B,G,R,A. We swizzle dvui's RGBA into BGRA
	// in SDVUIWidget::QueueDrawData, so the declaration stays VET_Color.
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDvuiHostVertex, PackedColor), VET_Color,  2, Stride));
	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FDvuiVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}

TGlobalResource<FDvuiVertexDeclaration> GDvuiVertexDeclaration;
