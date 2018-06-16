#include "GfxEmbeddedShaders.h"

namespace Rush
{

const char* MSL_EmbeddedShaders = R"(
#include <metal_stdlib>
#include <metal_common>
#include <metal_matrix>

using namespace metal;

struct Constants
{
	float4x4 matViewProj;
	float4 transform2D;
};

struct VSInput
{
	float3 pos [[attribute(0)]];
	float2 tex [[attribute(1)]];
	float4 col [[attribute(2)]];
};

struct PSInput
{
	float2 tex [[attribute(0)]];
	float4 col [[attribute(1)]];
};

struct VSOutput
{
	float4 pos [[position]];
	float2 tex;
	float4 col;
};

vertex VSOutput vsMain3D(
	VSInput v [[stage_in]],
	constant Constants& cb [[buffer(0)]])
{
	VSOutput res;
    res.pos = float4(v.pos, 1) * cb.matViewProj;
	res.col = v.col;
	res.tex = v.tex;
	return res;
}

vertex VSOutput vsMain2D(
	VSInput v [[stage_in]],
	constant Constants& cb [[buffer(0)]])
{
	VSOutput res;
    res.pos.xy = v.pos.xy * cb.transform2D.xy + cb.transform2D.zw;
	res.pos.zw = float2(v.pos.z, 1);
	res.col = v.col;
	res.tex = v.tex;
	return res;
}

fragment float4 psMain(
	PSInput v [[stage_in]])
{
	return v.col;
}

fragment float4 psMainTextured(
	PSInput v [[stage_in]], texture2d<float> t [[texture(2)]], sampler s [[sampler(1)]])
{
	return v.col * t.sample(s, v.tex);
}
)";

}
