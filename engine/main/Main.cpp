#include <SDL.h>
#include <SDL_syswm.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "graphics/animation/AnimationComponent.h"
#include "graphics/animation/AnimationManager.h"
#include "graphics/mesh/Mesh.h"
#include "graphics/mesh/Vertex.h"
#include "graphics/mesh/MeshManager.h"
#include "graphics/mesh/MeshComponent.h"
#include "graphics/skeleton/Skeleton.h"
#include "graphics/skeleton/SkeletonManager.h"
#include "graphics/texture/TextureManager.h"
#include "graphics/texture/Texture.h"

#include "graphics/ceasset/input/AssetImporter.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "core/Engine.h"
#include "core/FpsCounter.h"
#include "common/debug/AssertThread.h"
#include "core/clock/RealTimeClock.h"
#include "core/clock/GameTimeClock.h"
#include "event/ToggleBindPoseEvent.h"
#include "event/SetRenderModeEvent.h"
#include "core/Camera.h"
#include "event/SdlEvent.h"
#include "core/EditorCameraEventHandler.h"

#include "cef/CefMain.h"
#include "event/WindowsMessageEvent.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

SDL_Window* g_window = NULL;
SDL_GLContext g_context;

bool g_renderQuad = true;

GLuint g_skinnedMeshDiffuseTextureProgramId = 0;
GLuint g_vbo = 0;
GLuint g_ibo = 0;
GLuint g_vao = 0;
GLuint g_tbo = 0;

GLuint g_skeletonProgramId = 0;
GLuint g_uiProgramId = 0;
GLuint g_gridProgramId = 0;
GLuint g_skinnedMeshWireFrameDiffuseTextureProgramId = 0;

GLuint g_skinnedMeshDiffuseTextureProjectionViewModelMatrixId = -1;
GLuint g_skinnedMeshDiffuseTexturePaletteId = -1;
GLuint g_paletteTextureUnit = -1;
GLuint g_paletteGenTex = -1;
GLuint g_skinnedMeshDiffuseTextureDiffuseTextureId = -1;
GLuint g_diffuseTextureUnit = -1;
GLuint g_diffuseTextureID = -1;

GLuint g_skeletonProjectionViewModelMatrixId = -1;
GLuint g_skeletonPaletteId = -1;

GLuint g_skinnedMeshWireFrameDiffuseTextureProjectionViewModelMatrixId = -1;
GLuint g_skinnedMeshWireFrameDiffuseTexturePaletteId = -1;
GLuint g_skinnedMeshWireFrameDiffuseTextureDiffuseTextureId = -1;

GLuint g_gridProjectionViewModelMatrixId = -1;

GLuint g_uiTextureId = -1;
GLuint g_uiTextureUnit = -1;
GLuint g_uiTextureID = -1;

std::vector<const char*> g_assetNames = {
	// "assets/Quarterback Pass.ceasset",
	// "assets/Thriller Part 2.ceasset",
	// "assets/Standing Walk Forward.ceasset",
	"assets/Zombie Stand Up.ceasset"
};

CE::AssetImporter* g_assetImporter;

std::vector<CE::MeshComponent*> g_meshComponents;
std::vector<CE::AnimationComponent*> g_animationComponents;

CE::CefMain* cefMain;

EventSystem* eventSystem;
CE::Engine* engine;

CE::FpsCounter* g_fpsCounter;

CE::Camera* g_camera;

void PrintProgramLog(GLuint program)
{
	if (!glIsProgram(program))
	{
		printf("GLuint %d is not a program\n", program);
		return;
	}

	GLint maxLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

	char* infoLog = new char[maxLength];

	GLint infoLogLength = 0;
	glGetProgramInfoLog(program, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0)
	{
		printf("program log:\n");
		printf("%s\n", infoLog);
	}

	delete[] infoLog;
}

void PrintShaderLog(GLuint shader)
{
	if (!glIsShader(shader))
	{
		printf("GLuint %d is not a shader\n", shader);
		return;
	}

	GLint maxLength = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

	char* infoLog = new char[maxLength];

	GLint infoLogLength = 0;
	glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0)
	{
		printf("shader log:\n");
		printf("%s\n", infoLog);
	}

	delete[] infoLog;
}

void RenderMesh(CE::MeshComponent& meshComponent, CE::AnimationComponent& animationComponent, const glm::mat4& projectionViewModel)
{
	bool renderWireFrameOnly = engine->GetRenderMode() == 3;
	GLuint activeProgramID = -1;
	GLuint activeProjectionViewModelMatrixID = -1;
	GLuint activePaletteID = -1;
	GLuint activeDiffuseTextureLocation = -1;

	if (renderWireFrameOnly)
	{
		activeProgramID = g_skinnedMeshWireFrameDiffuseTextureProgramId;
		activeProjectionViewModelMatrixID = g_skinnedMeshWireFrameDiffuseTextureProjectionViewModelMatrixId;
		activePaletteID = g_skinnedMeshWireFrameDiffuseTexturePaletteId;
		activeDiffuseTextureLocation = g_skinnedMeshWireFrameDiffuseTextureDiffuseTextureId;
	}
	else
	{
		activeProgramID = g_skinnedMeshDiffuseTextureProgramId;
		activeProjectionViewModelMatrixID = g_skinnedMeshDiffuseTextureProjectionViewModelMatrixId;
		activePaletteID = g_skinnedMeshDiffuseTexturePaletteId;
		activeDiffuseTextureLocation = g_skinnedMeshDiffuseTextureDiffuseTextureId;
	}

	glUseProgram(activeProgramID);

	unsigned int stride = sizeof(CE::Vertex1P1UV4J);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(CE::Vertex1P1UV4J, position)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(CE::Vertex1P1UV4J, uv)));
	glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, stride, reinterpret_cast<void*>(offsetof(CE::Vertex1P1UV4J, jointIndices)));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(CE::Vertex1P1UV4J, jointWeights)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	glUniformMatrix4fv(activeProjectionViewModelMatrixID, 1, GL_FALSE, &projectionViewModel[0][0]);

	if (engine->IsRenderBindPose())
	{
		animationComponent.ResetMatrixPalette();
	}

	animationComponent.BindMatrixPalette(
		g_paletteTextureUnit,
		g_paletteGenTex,
		g_tbo,
		activePaletteID);

	if (engine->GetRenderMode() == 0 || engine->GetRenderMode() == 2 || renderWireFrameOnly)
	{
		meshComponent.Draw(
			g_vbo,
			g_ibo,
			g_diffuseTextureID,
			activeDiffuseTextureLocation,
			g_diffuseTextureUnit);
	}

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
}

void RenderSkeleton(CE::AnimationComponent& animationComponent, const glm::mat4& projectionViewModel)
{
	glUseProgram(g_skeletonProgramId);

	struct DebugSkeletonVertex
	{
		glm::vec3 position;
		glm::vec3 color;
		unsigned jointIndex;
	};

	unsigned stride = sizeof(DebugSkeletonVertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(DebugSkeletonVertex, position)));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(DebugSkeletonVertex, color)));
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, stride, reinterpret_cast<void*>(offsetof(DebugSkeletonVertex, jointIndex)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glUniformMatrix4fv(g_skeletonProjectionViewModelMatrixId, 1, GL_FALSE, &projectionViewModel[0][0]);

	if (engine->IsRenderBindPose())
	{
		animationComponent.ResetMatrixPalette();
	}

	animationComponent.BindMatrixPalette(
		g_paletteTextureUnit,
		g_paletteGenTex,
		g_tbo,
		g_skeletonPaletteId);

	const CE::Skeleton* skeleton = animationComponent.GetSkeleton();// CE::SkeletonManager::Get().GetSkeleton(g_assetNames[i]);

	std::vector<DebugSkeletonVertex> debugVertices;
	std::vector<unsigned> debugJointIndices;
	std::vector<unsigned> debugLineIndices;

	for (unsigned i = 0; i < skeleton->joints.size(); ++i)
	{
		glm::mat4 bindPose = glm::inverse(skeleton->joints[i].inverseBindPose);

		// todo: bad
		glm::vec3 scale;
		glm::quat orientation;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(
			bindPose,
			scale,
			orientation,
			translation,
			skew,
			perspective);

		DebugSkeletonVertex vertex;
		vertex.position = translation;
		vertex.color = glm::vec3(1.f, 0.f, 0.f);
		vertex.jointIndex = i;

		debugVertices.push_back(vertex);

		debugJointIndices.push_back(i);

		if (skeleton->joints[i].parentIndex != -1)
		{
			debugLineIndices.push_back(skeleton->joints[i].parentIndex);
			debugLineIndices.push_back(i);
		}
	}

	if (engine->GetRenderMode() == 1 || engine->GetRenderMode() == 2)
	{
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(DebugSkeletonVertex) * debugVertices.size(), debugVertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * debugJointIndices.size(), debugJointIndices.data(), GL_STATIC_DRAW);

		glPointSize(5.f);
		glDrawElements(GL_POINTS, (GLsizei)debugJointIndices.size(), GL_UNSIGNED_INT, NULL);

		for (unsigned i = 0; i < skeleton->joints.size(); ++i)
		{
			debugVertices[i].color = glm::vec3(1.f, 1.f, 0.f);
		}

		glBufferData(GL_ARRAY_BUFFER, sizeof(DebugSkeletonVertex) * debugVertices.size(), debugVertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * debugLineIndices.size(), debugLineIndices.data(), GL_STATIC_DRAW);

		glLineWidth(1.f);
		glDrawElements(GL_LINES, (GLsizei)debugLineIndices.size(), GL_UNSIGNED_INT, NULL);
	}

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

void RenderGrid(const glm::mat4& projectionViewModel)
{
	glUseProgram(g_gridProgramId);

	struct GridVertex
	{
		glm::vec3 position;
		// color is hardcoded in GridShader.frag
	};

	unsigned stride = sizeof(GridVertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GridVertex, position)));
	glEnableVertexAttribArray(0);

	glUniformMatrix4fv(g_gridProjectionViewModelMatrixId, 1, GL_FALSE, &projectionViewModel[0][0]);

	std::vector<GridVertex> gridVertices;
	std::vector<unsigned> gridIndices;

	int sideLength = 2000;
	int halfSideLength = sideLength / 2;
	int increment = 100;

	for (int i = -halfSideLength; i <= halfSideLength; i += increment)
	{
		GridVertex gridVertexMaxZ;
		gridVertexMaxZ.position = glm::vec3(i, 0, halfSideLength);
		gridVertices.push_back(gridVertexMaxZ);
		gridIndices.push_back(static_cast<unsigned>(gridIndices.size()));

		GridVertex gridVertexMinZ;
		gridVertexMinZ.position = glm::vec3(i, 0, -halfSideLength);
		gridVertices.push_back(gridVertexMinZ);
		gridIndices.push_back(static_cast<unsigned>(gridIndices.size()));


		GridVertex gridVertexMaxX;
		gridVertexMaxX.position = glm::vec3(halfSideLength, 0, i);
		gridVertices.push_back(gridVertexMaxX);
		gridIndices.push_back(static_cast<unsigned>(gridIndices.size()));

		GridVertex gridVertexMinX;
		gridVertexMinX.position = glm::vec3(-halfSideLength, 0, i);
		gridVertices.push_back(gridVertexMinX);
		gridIndices.push_back(static_cast<unsigned>(gridIndices.size()));
	}

	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GridVertex) * gridVertices.size(), gridVertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * gridIndices.size(), gridIndices.data(), GL_STATIC_DRAW);

	glLineWidth(1.f);
	glDrawElements(GL_LINES, (GLsizei)gridIndices.size(), GL_UNSIGNED_INT, NULL);

	glDisableVertexAttribArray(0);
}

void RenderUI()
{
	glUseProgram(g_uiProgramId);

	struct UIVertex
	{
		glm::vec2 position;
		float uv[2];
	};

	unsigned stride = sizeof(UIVertex);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, position)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, uv)));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	std::vector<UIVertex> uiVertices;
	UIVertex uiVertex;

	uiVertex.position = glm::vec2(-1.f, -1.f);
	uiVertex.uv[0] = 0.f;
	uiVertex.uv[1] = 1.f;
	uiVertices.push_back(uiVertex);

	uiVertex.position = glm::vec2(1.f, -1.f);
	uiVertex.uv[0] = 1.f;
	uiVertex.uv[1] = 1.f;
	uiVertices.push_back(uiVertex);

	uiVertex.position = glm::vec2(-1.f, 1.f);
	uiVertex.uv[0] = 0.f;
	uiVertex.uv[1] = 0.f;
	uiVertices.push_back(uiVertex);

	uiVertex.position = glm::vec2(1.f, 1.f);
	uiVertex.uv[0] = 1.f;
	uiVertex.uv[1] = 0.f;
	uiVertices.push_back(uiVertex);

	std::vector<unsigned> uiIndices;
	uiIndices.push_back(0);
	uiIndices.push_back(1);
	uiIndices.push_back(2);
	uiIndices.push_back(1);
	uiIndices.push_back(3);
	uiIndices.push_back(2);

	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(UIVertex) * uiVertices.size(), uiVertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * uiIndices.size(), uiIndices.data(), GL_STATIC_DRAW);

	// TODO: How much of this has to be done every Draw() call?
	// TODO: double check these for UI
	glBindTexture(GL_TEXTURE_2D, g_uiTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	const char* viewBuffer = cefMain->GetViewBuffer();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, viewBuffer);
	const char* popupBuffer = cefMain->GetPopupBuffer();
	if (popupBuffer != nullptr)
	{
		const CefRect& popupRect = cefMain->GetPopupRect();
		glTexSubImage2D(GL_TEXTURE_2D, 0, popupRect.x, popupRect.y, popupRect.width, popupRect.height, GL_BGRA, GL_UNSIGNED_BYTE, popupBuffer);
	}
	glGenerateMipmap(GL_TEXTURE_2D);
	glUniform1i(g_uiTextureId, g_uiTextureUnit);

	glDrawElements(GL_TRIANGLES, (GLsizei)uiIndices.size(), GL_UNSIGNED_INT, NULL);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}

void Render()
{
	//Clear color buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// TODO: Is this required? Works without it?
	glBindVertexArray(g_vao);

	glm::mat4 projection = glm::perspective(glm::quarter_pi<float>(), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 10000.0f);
	glm::mat4 view = g_camera->CreateViewMatrix();
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 projectionViewModel = projection * view * model;

	for (size_t i = 0; i < g_assetNames.size(); ++i)
	{
		RenderMesh(*g_meshComponents[i], *g_animationComponents[i], projectionViewModel);
		RenderSkeleton(*g_animationComponents[i], projectionViewModel);
	}

	RenderGrid(projectionViewModel);

	// Because of depth testing, and because the UI is currently rendered as
	// one giant texture the size of the screen instead of just its visible parts,
	// we must render the UI last. Otherwise, everything fails the depth test.
	//
	// Once we render only visible parts, we could render the UI first.
	// Then, everything behind the UI will fail the depth test, but for good reason.
	RenderUI();
}

std::string ReadFile(const char *file)
{

#ifdef __APPLE__
	std::string fileString(file);

	std::string directoryName;
	std::string fileName;
	std::string fileTitle;
	std::string fileExtension;

	size_t slashPos = fileString.find_last_of('/');
	if (slashPos == std::string::npos)
	{
		directoryName = std::string();
		fileName = fileString;
	}
	else
	{
		directoryName = fileString.substr(0, slashPos);
		fileName = fileString.substr(slashPos + 1);
	}
	size_t dotPos = fileName.find_last_of('.');
	fileTitle = fileName.substr(0, dotPos);
	fileExtension = fileName.substr(dotPos + 1);

	// TODO: Free? See https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFMemoryMgmt/Concepts/Ownership.html#//apple_ref/doc/uid/20001148-103029
	// Also do this where this logic is copy/pasted.
	CFStringRef fileTitleStringRef = CFStringCreateWithCStringNoCopy(NULL, fileTitle.c_str(), kCFStringEncodingASCII, kCFAllocatorNull);
	CFStringRef fileExtensionStringRef = CFStringCreateWithCStringNoCopy(NULL, fileExtension.c_str(), kCFStringEncodingASCII, kCFAllocatorNull);
	CFStringRef directoryNameStringRef = directoryName.empty() ? NULL : CFStringCreateWithCStringNoCopy(NULL, directoryName.c_str(), kCFStringEncodingASCII, kCFAllocatorNull);

	CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef fileUrl = CFBundleCopyResourceURL(
		mainBundle,
		fileTitleStringRef,
		fileExtensionStringRef,
		directoryNameStringRef);

	UInt8 realFileName[1024];
	CFURLGetFileSystemRepresentation(fileUrl, true, realFileName, 1024);
#else
	const char* realFileName = file;
#endif

	std::ifstream stream((const char *)realFileName);

	if (!stream.is_open())
	{
		printf("Error Opening File: %s\n", realFileName);
		return std::string();
	}

	std::stringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

GLuint CreateShader(GLenum shaderType, const char* shaderFileName)
{
	GLuint shader = glCreateShader(shaderType);

	std::string shaderSource = ReadFile(shaderFileName);
	const char* shaderSourceStr = shaderSource.c_str();
	glShaderSource(shader, 1, &shaderSourceStr, NULL);

	glCompileShader(shader);

	PrintShaderLog(shader);

	GLint isShaderCompiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
	if (isShaderCompiled != GL_TRUE)
	{
		printf("unable to compile shader %s\n", shaderFileName);
		return -1;
	}

	return shader;
}

GLuint CreateProgram(const char* vertexShaderFileName, const char* fragmentShaderFileName)
{
	GLuint programId = glCreateProgram();

	GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, vertexShaderFileName);
	if (vertexShader == -1)
	{
		return -1;
	}
	glAttachShader(programId, vertexShader);

	GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentShaderFileName);
	if (fragmentShader == -1)
	{
		return -1;
	}
	glAttachShader(programId, fragmentShader);

	glLinkProgram(programId);

	PrintProgramLog(programId);

	GLint programSuccess = GL_TRUE;
	glGetProgramiv(programId, GL_LINK_STATUS, &programSuccess);
	if (programSuccess != GL_TRUE)
	{
		printf(
			"error linking program %d for shaders %s and %s\n",
			programId,
			vertexShaderFileName,
			fragmentShaderFileName);
		return -1;
	}

	return programId;
}


bool InitializeOpenGL()
{
	g_skinnedMeshDiffuseTextureProgramId = CreateProgram("shaders/SkinnedMeshShader.vert", "shaders/DiffuseTextureShader.frag");
	if (g_skinnedMeshDiffuseTextureProgramId == -1)
	{
		return false;
	}
	g_skinnedMeshDiffuseTextureProjectionViewModelMatrixId = glGetUniformLocation(g_skinnedMeshDiffuseTextureProgramId, "projectionViewModel");
	g_skinnedMeshDiffuseTexturePaletteId = glGetUniformLocation(g_skinnedMeshDiffuseTextureProgramId, "palette");
	g_skinnedMeshDiffuseTextureDiffuseTextureId = glGetUniformLocation(g_skinnedMeshDiffuseTextureProgramId, "diffuseTexture");

	g_skeletonProgramId = CreateProgram("shaders/SkeletonShader.vert", "shaders/FragmentShader.frag");
	if (g_skeletonProgramId == -1)
	{
		return false;
	}
	g_skeletonProjectionViewModelMatrixId = glGetUniformLocation(g_skeletonProgramId, "projectionViewModel");
	g_skeletonPaletteId = glGetUniformLocation(g_skeletonProgramId, "palette");

	g_uiProgramId = CreateProgram("shaders/UIShader.vert", "shaders/UIShader.frag");
	if (g_uiProgramId == -1)
	{
		return false;
	}
	g_uiTextureId = glGetUniformLocation(g_uiProgramId, "uiTexture");

	g_gridProgramId = CreateProgram("shaders/GridShader.vert", "shaders/GridShader.frag");
	if (g_gridProgramId == -1)
	{
		return false;
	}
	g_gridProjectionViewModelMatrixId = glGetUniformLocation(g_gridProgramId, "projectionViewModel");

	g_skinnedMeshWireFrameDiffuseTextureProgramId = CreateProgram("shaders/SkinnedMeshShader.vert", "shaders/WireFrameDiffuseTextureShader.frag");
	if (g_skinnedMeshWireFrameDiffuseTextureProgramId == -1)
	{
		return false;
	}
	g_skinnedMeshWireFrameDiffuseTextureProjectionViewModelMatrixId = glGetUniformLocation(g_skinnedMeshWireFrameDiffuseTextureProgramId, "projectionViewModel");
	g_skinnedMeshWireFrameDiffuseTexturePaletteId = glGetUniformLocation(g_skinnedMeshWireFrameDiffuseTextureProgramId, "palette");
	g_skinnedMeshWireFrameDiffuseTextureDiffuseTextureId = glGetUniformLocation(g_skinnedMeshWireFrameDiffuseTextureProgramId, "diffuseTexture");

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// TODO: what if there are dupes
	for (size_t i = 0; i < g_assetNames.size(); ++i)
	{
		// TODO: bad
		CE::Skeleton* skeleton = new CE::Skeleton();
		CE::Meshes* meshes = new CE::Meshes();
		CE::Animations* animations = new CE::Animations();
		CE::Textures* textures = new CE::Textures();

		CE::AssetImporter::ImportSkeletonMeshesAnimationsTextures(
			g_assetNames[i],
			*skeleton,
			*meshes,
			*animations,
			*textures);

		g_meshComponents.push_back(new CE::MeshComponent(meshes, textures));
		g_animationComponents.push_back(new CE::AnimationComponent(skeleton, animations, eventSystem));
	}

	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);

	// TODO: Why do I need to bind the buffer immediately after generating it?
	glGenBuffers(1, &g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

	glGenBuffers(1, &g_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibo);

	g_diffuseTextureUnit = 0;
	glActiveTexture(GL_TEXTURE0 + g_diffuseTextureUnit);
	glGenTextures(1, &g_diffuseTextureID);
	glBindTexture(GL_TEXTURE_2D, g_diffuseTextureID);

	// TODO: How much of this do I need to do here, versus every call?
	g_paletteTextureUnit = 1;
	glGenBuffers(1, &g_tbo);
	glBindBuffer(GL_TEXTURE_BUFFER, g_tbo);
	glActiveTexture(GL_TEXTURE0 + g_paletteTextureUnit);
	glGenTextures(1, &g_paletteGenTex);
	glBindTexture(GL_TEXTURE_BUFFER, g_paletteGenTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, g_tbo);

	g_uiTextureUnit = 2;
	glActiveTexture(GL_TEXTURE0 + g_uiTextureUnit);
	glGenTextures(1, &g_uiTextureID);
	glBindTexture(GL_TEXTURE_2D, g_uiTextureID);

	return true;
}

#ifdef _WIN32
// osr_window_win.cc
void WindowsMessageHook(
		void* userdata,
		void* hWnd,
		unsigned int message,
		Uint64 wParam,
		Sint64 lParam)
{
	CE::WindowsMessageEvent event;
	event.userdata = userdata;
	event.hWnd = hWnd;
	event.message = message;
	event.wParam = wParam;
	event.lParam = lParam;
	eventSystem->DispatchEvent(event);
}
#endif

bool Initialize(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// Enable MSAA.
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	g_window = SDL_CreateWindow(
		"Composite Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (g_window == NULL)
	{
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return false;
	}

	g_context = SDL_GL_CreateContext(g_window);

	// CefKeyEvent keyevent;
	// keyevent.unmodified_character = 0;

	if (g_context == NULL)
	{
		printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
		return false;
	}

#ifdef _WIN32
	SDL_SetWindowsMessageHook(&WindowsMessageHook, nullptr);
#endif

	// Initialize GLEW
	//glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK)
	{
		printf("Error initializing GLEW! %s\n", glewGetErrorString(glewError));
	}

	// Disable VSync
	if (SDL_GL_SetSwapInterval(0) < 0)
	{
		printf("Warning: Unable to set immediate updates for VSync! SDL_Error: %s\n", SDL_GetError());
	}

	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

	//CE::MeshManager::Get().Initialize(g_fbxImporter);
	//CE::AnimationManager::Get().Initialize(g_fbxImporter);
	//CE::SkeletonManager::Get().Initialize(g_fbxImporter);
	//CE::TextureManager::Get().Initialize(g_stbiImporter);

	eventSystem = new EventSystem();
	engine = new CE::Engine(eventSystem);
	
	cefMain = new CE::CefMain(eventSystem, g_window);

	g_fpsCounter = new CE::FpsCounter(eventSystem);

	g_camera = new CE::Camera(glm::vec3(0, 100, 700), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

	if (!InitializeOpenGL())
	{
		printf("Unable to initialize OpenGL!\n");
		return false;
	}

	const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
	const GLubyte* version = glGetString(GL_VERSION); // version as a string
	printf("GL_RENDERER: %s\n", renderer);
	printf("GL_VERSION: %s\n", version);
	// TODO: Doesn't work on my laptop's Intel HD Graphics 4000, which only supports up to OpenGL 4.0.

	// GLint components;
	// glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
	// printf("GL_MAX_VERTEX_UNIFORM_COMPONENTS: %u\n", components);

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

	// Enable MSAA.
	glEnable(GL_MULTISAMPLE);

	// enable alpha blending (allows transparent textures)
	// https://gamedev.stackexchange.com/questions/29492/opengl-blending-gui-textures
	// https://www.opengl.org/archives/resources/faq/technical/transparency.htm
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (!cefMain->StartCef(argc, argv, SCREEN_WIDTH, SCREEN_HEIGHT))
	{
		printf("CEF failed to start!\n");
		return false;
	}

	return true;
}

void Destroy()
{
	CE::MeshManager::Get().Destroy();
	CE::AnimationManager::Get().Destroy();
	CE::SkeletonManager::Get().Destroy();
	CE::TextureManager::Get().Destroy();

	cefMain->StopCef();

	SDL_DestroyWindow(g_window);
	g_window = NULL;

	SDL_Quit();
}

int main(int argc, char* argv[])
{
	CE_SET_MAIN_THREAD();

	if (!Initialize(argc, argv))
	{
		printf("Failed to initialize.\n");
		return 1;
	}

	uint64_t currentTicks = SDL_GetPerformanceCounter();
	uint64_t previousTicks = 0;

	CE::RealTimeClock::Get().Initialize(currentTicks);
	CE::GameTimeClock::Get().Initialize(currentTicks);

	bool quit = false;

	CE::EditorCameraEventHandler editorCameraEventHandler(eventSystem, g_camera);

	while (!quit)
	{
		previousTicks = currentTicks;
		currentTicks = SDL_GetPerformanceCounter();

		uint64_t deltaTicks = currentTicks - previousTicks;
		CE::RealTimeClock::Get().Update(deltaTicks);
		CE::GameTimeClock::Get().Update(deltaTicks);

		SDL_Event event;

		while (SDL_PollEvent(&event) != 0)
		{
			CE::SdlEvent wrappedEvent;
			wrappedEvent.event = event;
			eventSystem->DispatchEvent(wrappedEvent);

			// TODO: Haven't done focus events for Cef (see CefBrowserHost). Do I need these?
			switch (event.type)
			{
				case SDL_QUIT:
				{
					quit = true;
					break;
				}

				case SDL_KEYDOWN:
				{
					switch (event.key.keysym.sym)
					{
						case SDLK_q:
						{
							eventSystem->EnqueueEvent(ToggleBindPoseEvent());
							break;
						}

						case SDLK_e:
						{
							SetRenderModeEvent setRenderModeEvent;
							setRenderModeEvent.mode = (engine->GetRenderMode() + 1) % 3;
							eventSystem->EnqueueEvent(setRenderModeEvent);
							break;
						}
					}

					break;
				}
			}
		}

		editorCameraEventHandler.Update(CE::RealTimeClock::Get().GetDeltaSeconds());

		for (CE::AnimationComponent* animationComponent : g_animationComponents)
		{
			animationComponent->Update(CE::GameTimeClock::Get().GetDeltaSeconds());
		}
		g_fpsCounter->Update(CE::RealTimeClock::Get().GetDeltaSeconds());

		// TODO: Where does this go?
		eventSystem->DispatchEvents(CE::RealTimeClock::Get().GetCurrentTicks());

		Render();

		SDL_GL_SwapWindow(g_window);
	}

	Destroy();

	return 0;
}
