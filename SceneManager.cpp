///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
	fruitsInitialized = false; // Setting default instance of fruits to false (bool) 

}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
 * DefineObjectMaterials()
 *
 * This method is used for configuring the various material
 * settings for all of the objects in the 3D scene.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	// Pineapple material
	OBJECT_MATERIAL pineappleMaterial;
	pineappleMaterial.tag = "pineapple";
	pineappleMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.1f);
	pineappleMaterial.ambientStrength = 0.3f;
	pineappleMaterial.diffuseColor = glm::vec3(0.8f, 0.8f, 0.4f);
	pineappleMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.4f);
	pineappleMaterial.shininess = 0.0f;
	m_objectMaterials.push_back(pineappleMaterial);

	// Strawberry material
	OBJECT_MATERIAL strawberryMaterial;
	strawberryMaterial.tag = "strawberries";
	strawberryMaterial.ambientColor = glm::vec3(0.8f, 0.8f, 0.8f);
	strawberryMaterial.ambientStrength = 0.3f;
	strawberryMaterial.diffuseColor = glm::vec3(1.0f, 0.2f, 0.2f);
	strawberryMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
	strawberryMaterial.shininess = 0.0f;

	// Blueberry material
	OBJECT_MATERIAL blueberryMaterial;
	blueberryMaterial.tag = "blueberry";
	blueberryMaterial.ambientColor = glm::vec3(0.05f, 0.05f, 0.2f);
	blueberryMaterial.ambientStrength = 0.2f;
	blueberryMaterial.diffuseColor = glm::vec3(0.1f, 0.1f, 0.7f);
	blueberryMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.8f);
	blueberryMaterial.shininess = 0.0;
	m_objectMaterials.push_back(blueberryMaterial);

	// Table material
	OBJECT_MATERIAL tableMaterial;
	tableMaterial.tag = "table";
	tableMaterial.ambientColor = glm::vec3(0.2f, 0.1f, 0.05f);
	tableMaterial.ambientStrength = 0.2f;
	tableMaterial.diffuseColor = glm::vec3(0.6f, 0.3f, 0.1f);
	tableMaterial.specularColor = glm::vec3(0.3f, 0.2f, 0.1f);
	tableMaterial.shininess = 16.0f;
	m_objectMaterials.push_back(tableMaterial);

	// Wall material
	OBJECT_MATERIAL wallMaterial;
	wallMaterial.tag = "wall";
	wallMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
	wallMaterial.ambientStrength = 0.3f;
	wallMaterial.diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
	wallMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
	wallMaterial.shininess = 4.0f;
	m_objectMaterials.push_back(wallMaterial);

	// Orange material
	OBJECT_MATERIAL orangeMaterial;
	orangeMaterial.tag = "orange";
	orangeMaterial.ambientColor = glm::vec3(0.2f, 0.1f, 0.0f);
	orangeMaterial.ambientStrength = 0.2f;
	orangeMaterial.diffuseColor = glm::vec3(1.0f, 0.5f, 0.0f);
	orangeMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
	orangeMaterial.shininess = 0.0f;
	m_objectMaterials.push_back(orangeMaterial);

	// Lemon material
	OBJECT_MATERIAL lemonMaterial;
	lemonMaterial.tag = "lemon";
	lemonMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.0f);
	lemonMaterial.ambientStrength = 0.2f;
	lemonMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 0.0f);
	lemonMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.4f);
	lemonMaterial.shininess = 0.0f;
	m_objectMaterials.push_back(lemonMaterial);

	// Apple material
	OBJECT_MATERIAL appleMaterial;
	appleMaterial.tag = "apple";
	appleMaterial.ambientColor = glm::vec3(0.1f, 0.0f, 0.0f);
	appleMaterial.ambientStrength = 0.2f;
	appleMaterial.diffuseColor = glm::vec3(0.8f, 0.1f, 0.1f);
	appleMaterial.specularColor = glm::vec3(0.7f, 0.7f, 0.7f);
	appleMaterial.shininess = 5.0f;
	m_objectMaterials.push_back(appleMaterial);

	// Pineapple leaf material
	OBJECT_MATERIAL pineappleLeafMaterial;
	pineappleLeafMaterial.tag = "pineappleleaf";
	pineappleLeafMaterial.ambientColor = glm::vec3(0.0f, 0.2f, 0.0f);
	pineappleLeafMaterial.ambientStrength = 0.2f;
	pineappleLeafMaterial.diffuseColor = glm::vec3(0.0f, 0.8f, 0.0f);
	pineappleLeafMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
	pineappleLeafMaterial.shininess = 0.0f;
	m_objectMaterials.push_back(pineappleLeafMaterial);

	// Transparent box material
	OBJECT_MATERIAL boxMaterial;
	boxMaterial.tag = "transparentBox";
	boxMaterial.ambientColor = glm::vec3(0.1f, 0.1f, 0.1f);
	boxMaterial.ambientStrength = 0.1f;
	boxMaterial.diffuseColor = glm::vec3(0.7f, 0.7f, 0.7f);
	boxMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	boxMaterial.shininess = 40.0f;
	m_objectMaterials.push_back(boxMaterial);
}
/***********************************************************
 *  SetupSceneLight()
 *
 *  This method is used for setting up the lighting in the 3D scene
 *  by configuring light sources with appropriate positions,
 *  colors, and intensities to enhance the visual quality of
 *  the rendered objects, including global ambient light
 ***********************************************************/
void SceneManager::SetupSceneLight()
{
	// Enable lighting in the shader
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	// Global ambient light: Soft, cool toned blue-gray light to fill shadows (slightly increased)
	m_pShaderManager->setVec3Value("globalAmbientColor", glm::vec3(0.10f, 0.12f, 0.18f));

	// Light Source 0: Very soft, cool main light (greatly reduced intensity)
	m_pShaderManager->setVec3Value("lightSources[0].position", glm::vec3(-5.0f, 1.0f, -2.0f));
	m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", glm::vec3(0.3f, 0.33f, 0.36f));
	m_pShaderManager->setVec3Value("lightSources[0].specularColor", glm::vec3(0.1f, 0.11f, 0.12f));
	m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 2.0f);
	m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 0.02f);

	// Light Source 1: Soft blue fill light (slightly increased)
	m_pShaderManager->setVec3Value("lightSources[1].position", glm::vec3(-5.0f, 3.0f, 5.0f));
	m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", glm::vec3(0.35f, 0.4f, 0.55f));
	m_pShaderManager->setVec3Value("lightSources[1].specularColor", glm::vec3(0.17f, 0.2f, 0.27f));
	m_pShaderManager->setFloatValue("lightSources[1].focalStrength", 3.5f);
	m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 0.03f);

	// Light Source 2: Very subtle warm accent light (greatly reduced intensity)
	m_pShaderManager->setVec3Value("lightSources[2].position", glm::vec3(8.0f, 10.0f, -2.0f));
	m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", glm::vec3(0.2f, 0.18f, 0.15f));
	m_pShaderManager->setVec3Value("lightSources[2].specularColor", glm::vec3(0.1f, 0.09f, 0.07f));
	m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 1.0f);
	m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 0.01f);

	// Light Source 3: Soft blue backlight (slightly increased)
	m_pShaderManager->setVec3Value("lightSources[3].position", glm::vec3(-6.0f, 4.0f, -8.0f));
	m_pShaderManager->setVec3Value("lightSources[3].diffuseColor", glm::vec3(0.25f, 0.3f, 0.4f));
	m_pShaderManager->setVec3Value("lightSources[3].specularColor", glm::vec3(0.12f, 0.15f, 0.2f));
	m_pShaderManager->setFloatValue("lightSources[3].focalStrength", 3.5f);
	m_pShaderManager->setFloatValue("lightSources[3].specularIntensity", 0.03f);
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// Enable blending for supporting transparent rendering
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadConeMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadTaperedCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	m_basicMeshes->LoadPyramid3Mesh();

	// creating unique texture patterns for fruits, wall, and table
	CreateGLTexture("textures\\Pineapple.jpg", "pineapple");
	CreateGLTexture("textures\\Strawberry.jpg", "strawberry");
	CreateGLTexture("textures\\Blueberry.jpg", "blueberry");
	CreateGLTexture("textures\\Apple.jpg", "apple");
	CreateGLTexture("textures\\Table.jpg", "table");
	CreateGLTexture("textures\\Wall.jpg", "wall");
	CreateGLTexture("textures\\Orange.jpg", "orange");
	CreateGLTexture("textures\\Lemon.jpg", "lemon");
	CreateGLTexture("textures\\Apple.jpg", "apple");
	CreateGLTexture("textures\\pineapple_leaf.jpg", "pineappleleaf)");

	BindGLTextures();

	// define the object materials
	DefineObjectMaterials();

	// set up the lights for the scene
	SetupSceneLight();
	
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	if (!fruitsInitialized) {
		// Strawberry box dimensions and position
		const float strawberryBoxWidth = 3.0f;
		const float strawberryBoxHeight = 1.2f;
		const float strawberryBoxDepth = 2.2f;
		const float strawberryBoxZPos = 0.5f;  // Moved back

		// Blueberry box dimensions and position
		const float blueberryBoxWidth = 2.5f;
		const float blueberryBoxHeight = 0.58f;
		const float blueberryBoxDepth = 1.5f;
		const float blueberryBoxZPos = 3.0f;  // Moved back

		// Constants
		const int strawberryLayers = 6;
		const float baseLayerHeight = 0.3f;
		const float layerHeightIncrement = 0.2f;
		const float maxRandomOffset = 0.05f;

		for (int layer = 0; layer < strawberryLayers; layer++) {
			float layerHeight = baseLayerHeight + (layer * layerHeightIncrement);
			int fruitsPerRow = 14 - layer;  // Decrease fruits per row as they go up

			for (int row = 0; row < fruitsPerRow; row++) {
				for (int col = 0; col < fruitsPerRow; col++) {
					FruitProperties strawberry;

					// Calculate base position
					float xPos = -4.0f - (strawberryBoxWidth / 2) + (col + 0.5f) * (strawberryBoxWidth / fruitsPerRow);
					float zPos = strawberryBoxZPos - (strawberryBoxDepth / 2) + (row + 0.5f) * (strawberryBoxDepth / fruitsPerRow);

					// Add random offsets for more natural piling
					float xOffset = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * maxRandomOffset;
					float yOffset = (static_cast<float>(rand()) / RAND_MAX) * layerHeightIncrement * 0.5f;
					float zOffset = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * maxRandomOffset;

					strawberry.position = glm::vec3(xPos + xOffset, layerHeight + yOffset, zPos + zOffset);

					// Adjust scale for more variation
					float sizeVariation = 0.85f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.3f));
					strawberry.scale = glm::vec3(0.2f * sizeVariation, 0.25f * sizeVariation, 0.2f * sizeVariation);

					// Limit rotation to more natural angles
					strawberry.rotation = glm::vec3(
						(static_cast<float>(rand()) / RAND_MAX - 0.5f) * 30.0f,  // -15 to 15 degrees
						static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 360.0f)),  // Full Y rotation
						(static_cast<float>(rand()) / RAND_MAX - 0.5f) * 30.0f   // -15 to 15 degrees
					);

					strawberries.push_back(strawberry);
				}
			}
		}

		// Generate properties for blueberries
		const int blueberryLayers = 5;
		const float angle = glm::radians(30.0f);
		for (int layer = 0; layer < blueberryLayers; layer++) {
			float layerHeight = 0.25f + (layer * 0.17f);
			int fruitsPerRow = 20 - layer;

			for (int row = 0; row < fruitsPerRow; row++) {
				for (int col = 0; col < fruitsPerRow; col++) {
					FruitProperties blueberry;
					float x = -3.25f - (blueberryBoxWidth / 2) + (col + 0.6f) * (blueberryBoxWidth / fruitsPerRow);
					float z = blueberryBoxZPos - (blueberryBoxDepth / 2) + (row + 0.5f) * (blueberryBoxDepth / fruitsPerRow);
					float rotatedX = (x + 3.25f) * cos(-angle) - (z - blueberryBoxZPos) * sin(-angle);
					float rotatedZ = (x + 3.25f) * sin(-angle) + (z - blueberryBoxZPos) * cos(-angle);
					blueberry.position = glm::vec3(
						-3.25f + rotatedX,
						layerHeight,
						blueberryBoxZPos + rotatedZ
					);
					float sizeVariation = 1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.4f));
					// Reduced overall size by 15%
					const float sizeReduction = 0.85f;
					blueberry.scale = glm::vec3(0.0446f * sizeVariation * sizeReduction,
						0.0595f * sizeVariation * sizeReduction,
						0.0446f * sizeVariation * sizeReduction);
					blueberry.rotation = glm::vec3(
						static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 360.0f)),
						static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 360.0f)),
						static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 360.0f))
					);
					blueberries.push_back(blueberry);
				}
			}
		}

		fruitsInitialized = true;
	}

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// Clear both color and depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Draw opaque objects first
	// Draw table
	scaleXYZ = glm::vec3(10.0f, 1.0f, 7.0f);
	positionXYZ = glm::vec3(-7.0f, 0.0f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("table");
	SetShaderTexture("table");
	SetTextureUVScale(5.0f, 5.5f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw backdrop
	scaleXYZ = glm::vec3(10.0f, 5.0f, 7.0f);
	XrotationDegrees = 90.0f;
	positionXYZ = glm::vec3(-7.0f, 7.0f, -5.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("wall");
	SetShaderTexture("wall");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw Spherical body of pineapple (more oblong)
	scaleXYZ = glm::vec3(1.55f, 2.3f, 1.55f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-7.4f, 2.4f, 1.6f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("pineapple");
	SetShaderTexture("pineapple");
	SetTextureUVScale(4.5f, 4.5f);
	m_basicMeshes->DrawSphereMesh();

	// Draw pineapple body head
	scaleXYZ = glm::vec3(1.0f, 2.0f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-7.4f, 2.25f, 1.6f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("pineapple");
	SetShaderTexture("pineapple");
	SetTextureUVScale(1.5f, 1.5f);
	m_basicMeshes->DrawTaperedCylinderMesh();

	// Draw pineapple leaves
	const int numLeaves = 20;  // Total number of leaves
	const int leafSegments = 9;  // Segments per leaf for a layered effect
	const float baseLeafHeight = 0.9f;
	const float baseLeafWidth = 1.8f;
	const float leafAngle = 245.0f;  // Slight angle from vertical

	for (int leaf = 0; leaf < numLeaves; ++leaf) {
		float rotationAngle = static_cast<float>(leaf) / numLeaves * 360.0f;

		for (int segment = 0; segment < leafSegments; ++segment) {
			float segmentScale = 1.0f - (0.15f * segment);  // Smaller towards the tip
			float segmentHeight = baseLeafHeight * segmentScale;

			glm::vec3 leafScale(
				baseLeafWidth * segmentScale,
				segmentHeight,
				baseLeafWidth * segmentScale
			);

			glm::vec3 leafPosition(
				-7.5f + 0.05f * std::cos(glm::radians(rotationAngle)),
				4.5f + (segment * segmentHeight * 0.9f),  // Stack vertically with slight overlap
				1.6f + 0.05f * std::sin(glm::radians(rotationAngle))
			);

			SetTransformations(
				leafScale,
				leafAngle,      // Slight angle from vertical
				rotationAngle,  // Rotation around pineapple center
				0.0f,           // No Z rotation
				leafPosition
			);

			SetShaderMaterial("pineappleleaf");
			SetShaderTexture("pineappleleaf");
			SetTextureUVScale(1.0f, 1.0f);
			m_basicMeshes->DrawPyramid3Mesh();
		}
	}


	// Draw orange
	scaleXYZ = glm::vec3(0.75f, 0.75f, 0.75f);  // Decreased size
	positionXYZ = glm::vec3(-8.2f, 0.8f, 3.4f);  // Moved to touch the pineapple
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("orange");
	SetShaderTexture("orange");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawSphereMesh();

	// Draw apple
	scaleXYZ = glm::vec3(0.55f, 0.5f, 0.55f);  // Reduced size
	positionXYZ = glm::vec3(-6.8f, 0.7f, 3.6f);  // Positioned between orange and lemon
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("apple");
	SetShaderTexture("apple");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawSphereMesh();

	// Draw lemon
	scaleXYZ = glm::vec3(0.5f, 0.5f, 0.65f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-5.6f, 0.6f, 2.8f);  // Slightly behind apple and to the right
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("lemon");
	SetShaderTexture("lemon");
	SetTextureUVScale(1.0f, 1.0f);
	m_basicMeshes->DrawSphereMesh();

	// Draw fruit first (opaque)
	// Draw strawberries
	for (const auto& strawberry : strawberries) {
		glm::vec3 adjustedScale = glm::vec3(
			strawberry.scale.x,
			strawberry.scale.y * 1.2f,
			strawberry.scale.z
		);
		glm::vec3 adjustedRotation = strawberry.rotation + glm::vec3(180.0f, 0.0f, 0.0f);
		SetTransformations(adjustedScale, adjustedRotation.x, adjustedRotation.y, adjustedRotation.z, strawberry.position);
		SetShaderTexture("strawberry");
		SetTextureUVScale(1.0f, 1.0f);
		m_basicMeshes->DrawConeMesh();
	}

	// Draw blueberries
	for (const auto& blueberry : blueberries) {
		SetTransformations(blueberry.scale, blueberry.rotation.x, blueberry.rotation.y, blueberry.rotation.z, blueberry.position);
		SetShaderMaterial("blueberry");
		SetShaderTexture("blueberry");
		m_basicMeshes->DrawSphereMesh();
	}

	// Now draw transparent boxes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	// Draw strawberry box (transparent)
	scaleXYZ = glm::vec3(3.2f, 1.2f, 2.4f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-4.0f, 0.6f, 0.5f);  // Updated Z position
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("transparentBox");
	SetShaderColor(1.0f, 1.0f, 1.0f, 0.3f);	//setting 30% opacity
	m_basicMeshes->DrawBoxMesh();

	// Draw blueberry box (transparent)
	scaleXYZ = glm::vec3(2.5f, 0.65f, 1.7f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 30.0f;  // Rotated 30 degrees
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-3.25f, 0.6f, 3.0f);  // Updated Z position
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderMaterial("transparentBox");
	SetShaderColor(1.0f, 1.0f, 1.0f, 0.3f);	//setting 30% opacity
	m_basicMeshes->DrawBoxMesh();

	// Restore default state
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	/****************************************************************/
}
