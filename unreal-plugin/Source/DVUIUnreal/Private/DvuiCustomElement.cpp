#include "DvuiCustomElement.h"
#include "DvuiShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "GlobalRenderResources.h"
#include "PixelShaderUtils.h"
#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHIBufferInitializer.h"

FDvuiCustomElement::FDvuiCustomElement(
	TArray<FDvuiHostVertex>&& InVertices,
	TArray<uint32>&& InIndices,
	TArray<FDvuiDrawCmd>&& InCommands,
	FIntPoint InCanvasSize,
	FIntRect InWidgetRect)
	: Vertices(MoveTemp(InVertices))
	, Indices(MoveTemp(InIndices))
	, Commands(MoveTemp(InCommands))
	, CanvasSize(InCanvasSize)
	, WidgetRect(InWidgetRect)
{
}

void FDvuiCustomElement::Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs)
{

	if (Commands.Num() == 0 || Vertices.Num() == 0 || Indices.Num() == 0)
	{
		return;
	}

	FRHICommandListImmediate& RHICmdListImmediate = FRHICommandListImmediate::Get();

	// Upload vertex/index data once per frame via CreateBufferInitializer
	// (Lock/Unlock would only work on already-finalized buffers).
	const uint32 VBSize = Vertices.Num() * sizeof(FDvuiHostVertex);
	const uint32 IBSize = Indices.Num() * sizeof(uint32);

	FBufferRHIRef VertexBufferRHI;
	{
		FRHIBufferCreateDesc Desc =
			FRHIBufferCreateDesc::CreateVertex<FDvuiHostVertex>(TEXT("DvuiVB"), Vertices.Num())
				.AddUsage(EBufferUsageFlags::Volatile)
				.SetInitActionInitializer();
		FRHIBufferInitializer Init = RHICmdListImmediate.CreateBufferInitializer(Desc);
		Init.WriteData(Vertices.GetData(), VBSize);
		VertexBufferRHI = Init.Finalize();
	}

	FBufferRHIRef IndexBufferRHI;
	{
		FRHIBufferCreateDesc Desc =
			FRHIBufferCreateDesc::CreateIndex<uint32>(TEXT("DvuiIB"), Indices.Num())
				.AddUsage(EBufferUsageFlags::Volatile)
				.SetInitActionInitializer();
		FRHIBufferInitializer Init = RHICmdListImmediate.CreateBufferInitializer(Desc);
		Init.WriteData(Indices.GetData(), IBSize);
		IndexBufferRHI = Init.Finalize();
	}

	FRDGTextureRef OutputTexture = Inputs.OutputTexture;
	if (!OutputTexture)
	{
		return;
	}

	// Projection: map dvui canvas pixels (0..W, 0..H, top-left origin)
	// onto the widget's rect in slate output space, then to NDC.
	// Slate's draw pass already establishes a viewport / scissor; we render
	// in NDC where (-1,-1) is bottom-left.
	const float W = (float)WidgetRect.Width();
	const float H = (float)WidgetRect.Height();
	const float OffX = (float)WidgetRect.Min.X;
	const float OffY = (float)WidgetRect.Min.Y;
	const float OutW = (float)OutputTexture->Desc.Extent.X;
	const float OutH = (float)OutputTexture->Desc.Extent.Y;

	// (px,py) in dvui canvas → widget space (px,py) (1:1 since CanvasSize == widget size).
	// then to NDC: x = 2*(OffX + px)/OutW - 1, y = 1 - 2*(OffY + py)/OutH (Y flipped)
	FMatrix44f Proj = FMatrix44f(
		FPlane4f( 2.f / OutW,  0.f,         0.f, 0.f),
		FPlane4f( 0.f,        -2.f / OutH,  0.f, 0.f),
		FPlane4f( 0.f,         0.f,         1.f, 0.f),
		FPlane4f((2.f * OffX) / OutW - 1.f, 1.f - (2.f * OffY) / OutH, 0.f, 1.f)
	);

	// Capture this frame's data into the pass.
	const TArray<FDvuiDrawCmd>* CommandsPtr = &Commands;
	FBufferRHIRef CapturedVB = VertexBufferRHI;
	FBufferRHIRef CapturedIB = IndexBufferRHI;

	auto* PassParameters = GraphBuilder.AllocParameters<FDvuiPS::FParameters>();
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("DvuiCustomElement"),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, Proj, CapturedVB, CapturedIB, OutW, OutH](FRHICommandList& RHICmdList) mutable
		{
			TShaderMapRef<FDvuiVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			TShaderMapRef<FDvuiPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

			FGraphicsPipelineStateInitializer PSOInit;
			RHICmdList.ApplyCachedRenderTargets(PSOInit);
			PSOInit.PrimitiveType = PT_TriangleList;
			PSOInit.BoundShaderState.VertexDeclarationRHI = GDvuiVertexDeclaration.VertexDeclarationRHI;
			PSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			PSOInit.BoundShaderState.PixelShaderRHI  = PixelShader.GetPixelShader();
			PSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			// PMA blend: src=ONE, dst=INVERSE_SRC_ALPHA, both color and alpha.
			PSOInit.BlendState = TStaticBlendState<
				CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha,
				          BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();

			SetGraphicsPipelineState(RHICmdList, PSOInit, 0);

			FDvuiVS::FParameters VSParams{};
			VSParams.ProjectionMatrix = Proj;
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParams);

			RHICmdList.SetStreamSource(0, CapturedVB, 0);

			// Full viewport, since we built proj to handle widget offset.
			RHICmdList.SetViewport(0.f, 0.f, 0.f, OutW, OutH, 1.f);

			FRHISamplerState* Sampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			FRHITexture* WhiteTex = GWhiteTexture->TextureRHI;

			for (const FDvuiDrawCmd& Cmd : Commands)
			{
				if (Cmd.IndexCount == 0) continue;

				FDvuiPS::FParameters PSParams{};
				PSParams.ElementTexture = Cmd.Texture.IsValid() ? Cmd.Texture.GetReference() : WhiteTex;
				PSParams.ElementSampler = Sampler;
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParams);

				if (Cmd.bHasClipRect)
				{
					const FIntRect Sc = Cmd.ClipRect;
					RHICmdList.SetScissorRect(true,
						FMath::Max(0, Sc.Min.X), FMath::Max(0, Sc.Min.Y),
						FMath::Min((int32)OutW, Sc.Max.X), FMath::Min((int32)OutH, Sc.Max.Y));
				}
				else
				{
					RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
				}

				RHICmdList.DrawIndexedPrimitive(
					CapturedIB,
					/*BaseVertexIndex=*/Cmd.VertexOffset,
					/*MinIndex=*/0,
					/*NumVertices=*/Cmd.VertexCount,
					/*StartIndex=*/Cmd.IndexOffset,
					/*NumPrimitives=*/Cmd.IndexCount / 3,
					/*NumInstances=*/1);
			}

			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		});
}
