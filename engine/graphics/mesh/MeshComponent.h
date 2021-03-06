#ifndef _CE_MESH_COMPONENT_H_
#define _CE_MESH_COMPONENT_H_

#include <vector>

typedef unsigned int GLuint;

namespace CE
{
	struct Mesh;
	typedef std::vector<Mesh> Meshes;
	struct Texture;
	typedef std::vector<Texture> Textures;

	class MeshComponent
	{
	public:
		MeshComponent(Meshes* meshes, Textures* textures);

		void Draw(
			GLuint g_vbo,
			GLuint g_ibo,
			GLuint g_diffuseTextureID,
			GLuint g_diffuseTextureLocation,
			GLuint g_diffuseTextureUnit);

	private:
		void DrawMesh(
			const Mesh& mesh,
			GLuint g_vbo,
			GLuint g_ibo,
			GLuint g_diffuseTextureID,
			GLuint g_diffuseTextureLocation,
			GLuint g_diffuseTextureUnit);

	private:
		Meshes* m_meshes;
		// TODO: Remove.
		Textures* m_textures;
	};
}

#endif // _CE_MESH_COMPONENT_H_