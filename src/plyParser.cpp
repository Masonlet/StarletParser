#include "StarletParsers/parser.hpp"
#include "StarletParsers/utils/log.hpp"

#include "StarletGraphics/mesh/mesh.hpp"
#include "StarletMath/constants.hpp"

#include <string>

bool Parser::parsePlyMesh(const std::string& path, MeshCPU& drawInfo) {
	std::vector<unsigned char> file;
	if (!loadBinaryFile(file, path))
		return false;

	if (file.empty()) return error("PlyParser", "parsePlyMesh", "Input pointer is null\n");

	const unsigned char* p = file.data();
	std::string errorMsg;
	while (true) {
		if (!parsePlyHeader(p, drawInfo.numVertices, drawInfo.numTriangles, drawInfo.hasNormals, drawInfo.hasColours, drawInfo.hasTexCoords)) {
			errorMsg = "header or missing 'end_header'";
			break;
		}

		if (drawInfo.numVertices == 0 || drawInfo.numTriangles == 0) {
			errorMsg = "header, no vertices/triangles declared";
			break;
		}

		drawInfo.vertices.assign(drawInfo.numVertices, Vertex{});
		if (!parseVertices(p, drawInfo)) {
			errorMsg = "vertex data";
			break;
		}

		drawInfo.numIndices = drawInfo.numTriangles * 3;
		drawInfo.indices.assign(drawInfo.numIndices, 0u);
		if (!parseIndices(p, drawInfo)) {
			errorMsg = "face data";
			break;
		}

		return true;
	}

	drawInfo.indices.clear();
	drawInfo.vertices.clear();
	drawInfo.numVertices = drawInfo.numIndices = drawInfo.numTriangles = 0;
	return error("PlyParser", "LoadModelFromFile", ("Failed to parse " + errorMsg + '\n').c_str());
}

bool Parser::parsePlyHeader(const unsigned char*& p, unsigned int& numVerticesOut, unsigned int& numTrianglesOut, bool& hasNormalsOut, bool& hasColoursOut, bool& hasTexCoordsOut) {
	if (!p) return error("PlyParser", "parsePlyHeader", "Input pointer is null\n");
	p = skipWhitespace(p);

	bool hasNx = false, hasNy = false, hasNz = false;
	bool hasRed = false, hasGreen = false, hasBlue = false;
	bool hasU = false, hasV = false;
	while (*p) {
		const unsigned char* nextLine = skipToNextLine(p);
		const unsigned char* lineEnd = trimEOL(p, nextLine);

		if (lineEnd == p) {
			p = nextLine;
			continue;
		}

		if (strncmp((const char*)p, "element", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
			if (!parsePlyElementLine(p, numVerticesOut, numTrianglesOut))
				return false;
		}
		else if (strncmp((const char*)p, "property", 8) == 0) {
			if (!parsePlyPropertyLine(p, hasNx, hasNy, hasNz, hasRed, hasGreen, hasBlue, hasU, hasV))
				return false;
		}
		else if (strncmp((const char*)p, "end_header", 10) == 0) {
			hasNormalsOut = hasNx && hasNy && hasNz;
			hasColoursOut = hasRed && hasGreen && hasBlue;
			hasTexCoordsOut = hasU && hasV;
			p = nextLine;
			return true;
		}
		else if (!(strncmp((const char*)p, "ply", 3) == 0)
			&& !(strncmp((const char*)p, "format", 6) == 0)
			&& !(strncmp((const char*)p, "comment", 7) == 0))
			debugLog("parsePlyHeader", "Unknown line in PLY header: %.*s\n" + static_cast<int>(lineEnd - p), (const char*)p);

		p = nextLine;
	}

	return error("plyParser", "parsePlyHeader", "Failed, end of buffer reached");
}

bool Parser::parsePlyElementLine(const unsigned char*& p, unsigned int& verticesOut, unsigned int& trianglesOut) {
	if (!p) return error("PlyParser", "parsePlyHeader", "Input pointer is null\n");

	p = skipWhitespace(p += 7);
	if (strncmp((const char*)p, "vertex", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
		p = skipWhitespace(p += 6);
		return parseUInt(p, verticesOut);
	}
	else if (strncmp((const char*)p, "face", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
		p = skipWhitespace(p += 4);
		return parseUInt(p, trianglesOut);
	}
	return false;
}
bool Parser::parsePlyPropertyLine(const unsigned char*& p, bool& hasNx, bool& hasNy, bool& hasNz, bool& hasR, bool& hasG, bool& hasB, bool& hasU, bool& hasV) {
	if (!p) return error("PlyParser", "parsePlyPropertyLine", "Input pointer is null\n");
	p = skipWhitespace(p += 8);

	char type[32]{};
	if (!parseToken(p, (unsigned char*)type, sizeof(type)))
		return error("PlyParser", "parsePlyPropertyLine", "Failed to parse property type :" + std::string(type));

	if (strcmp(type, "list") == 0) {
		/*
		 1 = Count Type
		 2 = Value Type
		 3 = Property Name
		*/
		char property[3][32]{};
		for (int i = 0; i < 3; ++i)
			if (!parseToken(p, reinterpret_cast<unsigned char*>(property[i]), sizeof(property[i])))
				return error("PlyParser", "parsePlyPropertyLine", "Failed to parse property list type, number: " + std::to_string(i));

		return true;
	}

	char propertyName[32]{};
	if (!parseToken(p, (unsigned char*)propertyName, sizeof(propertyName)))
		return error("PlyParser", "parsePlyPropertyLine", "Failed to parse property name :" + std::string(propertyName));

	if (strcmp(propertyName, "nx") == 0 || strcmp(propertyName, "normal_x") == 0) hasNx = true;
	else if (strcmp(propertyName, "ny") == 0 || strcmp(propertyName, "normal_y") == 0) hasNy = true;
	else if (strcmp(propertyName, "nz") == 0 || strcmp(propertyName, "normal_z") == 0) hasNz = true;
	else if (strcmp(propertyName, "red") == 0) hasR = true;
	else if (strcmp(propertyName, "green") == 0) hasG = true;
	else if (strcmp(propertyName, "blue") == 0) hasB = true;
	else if (strcmp(propertyName, "u") == 0 || strcmp(propertyName, "texture_u") == 0) hasU = true;
	else if (strcmp(propertyName, "v") == 0 || strcmp(propertyName, "texture_v") == 0) hasV = true;
	return true;
}

bool Parser::parseVertices(const unsigned char*& p, MeshCPU& drawInfo) {
	if (!p) return error("PlyParser", "parseVertices", "Input pointer is null\n");
	if (!drawInfo.numVertices) return error("PlyParser", "parseVertices", "No vertices declared in header\n");

	float minY = FLT_MAX, maxY = -FLT_MAX;
	unsigned int i = 0;
	while (i < drawInfo.numVertices && *p) {
		Vertex& v = drawInfo.vertices[i];
		const unsigned char* nextLine = skipToNextLine(p);
		const unsigned char* lineEnd = trimEOL(p, nextLine);

		if (lineEnd == p) {
			p = nextLine;
			continue;
		}

		if (*p == '\0') return false;

		bool valid = true;
		while (valid) {
			PARSE_OR(valid = false, parseFloat, v.pos.x, "Failed to parse position X");
			PARSE_OR(valid = false, parseFloat, v.pos.y, "Failed to parse position Y");
			PARSE_OR(valid = false, parseFloat, v.pos.z, "Failed to parse position Z");
			break;
		}
		if (!valid) {
			p = nextLine;
			continue;
		}

		if (drawInfo.hasNormals) {
			while (valid) {
				PARSE_OR(valid = false, parseFloat, v.norm.x, "Failed to parse normal X");
				PARSE_OR(valid = false, parseFloat, v.norm.y, "Failed to parse normal Y");
				PARSE_OR(valid = false, parseFloat, v.norm.z, "Failed to parse normal Z");
				break;
			}

			if (!valid) {
				p = nextLine;
				continue;
			}
		}

		if (drawInfo.hasColours) {
			if (*p != '\0') {
				Vec3 colour = { 1.0f, 1.0f, 1.0f };
				const unsigned char* original = p;
				while (valid) {
					PARSE_OR(valid = false, parseFloat, colour.r, "Failed to parse float colour R");
					PARSE_OR(valid = false, parseFloat, colour.g, "Failed to parse float colour G");
					PARSE_OR(valid = false, parseFloat, colour.b, "Failed to parse float colour B");
					break;
				}

				if (valid &&
					colour.x >= 0.0f && colour.x <= 1.0f &&
					colour.y >= 0.0f && colour.y <= 1.0f &&
					colour.z >= 0.0f && colour.z <= 1.0f) {
					v.col = Vec4{ colour.x, colour.y, colour.z, 1.0f };
					drawInfo.hasColours = true;
				}
				else {
					p = original;
					unsigned int ri = 0, gi = 0, bi = 0, ai = 256;

					valid = true;
					while (valid) {
						PARSE_OR(valid = false, parseUInt, ri, "");
						PARSE_OR(valid = false, parseUInt, gi, "");
						PARSE_OR(valid = false, parseUInt, bi, "");
						break;
					}
					if (!parseUInt(p, ai)) ai = 255;

					if (valid && ri <= 255 && gi <= 255 && bi <= 255) {
						v.col = Vec4{
								static_cast<float>(ri) / 255.0f,
								static_cast<float>(gi) / 255.0f,
								static_cast<float>(bi) / 255.0f,
								static_cast<float>(ai) / 255.0f
						};
					}
				}
			}
		}

		if (drawInfo.hasTexCoords) {
			while (valid) {
				PARSE_OR(valid = false, parseFloat, v.texCoord.x, "Failed to parse texcoord U");
				PARSE_OR(valid = false, parseFloat, v.texCoord.y, "Failed to parse texcoord V");
				break;
			}
		}

		if (v.pos.y < minY) minY = v.pos.y;
		if (v.pos.y > maxY) maxY = v.pos.y;

		++i;
		p = nextLine;
	}

	drawInfo.minY = minY;
	drawInfo.maxY = maxY;
	return true;
}
bool Parser::parseIndices(const unsigned char*& p, MeshCPU& drawInfo) {
	if (!p) return error("PlyParser", "parseIndices", "Input pointer is null");
	if (drawInfo.indices.empty() || drawInfo.numIndices == 0) return error("PlyParser", "parseIndices", "Index buffer not allocated");

	unsigned int triangleIndex = 0;
	while (triangleIndex < drawInfo.numTriangles && *p) {
		const unsigned char* nextLine = skipToNextLine(p);
		const unsigned char* lineEnd = trimEOL(p, nextLine);

		if (lineEnd == p) {
			p = nextLine;
			continue;
		}

		unsigned int count = 0;
		if (!parseUInt(p, count)) {
			p = nextLine;
			continue;
		}

		if (count != 3) {
			p = nextLine;
			continue;
		}

		unsigned int i0{ 0 }, i1{ 0 }, i2{ 0 };
		bool valid = true;
		while (valid) {
			PARSE_OR(valid = false, parseUInt, i0, "Failed to parse indice 1");
			PARSE_OR(valid = false, parseUInt, i1, "Failed to parse indice 2");
			PARSE_OR(valid = false, parseUInt, i2, "Failed to parse indice 3");
			break;
		}

		if (valid) {
			unsigned int base = triangleIndex * 3;
			drawInfo.indices[base + 0] = i0;
			drawInfo.indices[base + 1] = i1;
			drawInfo.indices[base + 2] = i2;
			++triangleIndex;
		}

		p = nextLine;
	}

	return true;
}