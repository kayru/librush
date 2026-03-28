#include "GfxPrimitiveBatch.h"

#include "GfxCommon.h"
#include "GfxDevice.h"
#include "GfxEmbeddedShaders.h"
#include "UtilLog.h"

namespace Rush
{

extern const char* MSL_EmbeddedShaders;


PrimitiveBatch::PrimitiveBatch(u32 maxBatchVertices)
: m_context(Gfx_AcquireContext())
, m_maxBatchVertices(maxBatchVertices)
, m_mode(BatchMode_Invalid)
, m_currColor(ColorRGBA::White())
, m_currPrim(GfxPrimitive::TriangleList)
, m_constantBufferDirty(true)
{
	RUSH_ASSERT(maxBatchVertices % 6 == 0);

	m_constants.color          = m_currColor;
	m_constants.transform2D    = Vec4(1, 1, 0, 0);
	m_constants.viewProjMatrix = Mat4::identity();

	const GfxCapability& caps = Gfx_GetCapability();

	if (caps.shaderTypeSupported(GfxShaderSourceType_SPV))
	{
		m_vertexShader2D = Gfx_CreateVertexShader(
		    GfxShaderSource(GfxShaderSourceType_SPV, (const char*)SPV_vsMain2D_data, SPV_vsMain2D_size, "vsMain2D"));
		m_vertexShader3D = Gfx_CreateVertexShader(
		    GfxShaderSource(GfxShaderSourceType_SPV, (const char*)SPV_vsMain3D_data, SPV_vsMain3D_size, "vsMain3D"));
		m_pixelShaderPlain = Gfx_CreatePixelShader(
		    GfxShaderSource(GfxShaderSourceType_SPV, (const char*)SPV_psMain_data, SPV_psMain_size, "psMain"));
		m_pixelShaderTextured = Gfx_CreatePixelShader(GfxShaderSource(
		    GfxShaderSourceType_SPV, (const char*)SPV_psMainTextured_data, SPV_psMainTextured_size, "psMainTextured"));
	}
	else if (caps.shaderTypeSupported(GfxShaderSourceType_DXBC))
	{
		m_vertexShader2D = Gfx_CreateVertexShader(
		    GfxShaderSource(GfxShaderSourceType_DXBC, (const char*)DXBC_vsMain2D_data, DXBC_vsMain2D_size));
		m_vertexShader3D = Gfx_CreateVertexShader(
		    GfxShaderSource(GfxShaderSourceType_DXBC, (const char*)DXBC_vsMain3D_data, DXBC_vsMain3D_size));
		m_pixelShaderPlain = Gfx_CreatePixelShader(
		    GfxShaderSource(GfxShaderSourceType_DXBC, (const char*)DXBC_psMain_data, DXBC_psMain_size));
		m_pixelShaderTextured = Gfx_CreatePixelShader(
		    GfxShaderSource(GfxShaderSourceType_DXBC, (const char*)DXBC_psMainTextured_data, DXBC_psMainTextured_size));
	}
	else if (caps.shaderTypeSupported(GfxShaderSourceType_MSL))
	{
		m_vertexShader2D =
		    Gfx_CreateVertexShader(GfxShaderSource(GfxShaderSourceType_MSL, MSL_EmbeddedShaders, 0, "vsMain2D"));
		m_vertexShader3D =
		    Gfx_CreateVertexShader(GfxShaderSource(GfxShaderSourceType_MSL, MSL_EmbeddedShaders, 0, "vsMain3D"));
		m_pixelShaderPlain =
		    Gfx_CreatePixelShader(GfxShaderSource(GfxShaderSourceType_MSL, MSL_EmbeddedShaders, 0, "psMain"));
		m_pixelShaderTextured =
		    Gfx_CreatePixelShader(GfxShaderSource(GfxShaderSourceType_MSL, MSL_EmbeddedShaders, 0, "psMainTextured"));
	}
#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
	else
	{
		RUSH_LOG_FATAL("Rendering back-end does not support SPIR-V, DXBC or MSL shaders.");
	}
#endif // RUSH_RENDER_API != RUSH_RENDER_API_NULL

	m_vertexFormat2D.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
	m_vertexFormat2D.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Texcoord, 0);
	m_vertexFormat2D.add(0, GfxVertexFormatDesc::DataType::Color, GfxVertexFormatDesc::Semantic::Color, 0);
	m_vertexFormat3D = m_vertexFormat2D;

	GfxBufferDesc desc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(Constants));
	m_constantBuffer = Gfx_CreateBuffer(desc, nullptr);

	m_plainBindings.descriptorSets[0].constantBuffers = 1;

	m_texturedBindings.descriptorSets[0].constantBuffers = 1;
	m_texturedBindings.descriptorSets[0].samplers        = 1;
	m_texturedBindings.descriptorSets[0].textures        = 1;

	GfxBufferDesc vbDesc(GfxBufferFlags::TransientVertex, GfxFormat_Unknown, m_maxBatchVertices, sizeof(BatchVertex));

	m_vertexBuffer = Gfx_CreateBuffer(vbDesc);

	BatchVertex* vertexData = new BatchVertex[m_maxBatchVertices];
	m_vertices              = LinearAllocator<BatchVertex>(vertexData, m_maxBatchVertices);

	m_samplerLiner = Gfx_CreateSamplerState(GfxSamplerDesc::makeLinear());
	m_samplerPoint = Gfx_CreateSamplerState(GfxSamplerDesc::makePoint());

	m_currSampler = m_samplerPoint.get();

	m_depth = caps.deviceNearDepth;
}

PrimitiveBatch::~PrimitiveBatch()
{
	delete[] m_vertices.data();
	Gfx_Release(m_context);
}

GfxRenderPipelineDesc PrimitiveBatch::makePipelineDesc(PipelineID id, const GfxRenderTargetDesc& renderTargetDesc) const
{
	const u32  bits     = u32(id);
	const bool textured = (bits & PipelineBit_Textured) != 0;
	const bool is3D     = (bits & PipelineBit_Is3D) != 0;
	const bool lines    = (bits & PipelineBit_Lines) != 0;

	GfxRenderPipelineDesc desc;
	desc.ps           = textured ? m_pixelShaderTextured.get() : m_pixelShaderPlain.get();
	desc.vs           = is3D ? m_vertexShader3D.get() : m_vertexShader2D.get();
	desc.vertexFormat = is3D ? m_vertexFormat3D : m_vertexFormat2D;
	desc.bindings     = textured ? m_texturedBindings : m_plainBindings;
	desc.blend[0]     = GfxBlendStateDesc::makeLerp();
	desc.depthStencil = GfxDepthStencilDesc::makeReadOnly(GfxCompareFunc::Always);
	desc.rasterizer   = GfxRasterizerDesc::makeNoCull();
	desc.primitive    = lines ? GfxPrimitive::LineList : GfxPrimitive::TriangleList;
	desc.renderTarget = renderTargetDesc;
	return desc;
}

PrimitiveBatch::PipelineSet& PrimitiveBatch::findOrAddPipelineSet(const GfxRenderTargetDesc& renderTargetDesc)
{
	for (auto& set : m_pipelineCache)
	{
		if (set.renderTarget == renderTargetDesc)
		{
			return set;
		}
	}
	m_pipelineCache.push_back({});
	m_pipelineCache.back().renderTarget = renderTargetDesc;
	return m_pipelineCache.back();
}

GfxRenderPipeline PrimitiveBatch::getOrCreatePipeline(const GfxRenderTargetDesc& renderTargetDesc, PipelineID id)
{
	PipelineSet& set = findOrAddPipelineSet(renderTargetDesc);
	if (!set.pipelines[u32(id)].valid())
	{
		set.pipelines[u32(id)] = Gfx_CreateRenderPipeline(makePipelineDesc(id, renderTargetDesc));
	}
	return set.pipelines[u32(id)].get();
}

void PrimitiveBatch::createPipelines(const GfxRenderTargetDesc& renderTargetDesc)
{
	for (u32 i = 0; i < PipelineID_COUNT; ++i)
	{
		getOrCreatePipeline(renderTargetDesc, PipelineID(i));
	}
}

GfxRenderPipeline PrimitiveBatch::getNextPipeline()
{
	const u32 bits = (m_currTexture.valid() ? PipelineBit_Textured : 0)
	               | (m_currPrim == GfxPrimitive::LineList ? PipelineBit_Lines : 0)
	               | (m_mode == BatchMode_3D ? PipelineBit_Is3D : 0);
	const PipelineID id = PipelineID(bits);

	const GfxRenderTargetDesc renderTargetDesc = Gfx_GetRenderTargetDesc(m_context);
	return getOrCreatePipeline(renderTargetDesc, id);
}

void PrimitiveBatch::flush()
{
	RUSH_ASSERT(m_mode != BatchMode_Invalid);

	if (m_vertices.size() == 0)
	{
		return;
	}

	GfxRenderPipeline nextPipeline = getNextPipeline();

	Gfx_SetRenderPipeline(m_context, nextPipeline);

	Gfx_UpdateBuffer(m_context, m_vertexBuffer, m_vertices.data(), (u32)m_vertices.sizeInBytes());
	Gfx_SetTexture(m_context, 0, m_currTexture);
	Gfx_SetSampler(m_context, 0, m_currSampler);
	Gfx_SetVertexStream(m_context, 0, m_vertexBuffer);

	if (m_constantBuffer.valid())
	{
		if (m_constantBufferDirty)
		{
			Gfx_UpdateBuffer(m_context, m_constantBuffer, &m_constants, sizeof(m_constants));
			m_constantBufferDirty = false;
		}
		Gfx_SetConstantBuffer(m_context, 0, m_constantBuffer);
	}

	Gfx_Draw(m_context, 0, (u32)m_vertices.size());

	m_vertices.clear();
}

void PrimitiveBatch::drawBox(const Box3& box, const ColorRGBA8& color)
{
	Vec3 v[8];

	v[0] = box.center() + box.dimensions() * Vec3(-0.5f, -0.5f, -0.5f);
	v[1] = box.center() + box.dimensions() * Vec3(+0.5f, -0.5f, -0.5f);
	v[2] = box.center() + box.dimensions() * Vec3(-0.5f, +0.5f, -0.5f);
	v[3] = box.center() + box.dimensions() * Vec3(+0.5f, +0.5f, -0.5f);
	v[4] = box.center() + box.dimensions() * Vec3(-0.5f, -0.5f, +0.5f);
	v[5] = box.center() + box.dimensions() * Vec3(+0.5f, -0.5f, +0.5f);
	v[6] = box.center() + box.dimensions() * Vec3(-0.5f, +0.5f, +0.5f);
	v[7] = box.center() + box.dimensions() * Vec3(+0.5f, +0.5f, +0.5f);

	drawLine(Line3(v[0], v[1]), color);
	drawLine(Line3(v[2], v[3]), color);
	drawLine(Line3(v[4], v[5]), color);
	drawLine(Line3(v[6], v[7]), color);

	drawLine(Line3(v[0], v[4]), color);
	drawLine(Line3(v[1], v[5]), color);
	drawLine(Line3(v[2], v[6]), color);
	drawLine(Line3(v[3], v[7]), color);

	drawLine(Line3(v[0], v[2]), color);
	drawLine(Line3(v[1], v[3]), color);
	drawLine(Line3(v[4], v[6]), color);
	drawLine(Line3(v[5], v[7]), color);
}

void PrimitiveBatch::drawTriangle(const Vec2& a, const Vec2& b, const Vec2& c, const ColorRGBA8& color)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 3);

	v[0].pos.x = a.x;
	v[0].pos.y = a.y;
	v[0].pos.z = m_depth;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = color;

	v[1].pos.x = b.x;
	v[1].pos.y = b.y;
	v[1].pos.z = m_depth;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = color;

	v[2].pos.x = c.x;
	v[2].pos.y = c.y;
	v[2].pos.z = m_depth;
	v[2].tex.x = 0;
	v[2].tex.y = 0;
	v[2].col   = color;
}

void PrimitiveBatch::drawTriangle(
    const Vec2& a, const Vec2& b, const Vec2& c, const ColorRGBA8& ca, const ColorRGBA8& cb, const ColorRGBA8& cc)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 3);

	v[0].pos.x = a.x;
	v[0].pos.y = a.y;
	v[0].pos.z = m_depth;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = ca;

	v[1].pos.x = b.x;
	v[1].pos.y = b.y;
	v[1].pos.z = m_depth;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = cb;

	v[2].pos.x = c.x;
	v[2].pos.y = c.y;
	v[2].pos.z = m_depth;
	v[2].tex.x = 0;
	v[2].tex.y = 0;
	v[2].col   = cc;
}

void PrimitiveBatch::drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const ColorRGBA8& color)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 3);

	v[0].pos.x = a.x;
	v[0].pos.y = a.y;
	v[0].pos.z = a.z;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = color;

	v[1].pos.x = b.x;
	v[1].pos.y = b.y;
	v[1].pos.z = b.z;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = color;

	v[2].pos.x = c.x;
	v[2].pos.y = c.y;
	v[2].pos.z = c.z;
	v[2].tex.x = 0;
	v[2].tex.y = 0;
	v[2].col   = color;
}

void PrimitiveBatch::drawTriangle(
    const Vec3& pa, const Vec3& pb, const Vec3& pc, const ColorRGBA8& ca, const ColorRGBA8& cb, const ColorRGBA8& cc)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 3);

	v[0].pos.x = pa.x;
	v[0].pos.y = pa.y;
	v[0].pos.z = pa.z;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = ca;

	v[1].pos.x = pb.x;
	v[1].pos.y = pb.y;
	v[1].pos.z = pb.z;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = cb;

	v[2].pos.x = pc.x;
	v[2].pos.y = pc.y;
	v[2].pos.z = pc.z;
	v[2].tex.x = 0;
	v[2].tex.y = 0;
	v[2].col   = cc;
}

void PrimitiveBatch::begin2D(const Vec2& scale, const Vec2& bias)
{
	RUSH_ASSERT(m_mode == BatchMode_Invalid);

	m_constants.transform2D = Vec4(scale.x, scale.y, bias.x, bias.y);
	m_constantBufferDirty   = true;
	m_mode                  = BatchMode_2D;
}

void PrimitiveBatch::begin2D(const Box2& bounds)
{
	float width  = bounds.width();
	float height = bounds.height();

	Vec2 scale = Vec2(2.0f / width, -2.0f / height);
	Vec2 bias  = Vec2(-1.0f, 1.0f);

	begin2D(scale, bias);
}

void PrimitiveBatch::begin2D(float width, float height)
{
	Box2 bounds(0, 0, width, height);
	begin2D(bounds);
}

void PrimitiveBatch::begin3D(const Mat4& viewProjMatrix)
{
	RUSH_ASSERT(m_mode == BatchMode_Invalid);
	m_constants.viewProjMatrix = viewProjMatrix.transposed();
	m_constantBufferDirty      = true;
	m_mode                     = BatchMode_3D;
}

void PrimitiveBatch::end2D()
{
	RUSH_ASSERT(m_mode == BatchMode_2D);
	flush();
	m_mode = BatchMode_Invalid;
}

void PrimitiveBatch::end3D()
{
	RUSH_ASSERT(m_mode == BatchMode_3D);
	flush();
	m_mode = BatchMode_Invalid;
}

void PrimitiveBatch::drawTexturedQuad(const Box2& rect, ColorRGBA8 color)
{
	drawTexturedQuad(rect.bl(), rect.br(), rect.tr(), rect.tl(), color);
}

void PrimitiveBatch::drawTexturedQuad(const TexturedQuad2D* q, ColorRGBA8 color)
{
	drawTexturedQuad(q->pos[0], q->pos[1], q->pos[2], q->pos[3], color, q->tex[0], q->tex[1], q->tex[2], q->tex[3]);
}

void PrimitiveBatch::drawTexturedQuad(const TexturedQuad3D* q, ColorRGBA8 color)
{
	drawTexturedQuad(q->pos[0], q->pos[1], q->pos[2], q->pos[3], color, q->tex[0], q->tex[1], q->tex[2], q->tex[3]);
}

void PrimitiveBatch::drawTexturedQuad(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, ColorRGBA8 color,
    const Vec2& ta, const Vec2& tb, const Vec2& tc, const Vec2& td)
{
	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 6);

	size_t i = 0;

	v[i].pos.x = a.x;
	v[i].pos.y = a.y;
	v[i].pos.z = m_depth;
	v[i].tex   = ta;
	v[i].col   = color;
	++i;

	v[i].pos.x = b.x;
	v[i].pos.y = b.y;
	v[i].pos.z = m_depth;
	v[i].tex   = tb;
	v[i].col   = color;
	++i;

	v[i].pos.x = c.x;
	v[i].pos.y = c.y;
	v[i].pos.z = m_depth;
	v[i].tex   = tc;
	v[i].col   = color;
	++i;

	v[i].pos.x = a.x;
	v[i].pos.y = a.y;
	v[i].pos.z = m_depth;
	v[i].tex   = ta;
	v[i].col   = color;
	++i;

	v[i].pos.x = c.x;
	v[i].pos.y = c.y;
	v[i].pos.z = m_depth;
	v[i].tex   = tc;
	v[i].col   = color;
	++i;

	v[i].pos.x = d.x;
	v[i].pos.y = d.y;
	v[i].pos.z = m_depth;
	v[i].tex   = td;
	v[i].col   = color;
	++i;
}

void PrimitiveBatch::drawTexturedQuad(const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pd, ColorRGBA8 color,
    const Vec2& ta, const Vec2& tb, const Vec2& tc, const Vec2& td)
{
	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 6);

	size_t i = 0;

	v[i].pos  = pa;
	v[i].tex  = ta;
	v[i].col  = color;
	++i;

	v[i].pos  = pb;
	v[i].tex  = tb;
	v[i].col  = color;
	++i;

	v[i].pos  = pc;
	v[i].tex  = tc;
	v[i].col  = color;
	++i;

	v[i].pos  = pa;
	v[i].tex  = ta;
	v[i].col  = color;
	++i;

	v[i].pos  = pc;
	v[i].tex  = tc;
	v[i].col  = color;
	++i;

	v[i].pos  = pd;
	v[i].tex  = td;
	v[i].col  = color;
	++i;
}

void PrimitiveBatch::drawRect(const Box2& rect, const ColorRGBA8& color)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::TriangleList, 6);

	size_t i = 0;

	v[i].pos.x = rect.tl().x;
	v[i].pos.y = rect.tl().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;

	v[i].pos.x = rect.bl().x;
	v[i].pos.y = rect.bl().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;

	v[i].pos.x = rect.br().x;
	v[i].pos.y = rect.br().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;

	v[i].pos.x = rect.tl().x;
	v[i].pos.y = rect.tl().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;

	v[i].pos.x = rect.br().x;
	v[i].pos.y = rect.br().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;

	v[i].pos.x = rect.tr().x;
	v[i].pos.y = rect.tr().y;
	v[i].pos.z = m_depth;
	v[i].tex.x = 0;
	v[i].tex.y = 0;
	v[i].col   = color;
	++i;
}

void PrimitiveBatch::drawLine(float ax, float ay, float bx, float by, const ColorRGBA8& color)
{
	drawLine(Line2(ax, ay, bx, by), color);
}

void PrimitiveBatch::drawLine(float ax, float ay, float az, float bx, float by, float bz, const ColorRGBA8& color)
{
	drawLine(Line3(ax, ay, az, bx, by, bz), color);
}

void PrimitiveBatch::drawLine(const Vec2& a, const Vec2& b, const ColorRGBA8& color) { drawLine(Line2(a, b), color); }

void PrimitiveBatch::drawLine(const Vec3& a, const Vec3& b, const ColorRGBA8& color) { drawLine(Line3(a, b), color); }

void PrimitiveBatch::drawLine(const Line3& line, const ColorRGBA8& color)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::LineList, 2);

	v[0].pos.x = line.start.x;
	v[0].pos.y = line.start.y;
	v[0].pos.z = line.start.z;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = color;

	v[1].pos.x = line.end.x;
	v[1].pos.y = line.end.y;
	v[1].pos.z = line.end.z;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = color;
}

void PrimitiveBatch::drawLine(const Line3& line, const ColorRGBA8& colorStart, const ColorRGBA8& colorEnd)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::LineList, 2);

	v[0].pos.x = line.start.x;
	v[0].pos.y = line.start.y;
	v[0].pos.z = line.start.z;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = colorStart;

	v[1].pos.x = line.end.x;
	v[1].pos.y = line.end.y;
	v[1].pos.z = line.end.z;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = colorEnd;
}

void PrimitiveBatch::drawLine(const Line2& line, const ColorRGBA8& colorStart, const ColorRGBA8& colorEnd)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::LineList, 2);

	v[0].pos.x = line.start.x;
	v[0].pos.y = line.start.y;
	v[0].pos.z = m_depth;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = colorStart;

	v[1].pos.x = line.end.x;
	v[1].pos.y = line.end.y;
	v[1].pos.z = m_depth;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = colorEnd;
}

void PrimitiveBatch::drawLine(const Line2& line, const ColorRGBA8& color)
{
	setTexture(InvalidResourceHandle());

	BatchVertex* v = drawVertices(GfxPrimitive::LineList, 2);

	v[0].pos.x = line.start.x;
	v[0].pos.y = line.start.y;
	v[0].pos.z = m_depth;
	v[0].tex.x = 0;
	v[0].tex.y = 0;
	v[0].col   = color;

	v[1].pos.x = line.end.x;
	v[1].pos.y = line.end.y;
	v[1].pos.z = m_depth;
	v[1].tex.x = 0;
	v[1].tex.y = 0;
	v[1].col   = color;
}

void PrimitiveBatch::drawCross(const Vec3& pos, float size, const ColorRGBA8& color)
{
	drawLine(Line3(pos + Vec3(-0.5f, 0, 0) * size, pos + Vec3(+0.5f, 0, 0) * size), color);
	drawLine(Line3(pos + Vec3(0, -0.5f, 0) * size, pos + Vec3(0, +0.5f, 0) * size), color);
	drawLine(Line3(pos + Vec3(0, 0, -0.5f) * size, pos + Vec3(0, 0, +0.5f) * size), color);
}

void PrimitiveBatch::setColor(ColorRGBA color)
{
	if (memcmp(&m_currColor, &color, sizeof(color)))
	{
		flush();
		m_currColor           = color;
		m_constants.color     = color;
		m_constantBufferDirty = true;
	}
}

void PrimitiveBatch::setTexture(GfxTextureArg tex)
{
	if (m_currTexture != tex)
	{
		flush();
		m_currTexture = tex;
	}
}

void PrimitiveBatch::setSampler(GfxSamplerArg smp)
{
	if (m_currSampler != smp)
	{
		flush();
		m_currSampler = smp;
	}
}

void PrimitiveBatch::setSampler(SamplerState smp)
{
	setSampler(smp == SamplerState_Linear ? m_samplerLiner : m_samplerPoint);
}

void PrimitiveBatch::setPrimitive(GfxPrimitive prim)
{
	RUSH_ASSERT(prim == GfxPrimitive::LineList || prim == GfxPrimitive::TriangleList);

	if (m_currPrim != prim)
	{
		flush();
		m_currPrim = prim;
	}
}

PrimitiveBatch::BatchVertex* PrimitiveBatch::drawVertices(GfxPrimitive primType, u32 vertexCount)
{
	setPrimitive(primType);

	RUSH_ASSERT(m_mode != BatchMode_Invalid);
	RUSH_ASSERT(vertexCount <= m_maxBatchVertices);

	if (m_vertices.size() + vertexCount >= m_maxBatchVertices)
	{
		flush();
	}

	return m_vertices.allocateUnsafe(vertexCount);
}
}
