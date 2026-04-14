cbuffer SceneConstants : register(b0) {
	row_major float4x4 model;
	row_major float4x4 view;
	row_major float4x4 projection;
	float3 lightPosition;
	float padding1;
	float3 cameraPosition;
	float padding2;
};

struct VS_INPUT {
	float3 position : POSITION;
	float3 normal   : NORMAL;
	float2 uv       : TEXCOORD;
};

struct PS_INPUT {
	float4 position      : SV_POSITION;
	float3 worldPosition : POSITION;
	float3 normal        : NORMAL;
	float2 uv            : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input) {
	PS_INPUT output;

	float4 worldPos      = mul(float4(input.position, 1.0f), model);
	output.worldPosition = worldPos.xyz;
	output.normal        = mul(input.normal, (float3x3)model);
	output.uv            = input.uv;

	float4 viewPos   = mul(worldPos, view);
	output.position  = mul(viewPos, projection);

	return output;
}

Texture2D    _AlbedoTex : register(t0);
SamplerState _Sampler   : register(s0);

static const float ALPHA_CUTOFF = 0.1f;

float4 PSMain(PS_INPUT input) : SV_TARGET {
	float4 texColor = _AlbedoTex.Sample(_Sampler, input.uv);

	clip(texColor.a - ALPHA_CUTOFF);

	float3 N        = normalize(input.normal);
	float3 lightVec = lightPosition - input.worldPosition;
	float  dist     = length(lightVec);
	float3 L        = lightVec / dist;

	const float lightRadius = 20.0;
	float attenuation = saturate(1.0 - dist / lightRadius);
	attenuation *= attenuation;

	float NDotL = max(dot(N, L), 0.0);

	float3 diffuse    = NDotL * attenuation;
	float3 ambient    = 0.04f * texColor.rgb;
	float3 finalColor = texColor.rgb * diffuse + ambient;

	return float4(finalColor, 1.0f);
}