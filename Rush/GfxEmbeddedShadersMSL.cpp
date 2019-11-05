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
	float4 color;
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

struct ResourcesPlain
{
	constant Constants* cb [[id(0)]];
};

struct ResourcesTextured
{
	constant Constants* cb [[id(0)]];
	sampler             s  [[id(1)]];
	texture2d<float>    t  [[id(2)]];
};

vertex VSOutput vsMain3D(
	VSInput v [[stage_in]],
	constant ResourcesPlain& resources [[buffer(0)]])
{
	VSOutput res;
    res.pos = float4(v.pos, 1) * resources.cb->matViewProj;
	res.col = v.col * resources.cb->color;
	res.tex = v.tex;
	return res;
}

vertex VSOutput vsMain2D(
	VSInput v [[stage_in]],
	constant ResourcesPlain& resources [[buffer(0)]])
{
	VSOutput res;
    res.pos.xy = v.pos.xy * resources.cb->transform2D.xy + resources.cb->transform2D.zw;
	res.pos.zw = float2(v.pos.z, 1);
	res.col = v.col * resources.cb->color;
	res.tex = v.tex;
	return res;
}

fragment float4 psMain(
	PSInput v [[stage_in]],
	constant ResourcesPlain& resources [[buffer(0)]])
{
	return v.col;
}

fragment float4 psMainTextured(
	PSInput v [[stage_in]],
	constant ResourcesTextured& resources [[buffer(0)]])
{
	return v.col * resources.t.sample(resources.s, v.tex);
}
)";

}
