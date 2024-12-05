#pragma once

#include <variant>
#include <vector>
#include <string>
#include <string_view>

#include "fae/math/math.h"
#include "fae/gfx/base/text.h"

// high-level to low-level interface

namespace Prim {

struct Color {
  float r, g, b, a;

  static const Color kTransparent;
  static const Color kBlack;
  static const Color kWhite;
};

inline const Color Color::kTransparent = Color { 0, 0, 0, 0 };
inline const Color Color::kBlack = Color { 0, 0, 0, 1 };
inline const Color Color::kWhite = Color { 1, 1, 1, 1 };

// NOTE: the coordinate system used by the primitives is X=right, Y=up
// and they assume to be drawn in a viewport that's 1x1 units
struct Primitive {
  M44 fTransform = m44_identity();

  void SetPos(float x, float y)
  {
    fTransform.col[3].x = x;
    fTransform.col[3].y = y;
  }

  float X() const { return fTransform.col[3].x; }
  float Y() const { return fTransform.col[3].y; }
};

struct Quad : Primitive {
  float fWidth, fHeight;

  static Quad WithSize(float width, float height)
  {
    Quad q = {};
    q.fWidth = width;
    q.fHeight = height;
    return q;    
  }

  static Quad MinMax(float minX, float minY, float maxX, float maxY)
  {
    Quad q;
    q.fTransform.col[3].x = (minX + maxX) * 0.5;
    q.fTransform.col[3].y = (minY + maxY) * 0.5;
    q.fWidth = maxX - minX;
    q.fHeight = maxY - minY;
    return q;  
  }

  V2 Center() const    { return v2(fTransform.col[3].x, fTransform.col[3].y); }
  float Left() const   { return X() - fWidth * 0.5f; }
  float Right() const  { return X() + fWidth * 0.5f; }
  float Bottom() const { return Y() - fHeight * 0.5f; }
  float Top()    const { return Y() + fHeight * 0.5f; }
};

struct Text : Primitive {
  std::string fString;
  Char_Size fCharSize;
};

struct PaintProps {
  Color fBorderColor;
  Color fBgColor;
  float fBorderThickness;
};

using Geom = std::variant<Quad, Text>;
}

struct RVulkanPrimitive {
  Prim::Geom fGeom;
  Prim::PaintProps fPaintProps;
};

class RVulkanPrimitiveBuilder {
  RVulkanPrimitive fPrim;

  RVulkanPrimitiveBuilder() = default;
  
public:
  template <typename T>
  explicit RVulkanPrimitiveBuilder(T geom) {
    fPrim.fGeom = geom;
  }

  static RVulkanPrimitiveBuilder Quad(float w, float h) {
    RVulkanPrimitiveBuilder bld = {};
    bld.fPrim.fGeom = Prim::Quad::WithSize(w, h);
    return bld;
  }

  static RVulkanPrimitiveBuilder Text(std::string_view str, Char_Size charSize) {
    RVulkanPrimitiveBuilder bld = {};
    Prim::Text t {};
    t.fString = str;
    t.fCharSize = charSize;
    bld.fPrim.fGeom = t;
    return bld;
  }

  RVulkanPrimitiveBuilder &BorderColor(Prim::Color col)
  {
    fPrim.fPaintProps.fBorderColor = col;
    return *this;
  }

  RVulkanPrimitiveBuilder &BorderThickness(float thick)
  {
    fPrim.fPaintProps.fBorderThickness = thick;
    return *this;
  }

  RVulkanPrimitiveBuilder &BackgroundColor(Prim::Color col)
  {
    fPrim.fPaintProps.fBgColor = col;
    return *this;
  }

  RVulkanPrimitiveBuilder &Position(float x, float y)
  {
    std::visit([x, y](auto &p) { p.SetPos(x, y); }, fPrim.fGeom);
    return *this;
  }

  RVulkanPrimitive Build()
  {
    return fPrim;
  }
};

using RVulkanPrimitiveArray = std::vector<RVulkanPrimitive>;
