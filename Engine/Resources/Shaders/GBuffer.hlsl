// GBuffer.hlsl
// Pass 1: escribe world-space normals al RT[0].
// El depth se escribe automáticamente por el pipeline (DSV).

cbuffer SceneConstants : register(b0) {
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 projection;
    float3 lightPosition;
    float  padding1;
    float3 cameraPosition;
    float  padding2;
};

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct PS_INPUT {
    float4 position      : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal   : NORMAL;
    float2 uv            : TEXCOORD;
};

struct PS_OUTPUT {
    float4 normals : SV_Target0;   // world-space normal packed a [0,1]
};

// -------------------------------------------------------------------------
PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT o;
    float4 worldPos  = mul(float4(input.position, 1.0f), model);
    o.worldPosition  = worldPos.xyz;
    o.worldNormal    = mul(input.normal, (float3x3)model); // world-space
    o.uv             = input.uv;
    float4 viewPos   = mul(worldPos, view);
    o.position       = mul(viewPos, projection);
    return o;
}

// -------------------------------------------------------------------------
Texture2D    _AlbedoTex  : register(t0);
SamplerState _Sampler    : register(s0);

static const float ALPHA_CUTOFF = 0.1f;

PS_OUTPUT PSMain(PS_INPUT input) {
    // Alpha cutoff (igual que antes)
    float4 texColor = _AlbedoTex.Sample(_Sampler, input.uv);
    clip(texColor.a - ALPHA_CUTOFF);

    PS_OUTPUT o;
    // Normalizar aquí porque la interpolación del rasterizador no conserva longitud
    float3 N   = normalize(input.worldNormal);
    o.normals  = float4(N * 0.5f + 0.5f, 1.0f); // packed [0,1]
    return o;
}