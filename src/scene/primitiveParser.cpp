#pragma once

#include "starletparsers/scene/primitive.hpp"
#include "starletparsers/scene/primitiveParser.hpp"
#include "starletparsers/common/parserUtils.hpp"

bool parseTriangle(const unsigned char*& p, Primitive& out) {
  PARSE_STRING_OR(return false, p, out.name, 64, "triangle name");
  Vec3 temp;
  PARSE_OR(return false, parseVec3, temp, "triangle position");
  out.transform.pos = { temp, 1.0f };
  PARSE_OR(return false, parseVec3, out.transform.rot, "triangle rotation");
  PARSE_OR(return false, parseVec3, out.transform.size, "triangle size");
  return parseColour(p, out.colour, out.colourMode);
}