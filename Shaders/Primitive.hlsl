struct Constants
{
	float4x4 matViewProj;
	float4 transform2D;
	float4 color;
};

struct VSInput
{
	float3 a_pos0 : POSITION0;
	float2 a_tex0 : TEXCOORD0;
	float4 a_col0 : COLOR0;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float2 tex : TEXCOORD0;
	float4 col : COLOR0;
};

cbuffer constantBuffer0 : register(b0)
{
	Constants constantBuffer0;
};

SamplerState sampler0 : register(s1);
Texture2D<float4> texture0 : register(t2);

VSOutput vsMain3D(VSInput v)
{
	VSOutput res;
	res.pos = mul(float4(v.a_pos0, 1), constantBuffer0.matViewProj);
	res.col = v.a_col0;
	res.tex = v.a_tex0;
	return res;
}

VSOutput vsMain2D(VSInput v)
{
	VSOutput res;
    res.pos.xy = v.a_pos0.xy * constantBuffer0.transform2D.xy + constantBuffer0.transform2D.zw;
	res.pos.zw = float2(v.a_pos0.z, 1);
	res.col = v.a_col0;
	res.tex = v.a_tex0;
	return res;
}

float4 psMain(VSOutput v) : SV_Target
{
	return v.col * constantBuffer0.color;
}

float4 psMainTextured(VSOutput v) : SV_Target
{
	return v.col * texture0.Sample(sampler0, v.tex) * constantBuffer0.color;
}
