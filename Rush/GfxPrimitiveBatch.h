#pragma once

#include "GfxDevice.h"
#include "MathTypes.h"
#include "UtilColor.h"
#include "UtilLinearAllocator.h"
#include "UtilArray.h"
#include "UtilTuple.h"


namespace Rush
{
struct TexturedQuad2D
{
	Vec2 pos[4];
	Vec2 tex[4];
};

struct TexturedQuad3D
{
	Vec3 pos[4];
	Vec2 tex[4];
};

class PrimitiveBatch
{
public:
	enum SamplerState
	{
		SamplerState_Point,
		SamplerState_Linear,

		SamplerState_COUNT
	};

	enum BatchMode
	{
		BatchMode_Invalid,
		BatchMode_2D,
		BatchMode_3D,

		BatchMode_COUNT
	};

	struct BatchVertex
	{
		Vec3       pos;
		Vec2       tex;
		ColorRGBA8 col;
	};

public:
	PrimitiveBatch(u32 maxBatchVertices = 12000);
	~PrimitiveBatch();

	void createPipelines(const GfxRenderTargetDesc& renderTargetDesc);

	Vec4        getTransform2D() const { return m_constants.transform2D; }
	const Mat4& getViewProjMatrix() const { return m_constants.viewProjMatrix; }

	void begin2D(const Vec2& scale, const Vec2& bias);
	void begin2D(float width, float height);
	void begin2D(const Tuple2i& size) { begin2D((float)size.x, (float)size.y); }
	void begin2D(const Tuple2u& size) { begin2D((float)size.x, (float)size.y); }
	void begin2D(const Tuple2f& size) { begin2D(size.x, size.y); }
	void begin2D(const Vec2& size) { begin2D(size.x, size.y); }
	void begin2D(const Box2& bounds);
	void end2D();

	void begin3D(const Mat4& viewProjMatrix);
	void end3D();

	void setColor(ColorRGBA color);
	void setTexture(GfxTextureArg tex);
	void setSampler(GfxSamplerArg smp);
	void setSampler(SamplerState smp);
	void setPrimitive(GfxPrimitive prim);

	void drawLine(float ax, float ay, float bx, float by, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(
	    float ax, float ay, float az, float bx, float by, float bz, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(const Vec2& a, const Vec2& b, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(const Vec3& a, const Vec3& b, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(const Line2& line, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(const Line2& line, const ColorRGBA8& colorStart, const ColorRGBA8& colorEnd);
	void drawLine(const Line3& line, const ColorRGBA8& color = ColorRGBA8::White());
	void drawLine(const Line3& line, const ColorRGBA8& colorStart, const ColorRGBA8& colorEnd);
	void drawRect(const Box2& rect, const ColorRGBA8& color = ColorRGBA8::White());

	void drawTriangle(const Vec2& a, const Vec2& b, const Vec2& c, const ColorRGBA8& color);
	void drawTriangle(const Vec2& pa, const Vec2& pb, const Vec2& pc, const ColorRGBA8& ca, const ColorRGBA8& cb,
	    const ColorRGBA8& cc);

	void drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const ColorRGBA8& color);
	void drawTriangle(const Vec3& pa, const Vec3& pb, const Vec3& pc, const ColorRGBA8& ca, const ColorRGBA8& cb,
	    const ColorRGBA8& cc);

	void drawTexturedQuad(const Box2& rect, ColorRGBA8 color = ColorRGBA8::White());
	void drawTexturedQuad(const TexturedQuad2D* q, ColorRGBA8 color = ColorRGBA8::White());
	void drawTexturedQuad(const TexturedQuad3D* q, ColorRGBA8 color = ColorRGBA8::White());

	void drawTexturedQuad(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d,
	    ColorRGBA8 color = ColorRGBA8::White(), const Vec2& ta = Vec2(0, 0), const Vec2& tb = Vec2(1, 0),
	    const Vec2& tc = Vec2(1, 1), const Vec2& td = Vec2(0, 1));

	void drawTexturedQuad(const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pd,
	    ColorRGBA8 color = ColorRGBA8::White(), const Vec2& ta = Vec2(0, 0), const Vec2& tb = Vec2(1, 0),
	    const Vec2& tc = Vec2(1, 1), const Vec2& td = Vec2(0, 1));

	void drawCross(const Vec3& pos, float size, const ColorRGBA8& color = ColorRGBA8::White());
	void drawBox(const Box3& box, const ColorRGBA8& color = ColorRGBA8::White());

	BatchVertex* drawVertices(GfxPrimitive primType, u32 vertexCount);

	void flush();

	inline u32 getMaxBatchVertices() const { return m_maxBatchVertices; }

private:

	static constexpr u32 PipelineBit_Is3D     = 1 << 0;
	static constexpr u32 PipelineBit_Lines    = 1 << 1;
	static constexpr u32 PipelineBit_Textured = 1 << 2;
	static constexpr u32 PipelineID_COUNT     = 1 << 3;

	enum class PipelineID : u32
	{
		PlainTriangles2D    = 0,
		PlainTriangles3D    = PipelineBit_Is3D,
		PlainLines2D        = PipelineBit_Lines,
		PlainLines3D        = PipelineBit_Lines | PipelineBit_Is3D,
		TexturedTriangles2D = PipelineBit_Textured,
		TexturedTriangles3D = PipelineBit_Textured | PipelineBit_Is3D,
		TexturedLines2D     = PipelineBit_Textured | PipelineBit_Lines,
		TexturedLines3D     = PipelineBit_Textured | PipelineBit_Lines | PipelineBit_Is3D,
	};

	struct PipelineSet
	{
		GfxRenderTargetDesc renderTarget;
		GfxOwn<GfxRenderPipeline> pipelines[PipelineID_COUNT];
	};

	GfxRenderPipelineDesc makePipelineDesc(PipelineID id, const GfxRenderTargetDesc& renderTargetDesc) const;
	PipelineSet&      findOrAddPipelineSet(const GfxRenderTargetDesc& renderTargetDesc);
	GfxRenderPipeline getOrCreatePipeline(const GfxRenderTargetDesc& renderTargetDesc, PipelineID id);
	GfxRenderPipeline getNextPipeline();

	GfxContext* m_context;

	u32       m_maxBatchVertices;
	BatchMode m_mode;

	ColorRGBA m_currColor;

	GfxPrimitive m_currPrim;

	GfxTexture m_currTexture;
	GfxSampler m_currSampler;

	GfxOwn<GfxSampler> m_samplerLiner;
	GfxOwn<GfxSampler> m_samplerPoint;

	GfxOwn<GfxBuffer>            m_vertexBuffer;
	LinearAllocator<BatchVertex> m_vertices;

	GfxVertexFormatDesc m_vertexFormat2D;
	GfxVertexFormatDesc m_vertexFormat3D;

	struct Constants
	{
		Mat4 viewProjMatrix;
		Vec4 transform2D; // xy: scale, zw: bias
		Vec4 color;
	};
	Constants         m_constants;
	GfxOwn<GfxBuffer> m_constantBuffer;
	bool              m_constantBufferDirty;

	GfxOwn<GfxVertexShader> m_vertexShader2D;
	GfxOwn<GfxVertexShader> m_vertexShader3D;

	GfxOwn<GfxPixelShader> m_pixelShaderPlain;
	GfxOwn<GfxPixelShader> m_pixelShaderTextured;

	GfxShaderBindingDesc m_plainBindings;
	GfxShaderBindingDesc m_texturedBindings;

	DynamicArray<PipelineSet> m_pipelineCache;

	float m_depth;
};
}
