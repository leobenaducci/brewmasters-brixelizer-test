#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Mesh.hpp"
#include "Texture.hpp"

using Microsoft::WRL::ComPtr;

class Model {
public:
	void Load(ID3D12Device*              device,
			  ID3D12GraphicsCommandList* cmdList,
			  ID3D12DescriptorHeap*      srvHeap,
			  const std::string&         path)
	{
		Assimp::Importer importer;

		aiScene const* scene = importer.ReadFile(path,
			aiProcess_Triangulate       |
			aiProcess_ConvertToLeftHanded |
			aiProcess_GenNormals        |
			aiProcess_CalcTangentSpace);

		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
			std::cerr << "[Model] Assimp Error: " << importer.GetErrorString() << '\n';
			return;
		}

		m_Directory = path.substr(0, path.find_last_of('/'));

		std::map<unsigned int, int> materialToSrvIndex;
		LoadMaterials(device, cmdList, srvHeap, scene, materialToSrvIndex);
		ProcessNode(device, scene->mRootNode, scene, materialToSrvIndex);

		std::cout << "[Model] Loaded model: " << path << '\n';
	}

	void ClearUploadBuffers() { m_UploadBuffers.clear(); }

	void Draw(
		ID3D12GraphicsCommandList* const cmdList,
		ID3D12DescriptorHeap* const srvHeap,
		UINT const descriptorSize,
		UINT albedoRootParamSlot = 1
	) const {
		for (auto const& mesh : m_Meshes) {
			mesh->Draw(cmdList, srvHeap, descriptorSize);
		}
	}

	std::size_t MeshCount()    const noexcept { return m_Meshes.size(); }
	std::size_t TextureCount() const noexcept { return m_Textures.size(); }
	std::vector<std::unique_ptr<Mesh>> const& GetMeshes() const noexcept { return m_Meshes; }

private:
	void LoadMaterials(ID3D12Device*                device,
					   ID3D12GraphicsCommandList*   cmdList,
					   ID3D12DescriptorHeap*        srvHeap,
					   aiScene const*               scene,
					   std::map<unsigned int, int>& outMap)
	{
		for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
			aiString texPath;

			if (scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) != AI_SUCCESS) {
				outMap[i] = -1;
				continue;
			}

			std::string filename = texPath.C_Str();
			std::replace(filename.begin(), filename.end(), '\\', '/');
			const std::string fullPath = m_Directory + "/" + filename;

			ComPtr<ID3D12Resource> uploadBuffer;
			ID3D12Resource* texRes = Texture::CreateTexture(device, cmdList, fullPath, uploadBuffer);

			if (texRes) {
				const int srvIndex = static_cast<int>(m_Textures.size());
				Texture::CreateSrv(device, srvHeap, texRes, srvIndex);

				m_Textures.push_back(texRes);
				m_UploadBuffers.push_back(std::move(uploadBuffer));
				outMap[i] = srvIndex;
			} else {
				outMap[i] = -1;
			}
		}
	}

	void ProcessNode(ID3D12Device*                      device,
					 aiNode const*                      node,
					 aiScene const*                     scene,
					 std::map<unsigned int, int> const& matMap)
	{
		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
			m_Meshes.push_back(BuildMesh(device, scene->mMeshes[node->mMeshes[i]], matMap));

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
			ProcessNode(device, node->mChildren[i], scene, matMap);
	}

	std::unique_ptr<Mesh> BuildMesh(ID3D12Device*                      device,
									aiMesh const*                      aiMesh,
									std::map<unsigned int, int> const& matMap)
	{
		// Vertices.
		std::vector<Vertex> vertices;
		vertices.reserve(aiMesh->mNumVertices);

		for (unsigned int i = 0; i < aiMesh->mNumVertices; ++i) {
			Vertex v{};
			v.position[0] = aiMesh->mVertices[i].x;
			v.position[1] = aiMesh->mVertices[i].y;
			v.position[2] = aiMesh->mVertices[i].z;

			if (aiMesh->HasNormals()) {
				v.normal[0] = aiMesh->mNormals[i].x;
				v.normal[1] = aiMesh->mNormals[i].y;
				v.normal[2] = aiMesh->mNormals[i].z;
			}

			if (aiMesh->mTextureCoords[0]) {
				v.texCoord[0] = aiMesh->mTextureCoords[0][i].x;
				v.texCoord[1] = aiMesh->mTextureCoords[0][i].y;
			}

			vertices.push_back(v);
		}

		// Indices.
		std::vector<uint32_t> indices;
		indices.reserve(aiMesh->mNumFaces * 3);

		for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i) {
			aiFace const& face = aiMesh->mFaces[i];
			assert(face.mNumIndices == 3);
			indices.push_back(face.mIndices[0]);
			indices.push_back(face.mIndices[1]);
			indices.push_back(face.mIndices[2]);
		}

		int const texIdx = matMap.count(aiMesh->mMaterialIndex)
						 ? matMap.at(aiMesh->mMaterialIndex)
						 : -1;

		return std::make_unique<Mesh>(device, vertices, indices, texIdx);
	}

	std::string m_Directory {};

	std::vector<std::unique_ptr<Mesh>>   m_Meshes {};
	std::vector<ComPtr<ID3D12Resource>>  m_Textures {};
	std::vector<ComPtr<ID3D12Resource>>  m_UploadBuffers {};
};