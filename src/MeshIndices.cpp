#include "externals.h"
#include "MeshIndices.h"
#include "MayaException.h"
#include "spans.h"
#include "dump.h"

MeshIndices::MeshIndices(const MeshSemantics& semantics, const MFnMesh& fnMesh)
{
	MStatus status;
	MObjectArray shaders;
	MIntArray mapPolygonToShader;

	// TODO: Deal with instances?
	const auto instanceNumber = 0;
	THROW_ON_FAILURE(fnMesh.getConnectedShaders(instanceNumber, shaders, mapPolygonToShader));

	const auto shaderCount = shaders.length();
	m_isShaderUsed.resize(shaderCount, false);

	const auto numPolygons = fnMesh.numPolygons();

	m_primitiveToShaderIndexMap.reserve(numPolygons * 2);

	// Reserve space for the indices, we assume every polygon is a quad.
	for (auto kind = 0; kind<Semantic::COUNT; ++kind)
	{
		auto& indexSet = m_indexSets.at(kind);
		const auto n = semantics.at(Semantic::from(kind)).size();
		for (auto set = 0; set<n; ++set)
		{
			indexSet[set].reserve(numPolygons * 6);
		}
	}

	auto& positions = m_indexSets.at(Semantic::POSITION).at(0);
	auto& normals = m_indexSets.at(Semantic::NORMAL).at(0);
	auto& uvSets = m_indexSets.at(Semantic::TEXCOORD);
	auto& colorSets = m_indexSets.at(Semantic::COLOR);

	auto& colorSemantics = semantics.at(Semantic::COLOR);
	auto& uvSetSemantics = semantics.at(Semantic::TEXCOORD);

	const auto colorSetCount = colorSemantics.size();
	const auto uvSetCount = uvSetSemantics.size();

	auto polygonIndex = 0;
	for (MItMeshPolygon itPoly(fnMesh.object()); !itPoly.isDone(); itPoly.next(), ++polygonIndex)
	{
		auto numTrianglesInPolygon = 0;
		THROW_ON_FAILURE(itPoly.numTriangles(numTrianglesInPolygon));

		// NOTE: This can be negative when no shader is attached (e.g. blend shapes)
		int shaderIndex = mapPolygonToShader[polygonIndex];
		if (shaderIndex >= 0)
		{
			m_isShaderUsed[shaderIndex] = true;
		}

		for (auto localTriangleIndex = 0; localTriangleIndex < numTrianglesInPolygon; ++localTriangleIndex)
		{
			m_primitiveToShaderIndexMap.push_back(shaderIndex);

			// TODO: Use Maya's triangulation here, and figure out how to look up the normal properly
			for (auto i = 0; i<3; ++i)
			{
				const auto localVertexIndex = i == 0 ? 0 : i + localTriangleIndex;

				int positionIndex = itPoly.vertexIndex(localVertexIndex, &status);
				THROW_ON_FAILURE(status);
				positions.push_back(positionIndex);

				int normalIndex = itPoly.normalIndex(localVertexIndex, &status);
				THROW_ON_FAILURE(status);
				normals.push_back(normalIndex);

				for (auto setIndex=0; setIndex<colorSetCount; ++setIndex)
				{
					int colorIndex;
					auto colorSetName = colorSemantics[setIndex].setName;
					status = itPoly.getColorIndex(localVertexIndex, colorIndex, &colorSetName);
					THROW_ON_FAILURE(status);
					colorSets.at(setIndex).push_back(colorIndex);
				}

				for (auto setIndex = 0; setIndex<uvSetCount; ++setIndex)
				{
					int uvIndex;
					auto uvSetName = uvSetSemantics[setIndex].setName;
					status = itPoly.getUVIndex(localVertexIndex, uvIndex, &uvSetName);
					THROW_ON_FAILURE(status);
					uvSets.at(setIndex).push_back(uvIndex);
				}
			}
		}
	}
}

MeshIndices::~MeshIndices()
{
}

void MeshIndices::dump(const std::string& name, const std::string& indent) const
{
	auto& positions = m_indexSets[Semantic::POSITION].at(0);
	auto& normals = m_indexSets[Semantic::NORMAL].at(0);
	auto& uvSets = m_indexSets[Semantic::TEXCOORD];
	auto& colorSets = m_indexSets[Semantic::COLOR];

	cout << indent << std::quoted(name) << ": {" << endl;
	const auto subIndent = indent + "\t";
	
	dump_span<float>("POSITION", span(positions), subIndent);
	cout << "," << endl;
	
	dump_span<float>("NORMAL", span(normals), subIndent);
	cout << "," << endl;
	
	for (const auto& pair : uvSets)
		dump_span<float>("TEXCOORD_" + std::to_string(pair.first), span(pair.second), subIndent);

	if (!uvSets.empty())
		cout << "," << endl;

	for (const auto& pair : colorSets)
		dump_span<float>("COLOR_" + std::to_string(pair.first), span(pair.second), subIndent);

	if (!colorSets.empty())
		cout << endl;

	cout << indent << "}";
}
