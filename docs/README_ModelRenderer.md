# ModelRenderer Component

## Overview

The `ModelRenderer` component is a powerful rendering component that allows you to load and display 3D models in your game engine. It supports GLTF/GLB model formats and provides automatic texture loading, material management, and optimized binary model support for PSVita builds.

## Features

- **GLTF/GLB Support**: Load industry-standard 3D models
- **Automatic Texture Loading**: Supports diffuse, normal, and ARM textures
- **Material Management**: Automatic material creation from GLTF data
- **Vita Optimization**: Binary model format for lightweight Vita builds
- **Editor Integration**: Full inspector support with model browser
- **Scene Serialization**: Models are saved/loaded with scenes
- **Code Generation**: Models appear in generated game code for both platforms

## Usage

### Basic Usage

```cpp
// Add ModelRenderer component to a scene node
auto modelNode = scene->createNode("My Model");
auto modelRenderer = modelNode->addComponent<ModelRenderer>();

// Load a GLTF model
if (modelRenderer->loadModel("assets/models/my_model.gltf")) {
    std::cout << "Model loaded successfully!" << std::endl;
} else {
    std::cerr << "Failed to load model" << std::endl;
}
```

### Model Discovery

```cpp
// Discover available models in a directory
auto models = ModelRenderer::discoverModels("assets/models");
for (const auto& modelPath : models) {
    std::cout << "Found model: " << modelPath << std::endl;
}
```

### Shadow Configuration

```cpp
// Configure shadow properties
modelRenderer->setCastShadows(true);
modelRenderer->setReceiveShadows(true);
```

## API Reference

### Constructor/Destructor

```cpp
ModelRenderer();
virtual ~ModelRenderer();
```

### Model Loading

```cpp
// Load a model from file
bool loadModel(const std::string& modelPath);

// Unload current model
void unloadModel();
```

**Parameters:**
- `modelPath`: Path to the model file (supports .gltf, .glb, .bmodel)

**Returns:**
- `true` if model loaded successfully, `false` otherwise

### Model Properties

```cpp
// Get model information
const std::string& getModelPath() const;
const std::string& getModelName() const;
bool isModelLoaded() const;

// Get model data
const std::vector<std::shared_ptr<Mesh>>& getMeshes() const;
const std::vector<std::shared_ptr<Material>>& getMaterials() const;
```

### Shadow Properties

```cpp
// Configure shadow casting
bool getCastShadows() const;
void setCastShadows(bool cast);

// Configure shadow receiving
bool getReceiveShadows() const;
void setReceiveShadows(bool receive);
```

### Static Methods

```cpp
// Discover models in a directory
static std::vector<std::string> discoverModels(const std::string& directory = "assets/models");
```

**Parameters:**
- `directory`: Directory to search for models

**Returns:**
- Vector of model file paths found in the directory

## Supported File Formats

### Input Formats
- **GLTF** (.gltf): ASCII GLTF format
- **GLB** (.glb): Binary GLTF format
- **Binary Model** (.bmodel): Custom Vita-optimized format

### Model Requirements
- Models should be in GLTF 2.0 format
- Supports multiple meshes per model
- Supports multiple materials per model
- Automatic texture loading for:
  - Diffuse textures
  - Normal maps
  - ARM textures (Ambient Occlusion, Roughness, Metallic)

## Vita Optimization

For PSVita builds, the ModelRenderer automatically converts GLTF models to a lightweight binary format:

```cpp
// On Vita, models are automatically converted to binary format
// This happens transparently when loading GLTF models
modelRenderer->loadModel("assets/models/complex_model.gltf");
// Internally converts to binary format for optimal performance
```

### Binary Model Format

The binary model format (.bmodel) is optimized for Vita's memory constraints:
- Compressed vertex data
- Optimized index format
- Reduced texture data
- Faster loading times

## Editor Integration

### Inspector Panel

The ModelRenderer provides a comprehensive inspector panel:

- **Model Information**: Shows loaded model name and path
- **Mesh Count**: Displays number of meshes in the model
- **Material Count**: Shows number of materials
- **Shadow Settings**: Configure shadow casting/receiving
- **Model Browser**: Browse and load models from the assets directory
- **Load/Unload Controls**: Easy model management

### Model Browser

The editor includes a model browser that:
- Discovers models in the assets/models directory
- Shows available GLTF/GLB files
- Provides one-click model loading
- Supports nested directory structures

## Scene Serialization

### Saving Scenes

Models are automatically included when saving scenes:

```cpp
// Models are automatically serialized with the scene
sceneSerializer.saveSceneToFile(scene, "my_scene.json");
```

### Loading Scenes

Models are automatically loaded when loading scenes:

```cpp
// Models are automatically deserialized with the scene
auto scene = sceneSerializer.loadSceneFromFile("my_scene.json");
```

### JSON Structure

Models are saved in the following JSON structure:

```json
{
  "components": [
    {
      "type": "ModelRenderer",
      "modelPath": "assets/models/my_model.gltf",
      "modelName": "my_model.gltf",
      "isLoaded": true,
      "castShadows": true,
      "receiveShadows": true
    }
  ]
}
```

## Code Generation

### Generated Game Code

Models are automatically included in generated game code:

```cpp
// Generated in game_main.cpp and vita_main.cpp
auto modelNode = gameScene->createNode("Model 1");
auto modelRenderer = modelNode->addComponent<ModelRenderer>();
modelRenderer->loadModel("assets/models/my_model.gltf");
modelNode->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
modelNode->getTransform().setRotation(glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)));
modelNode->getTransform().setScale(glm::vec3(1.0f, 1.0f, 1.0f));
gameScene->getRootNode()->addChild(modelNode);
```

### Platform Support

- **Linux**: Full GLTF support with all features
- **PSVita**: Automatic binary conversion for optimal performance

## Performance Considerations

### Memory Usage

- Models are loaded into memory when `loadModel()` is called
- Use `unloadModel()` to free memory when models are no longer needed
- Binary models on Vita use significantly less memory than GLTF

### Loading Times

- GLTF models: Slower initial load, full feature support
- Binary models: Faster load, optimized for Vita constraints

### Rendering Performance

- Multiple meshes per model are rendered efficiently
- Materials are cached and reused
- Shadow casting/receiving can be configured per model

## Troubleshooting

### Common Issues

1. **Model Not Loading**
   - Check file path is correct
   - Ensure model is in GLTF 2.0 format
   - Verify file permissions

2. **Textures Not Loading**
   - Check texture paths in GLTF file
   - Ensure texture files exist
   - Verify texture format is supported

3. **Performance Issues**
   - Use binary models on Vita
   - Reduce model complexity
   - Optimize texture sizes

### Debug Information

The ModelRenderer provides detailed logging:

```cpp
// Enable debug output
std::cout << "ModelRenderer: Loading model: " << modelPath << std::endl;
std::cout << "ModelRenderer: Successfully loaded model" << std::endl;
std::cout << "ModelRenderer: Meshes: " << meshCount << std::endl;
std::cout << "ModelRenderer: Materials: " << materialCount << std::endl;
```

## Examples

### Complete Example

```cpp
#include "Components/ModelRenderer.h"

void setupModel() {
    // Create scene node
    auto modelNode = scene->createNode("Lemon Model");
    
    // Add ModelRenderer component
    auto modelRenderer = modelNode->addComponent<ModelRenderer>();
    
    // Load model
    if (modelRenderer->loadModel("assets/models/lemon_1k.gltf/lemon_1k.gltf")) {
        // Configure properties
        modelRenderer->setCastShadows(true);
        modelRenderer->setReceiveShadows(true);
        
        // Position model
        modelNode->getTransform().setPosition(glm::vec3(0, 0, 0));
        modelNode->getTransform().setScale(glm::vec3(0.5f, 0.5f, 0.5f));
        
        // Add to scene
        scene->getRootNode()->addChild(modelNode);
        
        std::cout << "Model loaded successfully!" << std::endl;
    } else {
        std::cerr << "Failed to load model" << std::endl;
    }
}
```

### Model Discovery Example

```cpp
void discoverAndLoadModels() {
    // Discover available models
    auto models = ModelRenderer::discoverModels("assets/models");
    
    for (const auto& modelPath : models) {
        std::cout << "Found model: " << modelPath << std::endl;
        
        // Create node for each model
        auto node = scene->createNode("Model: " + modelPath);
        auto renderer = node->addComponent<ModelRenderer>();
        
        if (renderer->loadModel(modelPath)) {
            // Position models in a line
            static int modelIndex = 0;
            node->getTransform().setPosition(glm::vec3(modelIndex * 2.0f, 0, 0));
            scene->getRootNode()->addChild(node);
            modelIndex++;
        }
    }
}
```

## Dependencies

- **TinyGLTF**: For GLTF model loading
- **GLM**: For mathematical operations
- **OpenGL**: For rendering
- **STB**: For image loading (via TinyGLTF)

## Version History

- **v1.0**: Initial implementation with GLTF support
- **v1.1**: Added binary model support for Vita
- **v1.2**: Enhanced editor integration and scene serialization
