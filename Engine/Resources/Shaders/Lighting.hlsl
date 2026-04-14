// Lighting.hlsl
// Pass 2: shading completo con GI de Brixelizer.
// Mismo VS que antes — renderiza la escena otra vez leyendo el albedo
// y sumando diffuseGI como irradiancia indirecta.

cbuffer SceneConstants : register(b0) {
    row_major float4x4 model;
    row_major float4x4 view;
    row_major float4x4 projection;
    float3 lightPosition;
    float  padding1;
    float3 cameraPosition;
    float  padding2;
};

cbuffer GIConstants : register(b1) {
    float giIntensity;   // multiplicador de la contribución GI (empieza en 1.0)
    float3 padding;
};

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct PS_INPUT {
    float4 svPosition    : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal   : NORMAL;
    float2 uv            : TEXCOORD;
};

// -------------------------------------------------------------------------
PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT o;
    float4 worldPos  = mul(float4(input.position, 1.0f), model);
    o.worldPosition  = worldPos.xyz;
    o.worldNormal    = mul(input.normal, (float3x3)model);
    o.uv             = input.uv;
    float4 viewPos   = mul(worldPos, view);
    o.svPosition     = mul(viewPos, projection);
    return o;
}

// -------------------------------------------------------------------------
Texture2D    _AlbedoTex  : register(t0);
SamplerState _Sampler    : register(s0);

// diffuseGI de Brixelizer GI (NON_PIXEL_SHADER_RESOURCE → bind como SRV)
Texture2D<float4> _DiffuseGI : register(t1);

static const float ALPHA_CUTOFF = 0.1f;

float4 PSMain(PS_INPUT input) : SV_TARGET {
    float4 texColor = _AlbedoTex.Sample(_Sampler, input.uv);
    clip(texColor.a - ALPHA_CUTOFF);

    float3 N        = normalize(input.worldNormal);

    // --- Luz directa (igual que antes) ---
    float3 lightVec   = lightPosition - input.worldPosition;
    float  dist       = length(lightVec);
    float3 L          = lightVec / dist;

    const float lightRadius = 20.0f;
    float attenuation = saturate(1.0f - dist / lightRadius);
    attenuation      *= attenuation;

    float  NDotL   = max(dot(N, L), 0.0f);
    float3 direct  = texColor.rgb * NDotL * attenuation;

    // --- Ambient base ---
    float3 ambient = 0.04f * texColor.rgb;

    // --- GI indirecta de Brixelizer ---
    // _DiffuseGI está en espacio de pantalla: usamos svPosition.xy como coords de pixel
    uint2  px       = uint2(input.svPosition.xy);
    float3 gi       = _DiffuseGI.Load(int3(px, 0)).rgb;
    float3 indirect = texColor.rgb * gi * giIntensity;

    float3 finalColor = direct + ambient + indirect;
    return float4(finalColor, 1.0f);
}