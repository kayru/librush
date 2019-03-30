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
	void setTexture(GfxTexture tex);
	void setSampler(GfxSampler smp);
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
	GfxTechnique getNextTechnique();

	GfxContext* m_context;

	u32       m_maxBatchVertices;
	BatchMode m_mode;

	ColorRGBA m_currColor;

	GfxPrimitive m_currPrim;
	GfxTexture   m_currTexture;
	GfxSampler   m_currSampler;

	GfxSampler m_samplerLiner;
	GfxSampler m_samplerPoint;

	GfxBuffer                    m_vertexBuffer;
	LinearAllocator<BatchVertex> m_vertices;

	GfxVertexFormat m_vertexFormat2D;
	GfxVertexFormat m_vertexFormat3D;

	struct Constants
	{
		Mat4 viewProjMatrix;
		Vec4 transform2D; // xy: scale, zw: bias
		Vec4 color;
	};
	Constants m_constants;
	GfxBuffer m_constantBuffer;
	bool      m_constantBufferDirty;

	GfxVertexShader m_vertexShader2D;
	GfxVertexShader m_vertexShader3D;

	GfxPixelShader m_pixelShaderPlain;
	GfxPixelShader m_pixelShaderTextured;

	enum TechniqueID
	{
		TechniqueID_Plain2D,
		TechniqueID_Plain3D,
		TechniqueID_Textured2D,
		TechniqueID_Textured3D,
		TechniqueID_COUNT
	};

	GfxTechnique m_techniques[TechniqueID_COUNT];

	float m_depth;
};
}
