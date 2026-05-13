// dvui-unreal: TGlobalShader pair matching DvuiShader.usf.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RHI.h"

// Vertex layout for our dvui CVertex (matches DVUIVertex in DVUI.h):
//   float x, y;        // ATTRIBUTE0
//   float u, v;        // ATTRIBUTE1
//   uint32_t color;    // ATTRIBUTE2 (BGRA in memory after swizzle from dvui's RGBA)
struct FDvuiHostVertex
{
	float Position[2];
	float UV[2];
	uint32 PackedColor;
};
static_assert(sizeof(FDvuiHostVertex) == 20, "FDvuiHostVertex must match dvui CVertex");

class FDvuiVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDvuiVS);
	SHADER_USE_PARAMETER_STRUCT(FDvuiVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FDvuiPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDvuiPS);
	SHADER_USE_PARAMETER_STRUCT(FDvuiPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, ElementTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ElementSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FDvuiVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
};

extern TGlobalResource<FDvuiVertexDeclaration> GDvuiVertexDeclaration;
