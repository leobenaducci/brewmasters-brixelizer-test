cbuffer SceneConstants : register(b0) {
	row_major float4x4 model;
	row_major float4x4 view;
	row_major float4x4 projection;

	float3 lightPosition;
	float  pad0;

	float3 cameraPosition;
	float  pad1;
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

Texture2D    _AlbedoTex : register(t0);

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