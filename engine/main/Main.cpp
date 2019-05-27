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

#ifdef _WIN32
#include "include/cef_app.h"
#include "cef/client/UIClient.h"
#include "cef/client/UIRenderHandler.h"
#include "cef/browser/UIAppBrowser.h"
#include "cef/browser/UIBrowserProcessHandler.h"
#include "cef/client/UIRequestHandler.h"
#include "cef/client/UILifeSpanHandler.h"
#include "cef/browser/UIQueryHandler.h"
#include "cef/browser/UIQueryResponder.h"
#include "cef/browser/UIExternalMessagePump.h"
#include "include/wrapper/cef_message_router.h"
#endif

#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

SDL_Window* g_window = NULL;
SDL_GLContext g_context;

bool g_renderQuad = true;

GLuint g_programID = 0;
GLuint g_vbo = 0;
GLuint g_ibo = 0;
GLuint g_vao = 0;
GLuint g_tbo = 0;

GLuint g_programID2 = 0;
GLuint g_programID4 = 0;
GLuint g_programID5 = 0;
GLuint g_programID6 = 0;

GLuint g_projectionViewModelMatrixID = -1;
GLuint g_paletteID = -1;
GLuint g_paletteTextureUnit = -1;
GLuint g_paletteGenTex = -1;
GLuint g_diffuseTextureLocation = -1;
GLuint g_diffuseTextureUnit = -1;
GLuint g_diffuseTextureID = -1;

GLuint g_projectionViewModelMatrixID2 = -1;
GLuint g_paletteID2 = -1;

GLuint g_projectionViewModelMatrixID6 = -1;
GLuint g_paletteID6 = -1;
GLuint g_diffuseTextureLocation6 = -1;

GLuint g_projectionViewModelMatrixID5 = -1;

GLuint g_uiTextureLocation = -1;
GLuint g_uiTextureUnit = -1;
GLuint g_uiTextureID = -1;

//const char* g_assetName = "assets/Stand Up.ceasset";
//const char* g_assetName = "assets/Thriller Part 2.ceasset";
//const char* g_assetName = "assets/jla_wonder_woman.ceasset";
//const char* g_assetName = "assets/Quarterback Pass.ceasset";
//const char* g_fbxName = "assets/Soldier_animated_jump.fbx";
//const char* g_assetName = "assets/Standing Walk Forward.ceasset";

std::vector<const char*> g_assetNames = {
	"assets/Quarterback Pass.ceasset",
	"assets/Thriller Part 2.ceasset",
	"assets/Standing Walk Forward.ceasset"
};

CE::AssetImporter* g_assetImporter;

std::vector<CE::MeshComponent*> g_meshComponents;
std::vector<CE::AnimationComponent*> g_animationComponents;

#ifdef _WIN32
CefRefPtr<UIClient> g_uiClient;
CefRefPtr<CefBrowser> g_browser;
UIQueryHandler* queryHandler;
UIExternalMessagePump* externalMessagePump;
#endif

EventSystem* eventSystem;
CE::Engine* engine;

CE::FpsCounter* g_fpsCounter;

CE::Camera* g_camera;

void printProgramLog(GLuint program)
{
	//Make sure name is shader
	if (!glIsProgram(program))
	{
		printf("Name %d is not a program\n", program);
		return;
	}

	//Program log length
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	//Get info string length
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

	//Allocate string
	char* infoLog = new char[maxLength];

	//Get info log
	glGetProgramInfoLog(program, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0)
	{
		//Print Log
		printf("%s\n", infoLog);
	}

	//Deallocate string
	delete[] infoLog;
}

void printShaderLog(GLuint shader)
{
	//Make sure name is shader
	if (!glIsShader(shader))
	{
		printf("Name %d is not a shader\n", shader);
		return;
	}

	//Shader log length
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	//Get info string length
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

	//Allocate string
	char* infoLog = new char[maxLength];

	//Get info log
	glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
	if (infoLogLength > 0)
	{
		//Print Log
		printf("%s\n", infoLog);
	}

	//Deallocate string
	delete[] infoLog;
}

void RenderMesh(CE::MeshComponent& meshComponent, CE::AnimationComponent& animationComponent, const glm::mat4& projectionViewModel)
{
	bool renderWireFrameOnly = engine->RenderMode() == 3;
	GLuint activeProgramID = -1;
	GLuint activeProjectionViewModelMatrixID = -1;
	GLuint activePaletteID = -1;
	GLuint activeDiffuseTextureLocation = -1;

	if (renderWireFrameOnly)
	{
		activeProgramID = g_programID6;
		activeProjectionViewModelMatrixID = g_projectionViewModelMatrixID6;
		activePaletteID = g_paletteID6;
		activeDiffuseTextureLocation = g_diffuseTextureLocation6;
	}
	else
	{
		activeProgramID = g_programID;
		activeProjectionViewModelMatrixID = g_projectionViewModelMatrixID;
		activePaletteID = g_paletteID;
		activeDiffuseTextureLocation = g_diffuseTextureLocation;
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

	if (engine->RenderMode() == 0 || engine->RenderMode() == 2 || renderWireFrameOnly)
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
	glUseProgram(g_programID2);

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

	glUniformMatrix4fv(g_projectionViewModelMatrixID2, 1, GL_FALSE, &projectionViewModel[0][0]);

	if (engine->IsRenderBindPose())
	{
		animationComponent.ResetMatrixPalette();
	}

	animationComponent.BindMatrixPalette(
		g_paletteTextureUnit,
		g_paletteGenTex,
		g_tbo,
		g_paletteID2);

	const CE::Skeleton* skeleton = animationComponent.GetSkeleton();// CE::SkeletonManager::Get().GetSkeleton(g_fbxName);

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

	if (engine->RenderMode() == 1 || engine->RenderMode() == 2)
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
	glUseProgram(g_programID5);

	struct GridVertex
	{
		glm::vec3 position;
		// color is hardcoded in GridShader.frag
	};

	unsigned stride = sizeof(GridVertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GridVertex, position)));
	glEnableVertexAttribArray(0);

	glUniformMatrix4fv(g_projectionViewModelMatrixID5, 1, GL_FALSE, &projectionViewModel[0][0]);

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
#ifdef _WIN32
	glUseProgram(g_programID4);

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
	char* viewBuffer = ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetViewBuffer();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, viewBuffer);
	char* popupBuffer = ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetPopupBuffer();
	if (popupBuffer != nullptr)
	{
		const CefRect& popupRect = ((UIRenderHandler*)(g_uiClient->GetRenderHandler().get()))->GetPopupRect();
		glTexSubImage2D(GL_TEXTURE_2D, 0, popupRect.x, popupRect.y, popupRect.width, popupRect.height, GL_BGRA, GL_UNSIGNED_BYTE, popupBuffer);
	}
	glGenerateMipmap(GL_TEXTURE_2D);
	glUniform1i(g_uiTextureLocation, g_uiTextureUnit);

	glDrawElements(GL_TRIANGLES, (GLsizei)uiIndices.size(), GL_UNSIGNED_INT, NULL);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
#endif
}

void Render()
{
	//Clear color buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// TODO: Is this required? Works without it?
	glBindVertexArray(g_vao);

	glm::mat4 projection = glm::perspective(glm::pi<float>() * 0.25f, (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 10000.0f);
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

bool CreateProgram2()
{
	g_programID2 = glCreateProgram();

	// TODO: Copy shaders in CMAKE to .exe dir (or subdir next to .exe).

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vertexShaderSource = ReadFile("shaders/SkeletonShader.vert");
	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, NULL);
	glCompileShader(vertexShader);
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	printShaderLog(vertexShader);
	if (vShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		return false;
	}
	glAttachShader(g_programID2, vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	//const GLchar* fragmentShaderSource[] =
	//{
	//	"#version 410\nout vec4 LFragment; void main() { LFragment = vec4(1.0, 1.0, 1.0, 1.0); }"
	//};
	std::string fragmentShaderSource = ReadFile("shaders/FragmentShader.frag");
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, NULL);
	glCompileShader(fragmentShader);
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	printShaderLog(fragmentShader);
	if (fShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		return false;
	}
	glAttachShader(g_programID2, fragmentShader);

	glLinkProgram(g_programID2);
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(g_programID2, GL_LINK_STATUS, &programSuccess);
	printProgramLog(g_programID2);
	if (programSuccess != GL_TRUE)
	{
		printf("Error linking program %d!\n", g_programID2);
		return false;
	}

	return true;
}

bool CreateProgram4()
{
	g_programID4 = glCreateProgram();

	// TODO: Copy shaders in CMAKE to .exe dir (or subdir next to .exe).

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vertexShaderSource = ReadFile("shaders/UIShader.vert");
	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, NULL);
	glCompileShader(vertexShader);
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	printShaderLog(vertexShader);
	if (vShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		return false;
	}
	glAttachShader(g_programID4, vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	//const GLchar* fragmentShaderSource[] =
	//{
	//	"#version 410\nout vec4 LFragment; void main() { LFragment = vec4(1.0, 1.0, 1.0, 1.0); }"
	//};
	std::string fragmentShaderSource = ReadFile("shaders/UIShader.frag");
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, NULL);
	glCompileShader(fragmentShader);
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	printShaderLog(fragmentShader);
	if (fShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		return false;
	}
	glAttachShader(g_programID4, fragmentShader);

	glLinkProgram(g_programID4);
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(g_programID4, GL_LINK_STATUS, &programSuccess);
	printProgramLog(g_programID4);
	if (programSuccess != GL_TRUE)
	{
		printf("Error linking program %d!\n", g_programID4);
		return false;
	}

	return true;
}

bool CreateProgram5()
{
	g_programID5 = glCreateProgram();

	// TODO: Copy shaders in CMAKE to .exe dir (or subdir next to .exe).

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vertexShaderSource = ReadFile("shaders/GridShader.vert");
	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, NULL);
	glCompileShader(vertexShader);
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	printShaderLog(vertexShader);
	if (vShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		return false;
	}
	glAttachShader(g_programID5, vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	//const GLchar* fragmentShaderSource[] =
	//{
	//	"#version 410\nout vec4 LFragment; void main() { LFragment = vec4(1.0, 1.0, 1.0, 1.0); }"
	//};
	std::string fragmentShaderSource = ReadFile("shaders/GridShader.frag");
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, NULL);
	glCompileShader(fragmentShader);
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	printShaderLog(fragmentShader);
	if (fShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		return false;
	}
	glAttachShader(g_programID5, fragmentShader);

	glLinkProgram(g_programID5);
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(g_programID5, GL_LINK_STATUS, &programSuccess);
	printProgramLog(g_programID5);
	if (programSuccess != GL_TRUE)
	{
		printf("Error linking program %d!\n", g_programID5);
		return false;
	}

	return true;
}

bool CreateProgram6()
{
	g_programID6 = glCreateProgram();

	// TODO: Copy shaders in CMAKE to .exe dir (or subdir next to .exe).

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vertexShaderSource = ReadFile("shaders/SkinnedMeshShader.vert");
	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, NULL);
	glCompileShader(vertexShader);
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	printShaderLog(vertexShader);
	if (vShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		return false;
	}
	glAttachShader(g_programID6, vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	//const GLchar* fragmentShaderSource[] =
	//{
	//	"#version 410\nout vec4 LFragment; void main() { LFragment = vec4(1.0, 1.0, 1.0, 1.0); }"
	//};
	std::string fragmentShaderSource = ReadFile("shaders/WireFrameDiffuseTextureShader.frag");
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, NULL);
	glCompileShader(fragmentShader);
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	printShaderLog(fragmentShader);
	if (fShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		return false;
	}
	glAttachShader(g_programID6, fragmentShader);

	glLinkProgram(g_programID6);
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(g_programID6, GL_LINK_STATUS, &programSuccess);
	printProgramLog(g_programID6);
	if (programSuccess != GL_TRUE)
	{
		printf("Error linking program %d!\n", g_programID6);
		return false;
	}

	return true;
}


bool InitializeOpenGL()
{
	g_programID = glCreateProgram();

	// TODO: Copy shaders in CMAKE to .exe dir (or subdir next to .exe).

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	std::string vertexShaderSource = ReadFile("shaders/SkinnedMeshShader.vert");
	const char* vertexShaderSourceStr = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &vertexShaderSourceStr, NULL);
	glCompileShader(vertexShader);
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	printShaderLog(vertexShader);
	if (vShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile vertex shader %d!\n", vertexShader);
		return false;
	}
	glAttachShader(g_programID, vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	//const GLchar* fragmentShaderSource[] =
	//{
	//	"#version 410\nout vec4 LFragment; void main() { LFragment = vec4(1.0, 1.0, 1.0, 1.0); }"
	//};
	std::string fragmentShaderSource = ReadFile("shaders/DiffuseTextureShader.frag");
	const char* fragmentShaderSourceStr = fragmentShaderSource.c_str();
	glShaderSource(fragmentShader, 1, &fragmentShaderSourceStr, NULL);
	glCompileShader(fragmentShader);
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	printShaderLog(fragmentShader);
	if (fShaderCompiled != GL_TRUE)
	{
		printf("Unable to compile fragment shader %d!\n", fragmentShader);
		return false;
	}
	glAttachShader(g_programID, fragmentShader);

	glLinkProgram(g_programID);
	GLint programSuccess = GL_TRUE;
	glGetProgramiv(g_programID, GL_LINK_STATUS, &programSuccess);
	printProgramLog(g_programID);
	if (programSuccess != GL_TRUE)
	{
		printf("Error linking program %d!\n", g_programID);
		return false;
	}

	if (!CreateProgram2())
	{
		return false;
	}

	if (!CreateProgram4())
	{
		return false;
	}

	if (!CreateProgram5())
	{
		return false;
	}

	if (!CreateProgram6())
	{
		return false;
	}

	g_uiTextureLocation = glGetUniformLocation(g_programID4, "uiTexture");

	g_projectionViewModelMatrixID5 = glGetUniformLocation(g_programID5, "projectionViewModel");

	g_projectionViewModelMatrixID2 = glGetUniformLocation(g_programID2, "projectionViewModel");
	g_paletteID2 = glGetUniformLocation(g_programID2, "palette");

	g_projectionViewModelMatrixID6 = glGetUniformLocation(g_programID6, "projectionViewModel");
	g_paletteID6 = glGetUniformLocation(g_programID6, "palette");
	g_diffuseTextureLocation6 = glGetUniformLocation(g_programID6, "diffuseTexture");

	g_projectionViewModelMatrixID = glGetUniformLocation(g_programID, "projectionViewModel");
	g_paletteID = glGetUniformLocation(g_programID, "palette");
	g_diffuseTextureLocation = glGetUniformLocation(g_programID, "diffuseTexture");

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

bool StartCef()
{
#ifdef _WIN32
	CefMainArgs main_args(::GetModuleHandle(NULL));

	externalMessagePump = new UIExternalMessagePump();
	CefRefPtr<UIBrowserProcessHandler> browserProcessHandler = new UIBrowserProcessHandler(externalMessagePump);
	CefRefPtr<UIAppBrowser> app = new UIAppBrowser(browserProcessHandler);

	CefSettings settings;
	settings.external_message_pump = true;
	settings.windowless_rendering_enabled = true;
	settings.remote_debugging_port = 3469;
	CefString(&settings.browser_subprocess_path).FromASCII("CompositeCefSubprocess.exe");
	if (!CefInitialize(main_args, settings, app, NULL))
	{
		printf("CEF failed to initialize.\n");
		return false;
	}

	CefMessageRouterConfig messageRouterConfig;
	CefRefPtr<CefMessageRouterBrowserSide> messageRouterBrowserSide = CefMessageRouterBrowserSide::Create(messageRouterConfig);
	messageRouterBrowserSide->AddHandler(queryHandler, true);

	CefRefPtr<UIContextMenuHandler> contextMenuHandler = new UIContextMenuHandler();
	CefRefPtr<UIRenderHandler> renderHandler = new UIRenderHandler(SCREEN_WIDTH, SCREEN_HEIGHT);
	CefRefPtr<UILifeSpanHandler> lifeSpanHandler = new UILifeSpanHandler(messageRouterBrowserSide);
	CefRefPtr<UILoadHandler> loadHandler = new UILoadHandler();
	CefRefPtr<UIRequestHandler> requestHandler = new UIRequestHandler(messageRouterBrowserSide);
	g_uiClient = new UIClient(
		contextMenuHandler,
		renderHandler,
		lifeSpanHandler,
		loadHandler,
		requestHandler,
		messageRouterBrowserSide);

	SDL_SysWMinfo sysInfo;
	SDL_VERSION(&sysInfo.version);
	if (!SDL_GetWindowWMInfo(g_window, &sysInfo))
	{
		return false;
	}

	CefBrowserSettings browserSettings;
	CefWindowInfo windowInfo;
	windowInfo.SetAsWindowless(sysInfo.info.win.window);

	g_browser = CefBrowserHost::CreateBrowserSync(
		windowInfo,
		g_uiClient,
		"http://localhost:3000", // "about:blank"
		browserSettings,
		nullptr);
#endif

	return true;
}

void ToggleDevToolsWindow()
{
#ifdef _WIN32
	if (g_browser->GetHost()->HasDevTools())
	{
		g_browser->GetHost()->CloseDevTools();
	}
	else
	{
		SDL_SysWMinfo sysInfo;
		SDL_VERSION(&sysInfo.version);
		if (!SDL_GetWindowWMInfo(g_window, &sysInfo))
		{
			return;
		}

		CefBrowserSettings browserSettings;
		CefWindowInfo windowInfo;
		windowInfo.SetAsPopup(sysInfo.info.win.window, "DevTools");
		g_browser->GetHost()->ShowDevTools(windowInfo, g_uiClient, browserSettings, CefPoint(0, 0));
	}
#endif
}

#ifdef _WIN32
bool IsKeyDown(WPARAM wparam) {
	return (GetKeyState((int)wparam) & 0x8000) != 0;
}

// TODO: Either convert this to SDL or convert GetCefMouseModifiers to native.
int GetCefKeyboardModifiers(WPARAM wparam, LPARAM lparam) {
	int modifiers = 0;
	if (IsKeyDown(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (IsKeyDown(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (IsKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam) {
	case VK_RETURN:
		if ((lparam >> 16) & KF_EXTENDED)
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		if (!((lparam >> 16) & KF_EXTENDED))
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_NUMLOCK:
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	case VK_DIVIDE:
	case VK_MULTIPLY:
	case VK_SUBTRACT:
	case VK_ADD:
	case VK_DECIMAL:
	case VK_CLEAR:
		modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_SHIFT:
		if (IsKeyDown(VK_LSHIFT))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RSHIFT))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_CONTROL:
		if (IsKeyDown(VK_LCONTROL))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RCONTROL))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_MENU:
		if (IsKeyDown(VK_LMENU))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RMENU))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_LWIN:
		modifiers |= EVENTFLAG_IS_LEFT;
		break;
	case VK_RWIN:
		modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	}
	return modifiers;
}

void WindowsMessageHook(
		void* userdata,
		void* hWnd,
		unsigned int message,
		Uint64 wParam,
		Sint64 lParam)
{
	switch (message)
	{
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
		{
			CefKeyEvent keyEvent;
			keyEvent.windows_key_code = static_cast<int>(wParam);
			keyEvent.native_key_code = static_cast<int>(lParam);
			keyEvent.is_system_key = message == WM_SYSCHAR
				|| message == WM_SYSKEYDOWN
				|| message == WM_SYSKEYUP;
			if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
			{
				keyEvent.type = KEYEVENT_RAWKEYDOWN;
			}
			else if (message == WM_KEYUP || message == WM_SYSKEYUP)
			{
				keyEvent.type = KEYEVENT_KEYUP;
			}
			else
			{
				keyEvent.type = KEYEVENT_CHAR;
			}
			keyEvent.modifiers = GetCefKeyboardModifiers(static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));

			g_browser->GetHost()->SendKeyEvent(keyEvent);

			break;
		}
	}
}
#endif

bool Initialize()
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

#ifdef _WIN32
	queryHandler = new UIQueryHandler(eventSystem, new UIQueryResponder(eventSystem));
#endif

	g_fpsCounter = new CE::FpsCounter(eventSystem);

	//g_camera = new CE::Camera(glm::vec3(0, 100, 400), glm::vec3(0, 100, 0)); // paladin
	//g_camera = new CE::Camera(glm::vec3(0, 200, 400), glm::vec3(0, 100, 0)); // solider
	g_camera = new CE::Camera(glm::vec3(0, 100, 700), glm::vec3(0, 0, -1)); // thriller, quarterback
	//g_camera = new CE::Camera(glm::vec3(0, 2, 8), glm::vec3(0, 2, 0)); // wonder woman
	//g_camera = new CE::Camera(glm::vec3(0, 0, 1), glm::vec3(0, 0, 0));

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

	GLint components;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
	printf("GL_MAX_VERTEX_UNIFORM_COMPONENTS: %u\n", components);

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

	if (!StartCef())
	{
		printf("CEF failed to start!\n");
		return false;
	}

	return true;
}

void StopCef()
{
#ifdef _WIN32
	externalMessagePump->Shutdown();

	g_browser->GetHost()->CloseBrowser(true);

	g_browser = nullptr;
	g_uiClient = nullptr;

	CefShutdown();
#endif
}

void Destroy()
{
	CE::MeshManager::Get().Destroy();
	CE::AnimationManager::Get().Destroy();
	CE::SkeletonManager::Get().Destroy();
	CE::TextureManager::Get().Destroy();

	StopCef();

	SDL_DestroyWindow(g_window);
	g_window = NULL;

	SDL_Quit();
}

// TODO: Either convert this to native or convert GetCefKeyboardModifiers to SDL.
// osr_window_win.cc
#ifdef _WIN32
unsigned GetCefMouseModifiers(const SDL_Event& event)
{
	unsigned modifiers = 0;

	SDL_Keymod keymod = SDL_GetModState();

	if (keymod & KMOD_CTRL)
	{
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	}

	if (keymod & KMOD_SHIFT)
	{
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	}

	if (keymod & KMOD_ALT)
	{
		modifiers |= EVENTFLAG_ALT_DOWN;
	}

	if (keymod & KMOD_NUM)
	{
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	}

	if (keymod & KMOD_CAPS)
	{
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	}

#ifdef __APPLE__
	if (keymod & KMOD_GUI)
	{
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	}
#endif

	switch (event.type)
	{
		case SDL_MOUSEMOTION:
		{
			if (event.motion.state & SDL_BUTTON_LMASK)
			{
				modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
			}

			if (event.motion.state & SDL_BUTTON_MMASK)
			{
				modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
			}

			if (event.motion.state & SDL_BUTTON_RMASK)
			{
				modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
			}

			break;
		}

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			if (event.button.button == SDL_BUTTON_LEFT)
			{
				modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
			}

			if (event.button.button == SDL_BUTTON_MIDDLE)
			{
				modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
			}

			if (event.button.button == SDL_BUTTON_RIGHT)
			{
				modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
			}

			break;
		}

		case SDL_MOUSEWHEEL:
		{
			unsigned state = SDL_GetMouseState(NULL, NULL);

			if (state & SDL_BUTTON_LMASK)
			{
				modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
			}

			if (state & SDL_BUTTON_MMASK)
			{
				modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
			}

			if (state & SDL_BUTTON_RMASK)
			{
				modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
			}

			break;
		}

		case SDL_WINDOWEVENT:
		{
			switch (event.window.event)
			{
				case SDL_WINDOWEVENT_LEAVE:
				{
					unsigned state = SDL_GetMouseState(NULL, NULL);

					if (state & SDL_BUTTON_LMASK)
					{
						modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
					}

					if (state & SDL_BUTTON_MMASK)
					{
						modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
					}

					if (state & SDL_BUTTON_RMASK)
					{
						modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
					}

					break;
				}
			}

			break;
		}
	}

	return modifiers;
}
#endif

int main(int argc, char* argv[])
{
	CE_SET_MAIN_THREAD();

	if (!Initialize())
	{
		printf("Failed to initialize.\n");
		return -1;
	}

	uint64_t currentTicks = SDL_GetPerformanceCounter();
	uint64_t previousTicks = 0;

	CE::RealTimeClock::Get().Initialize(currentTicks);
	CE::GameTimeClock::Get().Initialize(currentTicks);

	bool quit = false;

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
#ifdef _WIN32
			externalMessagePump->ProcessEvent(event);
#endif

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
							setRenderModeEvent.mode = (engine->RenderMode() + 1) % 3;
							eventSystem->EnqueueEvent(setRenderModeEvent);
							break;
						}

						case SDLK_w:
						{
							g_camera->MoveForward(1000 * CE::RealTimeClock::Get().GetDeltaSeconds());
							break;
						}

						case SDLK_a:
						{
							g_camera->MoveLeft(1000 * CE::RealTimeClock::Get().GetDeltaSeconds());
							break;
						}

						case SDLK_s:
						{
							g_camera->MoveBackward(1000 * CE::RealTimeClock::Get().GetDeltaSeconds());
							break;
						}

						case SDLK_d:
						{
							g_camera->MoveRight(1000 * CE::RealTimeClock::Get().GetDeltaSeconds());
							break;
						}

						case SDLK_F11:
						case SDLK_F12:
						{
							ToggleDevToolsWindow();
							break;
						}
					}

					break;
				}

#ifdef _WIN32
				// osr_window_win.cc
				case SDL_MOUSEMOTION:
				{
					CefMouseEvent mouseEvent;
					mouseEvent.x = event.motion.x;
					mouseEvent.y = event.motion.y;
					mouseEvent.modifiers = GetCefMouseModifiers(event);

					g_browser->GetHost()->SendMouseMoveEvent(mouseEvent, false);

					break;
				}

				// osr_window_win.cc
				case SDL_MOUSEBUTTONDOWN:
				{
					CefBrowserHost::MouseButtonType mouseButtonType;
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						mouseButtonType = MBT_LEFT;
					}
					else if (event.button.button == SDL_BUTTON_MIDDLE)
					{
						mouseButtonType = MBT_MIDDLE;
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						mouseButtonType = MBT_RIGHT;
					}
					else
					{
						break;
					}

					CefMouseEvent mouseEvent;
					mouseEvent.x = event.button.x;
					mouseEvent.y = event.button.y;
					mouseEvent.modifiers = GetCefMouseModifiers(event);

					g_browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, false, event.button.clicks);

					break;
				}

				// osr_window_win.cc
				case SDL_MOUSEBUTTONUP:
				{
					CefBrowserHost::MouseButtonType mouseButtonType;
					if (event.button.button == SDL_BUTTON_LEFT)
					{
						mouseButtonType = MBT_LEFT;
					}
					else if (event.button.button == SDL_BUTTON_MIDDLE)
					{
						mouseButtonType = MBT_MIDDLE;
					}
					else if (event.button.button == SDL_BUTTON_RIGHT)
					{
						mouseButtonType = MBT_RIGHT;
					}
					else
					{
						break;
					}

					CefMouseEvent mouseEvent;
					mouseEvent.x = event.button.x;
					mouseEvent.y = event.button.y;
					mouseEvent.modifiers = GetCefMouseModifiers(event);

					g_browser->GetHost()->SendMouseClickEvent(mouseEvent, mouseButtonType, true, event.button.clicks);

					break;
				}

				// osr_window_win.cc
				case SDL_MOUSEWHEEL:
				{
					CefMouseEvent mouseEvent;
					SDL_GetMouseState(&mouseEvent.x, &mouseEvent.y);
					mouseEvent.modifiers = GetCefMouseModifiers(event);

					g_browser->GetHost()->SendMouseWheelEvent(mouseEvent, event.wheel.x, event.wheel.y);

					break;
				}

				// osr_window_win.cc
				case SDL_WINDOWEVENT:
				{
					switch (event.window.event)
					{
						case SDL_WINDOWEVENT_LEAVE:
						{
							CefMouseEvent mouseEvent;
							SDL_GetMouseState(&mouseEvent.x, &mouseEvent.y);
							mouseEvent.modifiers = GetCefMouseModifiers(event);

							g_browser->GetHost()->SendMouseMoveEvent(mouseEvent, true);

							break;
						}
					}

					break;
				}
#endif
			}
		}

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
