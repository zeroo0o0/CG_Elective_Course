/*
Student Information
Student ID:
Student Name:
*/

#include "Dependencies/glew/glew.h"
#include "Dependencies/GLFW/glfw3.h"
#include "Dependencies/glm/glm.hpp"
#include "Dependencies/glm/gtc/matrix_transform.hpp"

#include "Shader.h"
#include "Texture.h"

#include "Dependencies/assimp/Importer.hpp"
#include "Dependencies/assimp/scene.h"
#include "Dependencies/assimp/postprocess.h"
#include "Dependencies/stb_image/stb_image.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <limits>

// 全局变量
Shader mainShader;
Shader skyboxShader;
Texture penguinDiff;
Texture snowDiff;
Texture skyboxTexture;
Texture soccerDiff;
Texture stadiumDiff;

struct MeshRange {
	unsigned int baseIndex;
	unsigned int indexCount;
	std::string materialName;
	glm::vec3 boundsMin;
	glm::vec3 boundsMax;
	bool hasBounds;
};

// 材质信息结构体（包含纹理和颜色）
struct MaterialInfo {
	Texture* texture;
	glm::vec3 color;
};

// 材质纹理缓存
std::map<std::string, Texture*> textureCache;

// 材质信息缓存
std::map<std::string, MaterialInfo> materialInfoCache;

unsigned int penguinVAO, penguinVBO, penguinEBO;
unsigned int snowVAO, snowVBO, snowEBO;
unsigned int skyboxVAO, skyboxVBO;
unsigned int stadiumVAO, stadiumVBO, stadiumEBO;

unsigned int penguinIndexCount, snowIndexCount;
unsigned int soccerVAO, soccerVBO, soccerEBO;
unsigned int soccerIndexCount;
unsigned int stadiumIndexCount;

// Stadium mesh ranges for multi-material rendering
std::vector<MeshRange> stadiumMeshRanges;

// screen setting
const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

// camera
glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 10.0f); // 更高更远
glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, -5.0f, -10.0f)); // 俯视企鹅
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float lightBrightness = 0.7f;
// 聚光灯亮度：与方向光分开控制
float spotlightBrightness = 2.0f;
// spotlight enabled toggle (will be controlled automatically based on ambient)
bool spotlightEnabled = true;

// 基础环境光（固定，不随按键改变）
float ambientDay = 0.6f;

// Directional light day/night constants (T 切换时只改方向光亮度)
float dirLightDayBrightness = 0.6f;
float dirLightNightBrightness = -0.5f;
bool isDayDirectional = true;

// 聚光灯自动开关阈值：根据环境光判断
float spotlightAutoThreshold = 0.2f;
// 每次聚光灯开启时使用的初始亮度
float spotlightInitialBrightness = 3.0f;

// Spotlight variables
glm::vec3 spotlight1Pos = glm::vec3(-5.89f, 2.58f, 0.00f); //pos
glm::vec3 spotlight1Dir = glm::vec3(1.0f, -1.0f, 0.0f); // 向右下方
glm::vec3 spotlight1Color = glm::vec3(1.0f, 1.0f, 1.0f);

glm::vec3 spotlight2Pos = glm::vec3(5.89f, 2.58f, 0.00f); //pos
glm::vec3 spotlight2Dir = glm::vec3(-1.0f, -1.0f, 0.0f); // 向左下方
glm::vec3 spotlight2Color = glm::vec3(1.0f, 1.0f, 1.0f);
// 将聚光灯锥角显著放大以覆盖整个球场：
// inner = 90deg, outer = 180deg (outer 用于平滑衰减到无光)
float spotlightCutoff = glm::cos(glm::radians(90.0f));
float spotlightOuterCutoff = glm::cos(glm::radians(180.0f));

// Penguin control
glm::vec3 penguinPos = glm::vec3(0.0f, -3.175f, 0.7f);
float penguinRotation = 180.0f; // 向后转
float penguinVelocityY = 0.0f;
bool isJumping = false;
const float GRAVITY = -20.0f;
const float JUMP_FORCE = 6.0f;
const float GROUND_Y = -3.175f;

// Camera control
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float fov = 45.0f;
bool firstMouse = true;
bool middleMouseButtonPressed = false;

// Texture control
std::string penguinTexturePath = "resources/penguin/penguin_01.png";
std::string snowTexturePath = "resources/snow/snow_01.jpg";

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 按键状态追踪（支持同时多个方向移动和旋转）
bool keysPressed[GLFW_KEY_LAST + 1] = {false};

// 足球交互系统变量
// 初始让足球从空中掉落（高度可调整）
glm::vec3 soccerPos = glm::vec3(0.0f, 2.0f, 0.09f);
glm::vec3 soccerVelocity = glm::vec3(0.0f);
bool soccerHeld = false;
const float SOCCER_HOLD_RADIUS = 0.25f;
const float SOCCER_KICK_SPEED = 5.0f;
const float SOCCER_VELOCITY_DRAG = 0.95f;
// 足球重力（调大以更明显的自由落体效果）
const float SOCCER_GRAVITY = -25.0f;
// 踢球时附加的向上速度（若企鹅在空中则使用此值）
const float SOCCER_KICK_UP = 6.0f;
// 水平空气阻力系数（每秒速度衰减的比例，线性近似）
const float SOCCER_HORIZONTAL_DRAG = 0.8f; // 0.8 => 每秒约衰减80%
// 地面摩擦系数：当足球接触地面时额外作用于水平速度（线性近似，每秒比例）
const float SOCCER_GROUND_FRICTION = 4.0f;

// 用于存储OBJ文件的结构体
struct Vertex {
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

struct Model {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<MeshRange> meshRanges;
};

Model loadOBJ(const char* objPath)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(objPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
		exit(1);
	}

	Model model;
	unsigned int currentBaseIndex = 0;
	
	for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[i];
		unsigned int baseVertex = (unsigned int)model.vertices.size();
		unsigned int meshIndexCount = 0;
		glm::vec3 meshMin(std::numeric_limits<float>::max());
		glm::vec3 meshMax(std::numeric_limits<float>::lowest());
		
		for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
			Vertex vertex;
			vertex.position = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
			vertex.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
			if (mesh->mTextureCoords[0]) {
				vertex.uv = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
			}
			else {
				vertex.uv = glm::vec2(0.0f, 0.0f);
			}
			model.vertices.push_back(vertex);
			meshMin = glm::min(meshMin, vertex.position);
			meshMax = glm::max(meshMax, vertex.position);
		}
		
		for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
			aiFace face = mesh->mFaces[j];
			for (unsigned int k = 0; k < face.mNumIndices; k++) {
				model.indices.push_back(baseVertex + face.mIndices[k]);
				meshIndexCount++;
			}
		}
		
		// Extract material name
		std::string materialName = "DefaultMaterial";
		if (mesh->mMaterialIndex < scene->mNumMaterials) {
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			aiString name;
			material->Get(AI_MATKEY_NAME, name);
			materialName = std::string(name.C_Str());
		}
		
		MeshRange range;
		range.baseIndex = currentBaseIndex;
		range.indexCount = meshIndexCount;
		range.materialName = materialName;
		range.boundsMin = meshMin;
		range.boundsMax = meshMax;
		range.hasBounds = mesh->mNumVertices > 0;
		model.meshRanges.push_back(range);
		
		currentBaseIndex += meshIndexCount;
	}

	std::cout << "Successfully loaded " << objPath << " using Assimp." << std::endl;
	std::cout << "  Total meshes: " << scene->mNumMeshes << std::endl;
	for (const auto& range : model.meshRanges) {
		std::cout << "  Mesh: " << range.materialName << ", Indices: " << range.indexCount << std::endl;
	}
	return model;
}

// 从缓存中创建或获取纹理
Texture* getOrCreateTexture(const std::string& materialName, const std::string& texturePath)
{
	if (textureCache.find(materialName) == textureCache.end()) {
		Texture* newTexture = new Texture();
		bool flip = true;
		// 某些贴图方向与其他贴图相反，不做垂直翻转
		if (materialName == "coca_cola" || materialName == "POSTER" || materialName == "SCREEN") flip = false;
		newTexture->setupTexture(texturePath.c_str(), flip);
		textureCache[materialName] = newTexture;
		std::cout << "Loaded texture for material: " << materialName << " from " << texturePath << std::endl;
	}
	return textureCache[materialName];
}

void get_OpenGL_info()
{
	// OpenGL信息
	const GLubyte* name = glGetString(GL_VENDOR);
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* glversion = glGetString(GL_VERSION);
	std::cout << "OpenGL version: " << glversion << std::endl;
}

void sendDataToOpenGL()
{
	//待做
	//加载对象并绑定到VAO和VBO
	//加载纹理
	// 包括：纹理路径（有则指定，无则为 nullptr）和 Kd 颜色参数
	struct MaterialDef {
		const char* texturePath;
		glm::vec3 kdColor;
	};
	
	std::map<std::string, MaterialDef> materialDefs = {
		// 6 个有纹理的材质
		{"POSTER", {"resources/soccer_stadium/texture/Snapshot_787.png", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"coca_cola", {"resources/soccer_stadium/texture/what-you-can-see-from-coca-colas-digital-marketing-strategy.jpg", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"date_palm_leaf", {"resources/soccer_stadium/texture/date_palm_leaf_baseColor.png", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"ground.001", {"resources/soccer_stadium/texture/istockphoto-981701222-170667a.jpg", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"palm02", {"resources/soccer_stadium/texture/palm02_baseColor.png", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"wall", {"resources/soccer_stadium/texture/istockphoto-1153840036-170667a.jpg", glm::vec3(0.8f, 0.8f, 0.8f)}},
		
		// 22 个仅有颜色的材质（MTL 中的 Kd 值）
		{"METAL", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"Material.010", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"Material.011", {nullptr, glm::vec3(0.0f, 0.0f, 0.0f)}},
		{"NET", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		// {"SCREEN", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"SCREEN", {"resources/soccer_stadium/texture/Snapshot_787.png", glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"base", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"black_line", {nullptr, glm::vec3(0.007093f, 0.007093f, 0.007093f)}},
		{"blue_row", {nullptr, glm::vec3(0.024559f, 0.110481f, 0.8f)}},
		{"bordlight", {nullptr, glm::vec3(0.035624f, 0.035624f, 0.035624f)}},
		{"floor", {nullptr, glm::vec3(0.066979f, 0.048643f, 0.033644f)}},
		{"footpath_border", {nullptr, glm::vec3(0.0f, 0.0f, 0.0f)}},
		{"footpath_border2", {nullptr, glm::vec3(0.8f, 0.453396f, 0.0f)}},
		{"frame", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"ground", {nullptr, glm::vec3(0.104352f, 0.036809f, 0.023807f)}},
		{"light", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"mirror", {nullptr, glm::vec3(0.033562f, 0.249593f, 0.8f)}},
		{"red_row", {nullptr, glm::vec3(0.289316f, 0.024092f, 0.017649f)}},
		{"road", {nullptr, glm::vec3(0.0f, 0.0f, 0.0f)}},
		{"shutter", {nullptr, glm::vec3(0.058088f, 0.058088f, 0.058088f)}},
		{"silver", {nullptr, glm::vec3(0.8f, 0.711230f, 0.695283f)}},
		{"stadium", {nullptr, glm::vec3(0.8f, 0.8f, 0.8f)}},
		{"windows", {nullptr, glm::vec3(0.130105f, 0.032483f, 0.004530f)}},
	};
	
	// 加载所有材质
	for (const auto& pair : materialDefs) {
		const std::string& materialName = pair.first;
		const MaterialDef& def = pair.second;
		
		MaterialInfo info;
		info.color = def.kdColor;
		info.texture = nullptr;
		
		// 如果有纹理路径，加载纹理
		if (def.texturePath != nullptr) {
			info.texture = getOrCreateTexture(materialName, def.texturePath);
		}
		
		materialInfoCache[materialName] = info;
	}
    // 下面进行模型/VAO/纹理等的初始化（之前意外被放在了全局作用域）

	// 纹理预加载
	snowDiff.setupTexture("resources/snow/snow_01.jpg");

	// 企鹅纹理及模型（尝试从 resources/penguin/penguin.obj 加载；若不存在，将打印错误）
	penguinDiff.setupTexture(penguinTexturePath.c_str());
	{
		Model penguin = loadOBJ("resources/penguin/penguin.obj");
		penguinIndexCount = (unsigned int)penguin.indices.size();

		glGenVertexArrays(1, &penguinVAO);
		glGenBuffers(1, &penguinVBO);
		glGenBuffers(1, &penguinEBO);

		glBindVertexArray(penguinVAO);
		glBindBuffer(GL_ARRAY_BUFFER, penguinVBO);
		glBufferData(GL_ARRAY_BUFFER, penguin.vertices.size() * sizeof(Vertex), &penguin.vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, penguinEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, penguin.indices.size() * sizeof(unsigned int), &penguin.indices[0], GL_STATIC_DRAW);

		// position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		// uv
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
		glEnableVertexAttribArray(1);
		// normal
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	// 雪地：使用简单立方体（有厚度）替代单层平面，便于缩放和放置
	{
		// 每个面4个顶点（共24个顶点），便于正确法线和UV
		std::vector<Vertex> boxVerts;
		boxVerts.reserve(24);

		// 前 (z = 1)
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0,0,1)});
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0,0,1)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0,0,1)});
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0,0,1)});

		// 后 (z = -1)
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0,0,-1)});
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0,0,-1)});
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0,0,-1)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0,0,-1)});

		// 左 (x = -1)
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(-1,0,0)});
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(-1,0,0)});
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(-1,0,0)});
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(-1,0,0)});

		// 右 (x = 1)
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(1,0,0)});
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(1,0,0)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(1,0,0)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(1,0,0)});

		// 上 (y = 1)
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0,1,0)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0,1,0)});
		boxVerts.push_back({glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0,1,0)});
		boxVerts.push_back({glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0,1,0)});

		// 下 (y = -1)
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0,-1,0)});
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0,-1,0)});
		boxVerts.push_back({glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0,-1,0)});
		boxVerts.push_back({glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0,-1,0)});

		// 索引（6面，每面2三角，共36个索引）
		std::vector<unsigned int> boxIdx = {
			0,1,2, 2,3,0,
			4,5,6, 6,7,4,
			8,9,10, 10,11,8,
			12,13,14, 14,15,12,
			16,17,18, 18,19,16,
			20,21,22, 22,23,20
		};

		snowIndexCount = (unsigned int)boxIdx.size();

		glGenVertexArrays(1, &snowVAO);
		glGenBuffers(1, &snowVBO);
		glGenBuffers(1, &snowEBO);

		glBindVertexArray(snowVAO);
		glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
		glBufferData(GL_ARRAY_BUFFER, boxVerts.size() * sizeof(Vertex), &boxVerts[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, snowEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, boxIdx.size() * sizeof(unsigned int), &boxIdx[0], GL_STATIC_DRAW);

		// position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(0);
		// uv
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
		glEnableVertexAttribArray(1);
		// normal
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}
	soccerDiff.setupTexture("resources/soccer1/Ball_2_Texture.png");
	stadiumDiff.setupTexture("resources/soccer_stadium/texture/Snapshot_787.png");

	// Load soccer model (resources/soccer1/soccer1.obj)
	Model soccer = loadOBJ("resources/soccer1/soccer1.obj");
	soccerIndexCount = (unsigned int)soccer.indices.size();

	glGenVertexArrays(1, &soccerVAO);
	glGenBuffers(1, &soccerVBO);
	glGenBuffers(1, &soccerEBO);

	glBindVertexArray(soccerVAO);
	glBindBuffer(GL_ARRAY_BUFFER, soccerVBO);
	glBufferData(GL_ARRAY_BUFFER, soccer.vertices.size() * sizeof(Vertex), &soccer.vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, soccerEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, soccer.indices.size() * sizeof(unsigned int), &soccer.indices[0], GL_STATIC_DRAW);

	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	glEnableVertexAttribArray(0);
	// uv
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
	glEnableVertexAttribArray(1);
	// normal
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(2);

	// Load stadium model
	Model stadium = loadOBJ("resources/soccer_stadium/soccer_stadium.obj");
	stadiumIndexCount = (unsigned int)stadium.indices.size();
	stadiumMeshRanges = stadium.meshRanges;  // Store mesh ranges for multi-material rendering

	glGenVertexArrays(1, &stadiumVAO);
	glGenBuffers(1, &stadiumVBO);
	glGenBuffers(1, &stadiumEBO);

	glBindVertexArray(stadiumVAO);
	glBindBuffer(GL_ARRAY_BUFFER, stadiumVBO);
	glBufferData(GL_ARRAY_BUFFER, stadium.vertices.size() * sizeof(Vertex), &stadium.vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stadiumEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, stadium.indices.size() * sizeof(unsigned int), &stadium.indices[0], GL_STATIC_DRAW);

	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	glEnableVertexAttribArray(0);
	// uv
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
	glEnableVertexAttribArray(1);
	// normal
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(2);

	// Skybox
	float skyboxVertices[] = {
		// 位置坐标          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	std::vector<std::string> faces {
		"resources/skybox/posx.jpg", // 右（Right）
		"resources/skybox/negx.jpg", // 左（Left）
		"resources/skybox/posy.jpg", // 上（Top）
		"resources/skybox/negy.jpg", // 下（Bottom）
		"resources/skybox/posz.jpg", // 前（Front）
		"resources/skybox/negz.jpg"  // 后（Back）
	};
	skyboxTexture.setupCubeMap(faces);

}

// 辅助函数：根据材质渲染
void renderWithMaterial(const MeshRange& meshRange) {
	// 对NET（球网）材质禁用背面剔除以显示两面
	mainShader.setInt("twoSided", meshRange.materialName == "NET" ? 1 : 0);
	if (meshRange.materialName == "NET") {
		glDisable(GL_CULL_FACE);
	}
	
	if (materialInfoCache.find(meshRange.materialName) != materialInfoCache.end()) {
		const MaterialInfo& matInfo = materialInfoCache[meshRange.materialName];
		if (matInfo.texture != nullptr) {
			matInfo.texture->bind(0);
			mainShader.setInt("useTexture", 1);
		} else {
			stadiumDiff.bind(0);
			mainShader.setInt("useTexture", 0);
			mainShader.setVec3("baseColor", matInfo.color);
		}
	} else {
		stadiumDiff.bind(0);
		mainShader.setInt("useTexture", 0);
		mainShader.setVec3("baseColor", glm::vec3(0.8f, 0.8f, 0.8f));
	}
	
	// 恢复背面剔除
	if (meshRange.materialName == "NET") {
		glEnable(GL_CULL_FACE);
	}
}

void initializedGL(void) //run only once
{
	if (glewInit() != GLEW_OK) {
		std::cout << "GLEW not OK." << std::endl;
	}

	get_OpenGL_info();
	sendDataToOpenGL();

	// 确保足球初始位于空中并清零速度，以便从空中自由落体（不改变其他逻辑）
	// soccerPos.y = 3.0f;
	soccerVelocity = glm::vec3(0.0f);
	soccerHeld = false;

	//待做: set up the camera parameters	
	//待做: set up the vertex shader and fragment shader
	mainShader.setupShader("VertexShaderCode.glsl", "FragmentShaderCode.glsl");
	skyboxShader.setupShader("SkyboxVertexShader.glsl", "SkyboxFragmentShader.glsl");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);  // 启用背面剔除，但会在渲染NET时临时禁用

	// 初始化上一帧时间，避免首帧计算出过大的 deltaTime 导致物理数值异常（如被地面弹飞）
	lastFrame = (float)glfwGetTime();
}

void paintGL(void)  // 总是运行
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 深色背景
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// 更新企鹅运动（基于当前按键状态，支持同时移动和旋转）
	float penguinSpeed = 5.0f * deltaTime;
	float rotationSpeed = 200.0f * deltaTime;

	// 基于企鹅旋转计算前进方向
	glm::vec3 penguinForward = glm::vec3(
		sin(glm::radians(penguinRotation)),
		0.0f,
		cos(glm::radians(penguinRotation))
	);

	// 处理移动
	if (keysPressed[GLFW_KEY_UP])
		penguinPos += penguinForward * penguinSpeed;
	if (keysPressed[GLFW_KEY_DOWN])
		penguinPos -= penguinForward * penguinSpeed;

	// 处理旋转
	if (keysPressed[GLFW_KEY_LEFT])
		penguinRotation += rotationSpeed;
	if (keysPressed[GLFW_KEY_RIGHT])
		penguinRotation -= rotationSpeed;

	// 更新企鹅跳跃
	if (isJumping) {
		penguinVelocityY += GRAVITY * deltaTime;
		penguinPos.y += penguinVelocityY * deltaTime;
		if (penguinPos.y <= GROUND_Y) {
			penguinPos.y = GROUND_Y;
			isJumping = false;
			penguinVelocityY = 0.0f;
		}
	}

	// 足球交互系统
	float distToPenguin = glm::distance(penguinPos, soccerPos);
	
	// 碰撞检测：企鹅接近足球时拾取
	// 碰撞检测：企鹅接近足球时拾取（仅当足球未被持有且速度足够小时拾取）
	{
		const float SOCCER_PICKUP_VEL_THRESHOLD = 0.5f; // 小于此速度才认为球处于可拾起状态
		if (!soccerHeld && distToPenguin < SOCCER_HOLD_RADIUS && glm::length(soccerVelocity) < SOCCER_PICKUP_VEL_THRESHOLD) {
			soccerHeld = true;
			soccerVelocity = glm::vec3(0.0f);
		}
	}
	
	// 持有状态：足球跟随企鹅前方
	if (soccerHeld) {
		soccerPos = penguinPos + penguinForward * 0.1f;
		
		// 踢出：按K键释放足球，若企鹅在空中则给予向上初速度
		if (keysPressed[GLFW_KEY_K]) {
			// 只设置水平初速，垂直方向由重力驱动（自由落体）
			soccerVelocity = penguinForward * SOCCER_KICK_SPEED;
			soccerVelocity.y = 0.0f;
			soccerHeld = false;
		}
	}
	else {
		// 足球物理更新（自由运动）
		// 重力
		soccerVelocity.y += SOCCER_GRAVITY * deltaTime;
		// 水平阻尼（基于 deltaTime，使衰减与帧率无关）
		float horizFactor = 1.0f - SOCCER_HORIZONTAL_DRAG * deltaTime;
		if (horizFactor < 0.0f) horizFactor = 0.0f;
		soccerVelocity.x *= horizFactor;
		soccerVelocity.z *= horizFactor;
		// 位置积分
		soccerPos += soccerVelocity * deltaTime;

		// （已移除：与 stadiumMeshRanges 的 AABB 碰撞检测，保持其他逻辑不变）
		
		// 边界限制（球场范围）
		const float BOUND_XZ = 20.0f;
		const float GROUND_LEVEL = -3.115f;
		const float CEILING_LEVEL = 5.0f;
		
		// XZ平面边界
		if (soccerPos.x < -BOUND_XZ) {
			soccerPos.x = -BOUND_XZ;
			soccerVelocity.x *= -0.5f;  // 弹性碰撞
		}
		if (soccerPos.x > BOUND_XZ) {
			soccerPos.x = BOUND_XZ;
			soccerVelocity.x *= -0.5f;
		}
		if (soccerPos.z < -BOUND_XZ) {
			soccerPos.z = -BOUND_XZ;
			soccerVelocity.z *= -0.5f;
		}
		if (soccerPos.z > BOUND_XZ) {
			soccerPos.z = BOUND_XZ;
			soccerVelocity.z *= -0.5f;
		}
		
		// Y轴边界（垂直限制）
		if (soccerPos.y < GROUND_LEVEL) {
			soccerPos.y = GROUND_LEVEL;
			soccerVelocity.y *= -0.6f;  // 地面弹性较小
			// 当足球接触地面时，施加地面摩擦以更快减小水平速度
			float groundFactor = 1.0f - SOCCER_GROUND_FRICTION * deltaTime;
			if (groundFactor < 0.0f) groundFactor = 0.0f;
			soccerVelocity.x *= groundFactor;
			soccerVelocity.z *= groundFactor;
			// 若水平速度非常小，则直接置零以避免无限微滑
			if (glm::abs(soccerVelocity.x) < 0.02f) soccerVelocity.x = 0.0f;
			if (glm::abs(soccerVelocity.z) < 0.02f) soccerVelocity.z = 0.0f;
		}
		if (soccerPos.y > CEILING_LEVEL) {
			soccerPos.y = CEILING_LEVEL;
			soccerVelocity.y *= -0.3f;
		}
	}

	mainShader.use();

	// 照明信息
	// 使用方向光代表太阳光作为主光源
	// 点光源已注释掉
	// mainShader.setVec3("lightPos", 10000.0f, 10000.0f, 10000.0f); // 点光源已禁用
	// mainShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f); // 点光源已禁用
	
	mainShader.setVec3("viewPos", cameraPos);

	// 方向光作为主光源（太阳光）- 通过E/Q键控制亮度
	mainShader.setVec3("dirLightDirection", -0.05f, -0.3f, -0.1f); // 方向光，覆盖整个雪地
	mainShader.setVec3("dirLightColor", lightBrightness, lightBrightness, lightBrightness);

	// ambient 使用固定白天常数（T 现在只改变方向光亮度）
	float currentAmbientIntensity = ambientDay;
	glm::vec3 currentAmbient = glm::vec3(currentAmbientIntensity);
	float ambientIntensity = currentAmbientIntensity;

	// 聚光灯开关只由方向光亮度决定（小于阈值则开启）
	bool shouldEnableSpotlight = (lightBrightness < spotlightAutoThreshold);
	if (shouldEnableSpotlight && !spotlightEnabled) {
		spotlightBrightness = spotlightInitialBrightness;
	}
	spotlightEnabled = shouldEnableSpotlight;

	// 设置spotlight参数（聚光灯根据 spotlightEnabled 自动开/关）
	mainShader.setVec3("spotlight1Pos", spotlight1Pos);
	mainShader.setVec3("spotlight1Dir", spotlight1Dir);
	mainShader.setVec3("spotlight1Color", spotlight1Color * (spotlightEnabled ? spotlightBrightness : 0.0f));
	mainShader.setVec3("spotlight2Pos", spotlight2Pos);
	mainShader.setVec3("spotlight2Dir", spotlight2Dir);
	mainShader.setVec3("spotlight2Color", spotlight2Color * (spotlightEnabled ? spotlightBrightness : 0.0f));
	mainShader.setFloat("spotlightCutoff", spotlightCutoff);
	mainShader.setFloat("spotlightOuterCutoff", spotlightOuterCutoff);

	// ambient (day/night)
	mainShader.setVec3("ambientColor", currentAmbient);

	// 视图/投影
	glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	mainShader.setMat4("projection", projection);
	mainShader.setMat4("view", view);
	// 亮度数字叠加显示已关闭，对应的调试 uniform 传值也关闭
	// mainShader.setFloat("screenBrightness", lightBrightness);
	// mainShader.setFloat("spotBrightness", spotlightBrightness);
	// 默认：对纹理模型使用纹理
	mainShader.setInt("useTexture", 1);
	mainShader.setInt("twoSided", 0);

	// 渲染企鹅
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, penguinPos);
	model = glm::rotate(model, glm::radians(penguinRotation), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.0f / 3.0f));
	mainShader.setMat4("model", model);
	penguinDiff.bind(0);
	mainShader.setInt("useTexture", 1);
	mainShader.setInt("myTextureSampler", 0);
	glBindVertexArray(penguinVAO);
	glDrawElements(GL_TRIANGLES, penguinIndexCount, GL_UNSIGNED_INT, 0);

	// 用纹理渲染足球（使用动态位置）
	model = glm::mat4(1.0f);
	model = glm::translate(model, soccerPos);
	model = glm::scale(model, glm::vec3(0.08f));
	mainShader.setMat4("model", model);
	mainShader.setInt("useTexture", 1);
	soccerDiff.bind(0);
	mainShader.setInt("myTextureSampler", 0);
	glBindVertexArray(soccerVAO);
	glDrawElements(GL_TRIANGLES, soccerIndexCount, GL_UNSIGNED_INT, 0);

	// 渲染雪地：中心对齐球场中心（XZ），放在球场下方并放大 2 倍，同时增加厚度
	model = glm::mat4(1.0f);
	// 将雪地中心与球场中心在 XZ 对齐，Y 向下偏移以确保雪地位于球场下方
	model = glm::translate(model, glm::vec3(0.0f, -6.5f, 0.0f));
	// XZ 放大 2 倍（之前为30），厚度缩小为原来 1/20（6.0 / 20 = 0.3）
	model = glm::scale(model, glm::vec3(40.0f, 0.3f, 40.0f));
	mainShader.setMat4("model", model);
	// 确保为雪启用纹理
	mainShader.setInt("useTexture", 1);
	snowDiff.bind(0);
	mainShader.setInt("myTextureSampler", 0);
	glBindVertexArray(snowVAO);
	glDrawElements(GL_TRIANGLES, snowIndexCount, GL_UNSIGNED_INT, 0);

	// 渲染球场（以雪为中心，5倍缩放）
	model = glm::mat4(1.0f);
	// 用多材质纹理渲染球场
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, -5.0f, 0.0f));
	model = glm::scale(model, glm::vec3(5.0f));
	mainShader.setMat4("model", model);
	glBindVertexArray(stadiumVAO);

	// 直接使用全局定义的spotlight位置，不再每帧根据bordlight自动覆盖

	// 用对应的材质（纹理或颜色）渲染每个网格
	for (const auto& meshRange : stadiumMeshRanges) {
		renderWithMaterial(meshRange);
		glDrawElements(GL_TRIANGLES, meshRange.indexCount, GL_UNSIGNED_INT, 
			(void*)(meshRange.baseIndex * sizeof(unsigned int)));
	}

	/*
	// 调试用：在两个聚光灯原点绘制球形mesh，后续可直接删除
	mainShader.setInt("useTexture", 0);
	mainShader.setVec3("baseColor", glm::vec3(1.0f, 1.0f, 1.0f));
	mainShader.setInt("twoSided", 0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, spotlight1Pos);
	model = glm::scale(model, glm::vec3(0.12f));
	mainShader.setMat4("model", model);
	glBindVertexArray(soccerVAO);
	glDrawElements(GL_TRIANGLES, soccerIndexCount, GL_UNSIGNED_INT, 0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, spotlight2Pos);
	model = glm::scale(model, glm::vec3(0.12f));
	mainShader.setMat4("model", model);
	glBindVertexArray(soccerVAO);
	glDrawElements(GL_TRIANGLES, soccerIndexCount, GL_UNSIGNED_INT, 0);
	*/

	// 渲染天空盒
	glDepthFunc(GL_LEQUAL);
	skyboxShader.use();
	view = glm::mat4(glm::mat3(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp))); // 移除平移
	skyboxShader.setMat4("view", view);
	skyboxShader.setMat4("projection", projection);
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture.getID());
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS);
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// 设置当前窗口的鼠标按钮回调	
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		if (action == GLFW_PRESS) {
			middleMouseButtonPressed = true;
			firstMouse = true;
		}
		else if (action == GLFW_RELEASE) {
			middleMouseButtonPressed = false;
		}
	}
}

void cursor_position_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	// 设置当前窗口的光标位置回调
	if (!middleMouseButtonPressed) return;

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // 反转因为y坐标从下到上
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	// 确保当俯仰角超出范围时，屏幕不会翻转
	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// 设置当前窗口的滚动回调
	fov -= (float)yoffset;
	if (fov < 1.0f)
		fov = 1.0f;
	if (fov > 45.0f)
		fov = 45.0f;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// 更新按键状态
	if (action == GLFW_PRESS)
		keysPressed[key] = true;
	else if (action == GLFW_RELEASE)
		keysPressed[key] = false;

	// 相机移动
	float cameraSpeed = 160.0f * 4.0f * deltaTime;
	if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT))
		cameraPos += cameraSpeed * cameraFront;
	if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT))
		cameraPos -= cameraSpeed * cameraFront;
	if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT))
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

	// 纹理切换
	if (key == GLFW_KEY_1 && action == GLFW_PRESS)
		penguinDiff.setupTexture("resources/penguin/penguin_01.png");
	if (key == GLFW_KEY_2 && action == GLFW_PRESS)
		penguinDiff.setupTexture("resources/penguin/penguin_02.png");
	if (key == GLFW_KEY_3 && action == GLFW_PRESS)
		snowDiff.setupTexture("resources/snow/snow_01.jpg");
	if (key == GLFW_KEY_4 && action == GLFW_PRESS)
		snowDiff.setupTexture("resources/snow/snow_02.jpg");

	// 方向光亮度控制
	if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT))
		lightBrightness += 0.05f;
	if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT))
		lightBrightness -= 0.05f;
	if (lightBrightness < -2.0f) lightBrightness = -2.0f;

	// 聚光灯亮度控制: P增加, O减少
	if (key == GLFW_KEY_P && (action == GLFW_PRESS || action == GLFW_REPEAT))
		spotlightBrightness += 0.05f;
	if (key == GLFW_KEY_O && (action == GLFW_PRESS || action == GLFW_REPEAT))
		spotlightBrightness -= 0.05f;
	if (spotlightBrightness < 0.0f) spotlightBrightness = 0.0f;
	if (spotlightBrightness > 5.0f) spotlightBrightness = 5.0f;

	// 企鹅跳跃
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		if (!isJumping) {
			isJumping = true;
			penguinVelocityY = JUMP_FORCE;
		}
	}

	// 日/夜切换 (T键)
	if (key == GLFW_KEY_T && action == GLFW_PRESS) {
		isDayDirectional = !isDayDirectional;
		lightBrightness = isDayDirectional ? dirLightDayBrightness : dirLightNightBrightness;
	}

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}


int main(int argc, char* argv[])
{
	GLFWwindow* window;

	/* Initialize the glfw */
	if (!glfwInit()) {
		std::cout << "Failed to initialize GLFW" << std::endl;
		return -1;
	}
	/* glfw: configure; necessary for MAC */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Assignment 2", NULL, NULL);
	if (!window) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	/*注册回调函数*/
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetKeyCallback(window, key_callback);                                                                  //    
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);

	initializedGL();

	while (!glfwWindowShouldClose(window)) {
		/* 在此渲染 */
		paintGL();

		/* 交换前后缓冲区 */
		glfwSwapBuffers(window);

		/* 轮询并处理事件 */
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}
