#define FFX_BRIXELIZER_TRAVERSAL_EPS 0.001f
#define FFX_HLSL
#define FFX_GPU
#define FFX_WAVE
#include "gpu/brixelizer/ffx_brixelizer_trace_ops.h"

cbuffer SceneConstants : register(b0) {
	row_major float4x4 model;
	row_major float4x4 view;
	row_major float4x4 projection;

	float3 lightPosition;
	float  _padding0;

	float3 cameraPosition;
	float  _padding1;
};

struct VSInput {
	float3 position : POSITION;
	float3 normal   : NORMAL;
	float2 uv       : TEXCOORD;
};

struct PSInput {
	float4 position : SV_POSITION;
	float3 worldPos : POSITION;
	float3 normal   : NORMAL;
	float2 uv       : TEXCOORD;
};

// Brixelizer Stuff.
struct CascadeInfoBuffer {
    FfxBrixelizerCascadeInfo cascades[FFX_BRIXELIZER_MAX_CASCADES];
};

ConstantBuffer<CascadeInfoBuffer> _BrixelizerCascadesInfo : register(b1);

StructuredBuffer<uint> _AABBTrees  : register(t3);
StructuredBuffer<uint> _BrickAABBs : register(t4);
StructuredBuffer<uint> _BrickMaps  : register(t5);

Texture3D<float> _SDFAtlasTex : register(t6);
SamplerState _LinearClampSampler : register(s1);

// Brixelixer HLSL API Implementation.
FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 cascadeID) {
    return _BrixelizerCascadesInfo.cascades[cascadeID];
}

FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIndex) {
    float3 p0 = asfloat(uint3(_AABBTrees[elementIndex], _AABBTrees[elementIndex + 1], _AABBTrees[elementIndex + 2]));
    return p0;
}

FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIndex) {
    return _AABBTrees[elementIndex];
}

FfxUInt32 LoadBricksAABB(FfxUInt32 elementIndex) {
    return _BrickAABBs[elementIndex];
}

FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw) {
    return _SDFAtlasTex.SampleLevel(_LinearClampSampler, uvw, 0);
}

FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex) {
    return _BrickMaps[elementIndex];
}

float3 ToGridSpace(float3 worldPos, uint cascadeIndex) {
    FfxBrixelizerCascadeInfo info = _BrixelizerCascadesInfo.cascades[cascadeIndex];
    return (worldPos - info.grid_min) * info.ivoxel_size;
}
// ------------------------------------------------

Texture2D    _AlbedoTex : register(t2);
SamplerState _Sampler   : register(s0);

PSInput VSMain(VSInput input) {
	PSInput output;

	float4 world = mul(float4(input.position, 1.0f), model);
	output.worldPos = world.xyz;

	output.normal = normalize(mul(input.normal, (float3x3)model));

	float4 viewPos = mul(world, view);
	output.position = mul(viewPos, projection);

	output.uv = input.uv;

	return output;
}

static const float ALPHA_CUTOFF = 0.1f;

float4 PSMain(PSInput input) : SV_TARGET {
	float4 texColor = _AlbedoTex.Sample(_Sampler, input.uv);
	clip(texColor.a - ALPHA_CUTOFF);
	float3 albedo = texColor.rgb;

	float3 N = normalize(input.normal);
	float3 L = normalize(float3(0.5, 1.0, -0.5));

	float NdotL = max(dot(N, L), 0.0);

	float3 diffuse = albedo * NdotL;
	float3 ambient = 0.1 * albedo;
	float3 color = diffuse + ambient;

	return float4(color, 1.0);
}