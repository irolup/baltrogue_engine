# TextComponent API Documentation

## Overview

The `TextComponent` is a powerful text rendering component for the Baltrogue Engine that supports both 3D world space and 2D screen space text rendering. It uses the STB TrueType library for font rendering and supports multiple fonts, sizes, colors, and alignments.

## Features

- **Dual Rendering Modes**: World space (3D) and screen space (UI-like)
- **Font Support**: TTF font loading with atlas generation
- **Multi-line Text**: Automatic line wrapping and spacing
- **Text Alignment**: Left, center, and right alignment
- **Color Support**: RGBA color with alpha transparency
- **Scale and Spacing**: Customizable text scale and line spacing
- **Camera Attachment**: When child of camera, text sticks to screen space
- **Cross-Platform**: Works on both Linux and PS Vita builds

## Quick Start

### Creating Text in Code

```cpp
// Create a text node
auto textNode = scene->createNode("MyText");
auto textComponent = textNode->addComponent<TextComponent>();

// Configure the text
textComponent->setText("Hello World!");
textComponent->setFontPath("assets/fonts/DroidSans.ttf");
textComponent->setFontSize(64.0f);
textComponent->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
textComponent->setRenderMode(TextRenderMode::WORLD_SPACE);
textComponent->setAlignment(TextAlignment::CENTER);
```

### Creating Text in Editor

1. **Create New Text Node**:
   - Right-click in scene hierarchy → Create → Text
   - Or use main menu: Create → Text

2. **Add Text Component to Existing Node**:
   - Select a node in the hierarchy
   - In Properties panel, click "Add Component" → "Text Component"

3. **Configure Text Properties**:
   - Use the Properties panel to edit text content, font, size, color, etc.

## API Reference

### Constructor

```cpp
TextComponent()
```
Creates a new TextComponent with default settings:
- Text: "Hello World"
- Font: "assets/fonts/DroidSans.ttf"
- Font Size: 64.0f
- Color: White (1, 1, 1, 1)
- Render Mode: WORLD_SPACE
- Alignment: LEFT

### Text Content

#### `void setText(const std::string& newText)`
Sets the text content to be rendered.

**Parameters:**
- `newText`: The text string to display

**Example:**
```cpp
textComponent->setText("Welcome to the Game!");
```

#### `const std::string& getText() const`
Gets the current text content.

**Returns:** Current text string

### Font Configuration

#### `void setFontPath(const std::string& path)`
Sets the path to the TTF font file.

**Parameters:**
- `path`: Path to the font file (e.g., "assets/fonts/DroidSans.ttf")

**Example:**
```cpp
textComponent->setFontPath("assets/fonts/ProggyClean.ttf");
```

#### `void setFontSize(float size)`
Sets the font size in pixels.

**Parameters:**
- `size`: Font size (8.0f to 200.0f recommended)

**Example:**
```cpp
textComponent->setFontSize(48.0f);
```

### Visual Properties

#### `void setColor(const glm::vec4& newColor)`
Sets the text color with alpha transparency.

**Parameters:**
- `newColor`: RGBA color (r, g, b, a) where values are 0.0f to 1.0f

**Example:**
```cpp
// Red text with 50% transparency
textComponent->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));
```

#### `void setScale(float newScale)`
Sets the overall scale of the text.

**Parameters:**
- `newScale`: Scale factor (0.1f to 10.0f recommended)

**Example:**
```cpp
textComponent->setScale(2.0f); // Double size
```

#### `void setLineSpacing(float spacing)`
Sets the spacing between lines for multi-line text.

**Parameters:**
- `spacing`: Line spacing multiplier (0.5f to 3.0f recommended)

**Example:**
```cpp
textComponent->setLineSpacing(1.5f); // 50% more spacing
```

### Rendering Modes

#### `void setRenderMode(TextRenderMode mode)`
Sets how the text is rendered in 3D space.

**Parameters:**
- `mode`: Either `TextRenderMode::WORLD_SPACE` or `TextRenderMode::SCREEN_SPACE`

**Modes:**
- **WORLD_SPACE**: Text is rendered in 3D world coordinates, affected by camera perspective
- **SCREEN_SPACE**: Text is rendered in screen coordinates, always facing the camera

**Example:**
```cpp
// For UI-like text that always faces the camera
textComponent->setRenderMode(TextRenderMode::SCREEN_SPACE);
```

### Text Alignment

#### `void setAlignment(TextAlignment align)`
Sets the text alignment.

**Parameters:**
- `align`: Either `TextAlignment::LEFT`, `TextAlignment::CENTER`, or `TextAlignment::RIGHT`

**Example:**
```cpp
textComponent->setAlignment(TextAlignment::CENTER);
```

### Utility Methods

#### `float getTextWidth() const`
Gets the width of the current text in world units.

**Returns:** Text width as float

#### `float getTextHeight() const`
Gets the height of the current text in world units.

**Returns:** Text height as float

#### `glm::vec2 calculateTextSize() const`
Calculates the size of the current text.

**Returns:** Text size as glm::vec2 (width, height)

## Advanced Configuration

### Font Atlas Settings

The TextComponent uses font atlases for efficient rendering. You can configure these settings:

```cpp
// These are set in the constructor and can be modified if needed
textComponent->atlasWidth = 512;        // Atlas texture width
textComponent->atlasHeight = 512;       // Atlas texture height
textComponent->charsToInclude = 95;     // Number of characters to include
textComponent->firstCharCodePoint = 32; // First ASCII character (space)
```

### Multi-line Text

The component automatically handles multi-line text:

```cpp
textComponent->setText("Line 1\nLine 2\nLine 3");
textComponent->setLineSpacing(1.2f); // 20% more spacing between lines
```

### Camera Attachment for UI Text

To create UI-like text that sticks to the camera:

```cpp
// Create a camera
auto cameraNode = scene->createNode("Camera");
auto cameraComponent = cameraNode->addComponent<CameraComponent>();

// Create text as child of camera
auto textNode = scene->createNode("UIText");
auto textComponent = textNode->addComponent<TextComponent>();
textComponent->setRenderMode(TextRenderMode::SCREEN_SPACE);
textComponent->setText("Health: 100");

// Attach to camera
cameraNode->addChild(textNode);
```

## Supported Fonts

The engine includes several fonts in `assets/fonts/`:

- **DroidSans.ttf**: Clean, readable sans-serif font
- **ProggyClean.ttf**: Monospace font, good for code
- **ProggyVector.ttf**: Vector-style monospace font

## Performance Considerations

- **Font Atlas Caching**: Fonts are cached after first load
- **Character Limit**: Default atlas includes 95 characters (ASCII 32-126)
- **Memory Usage**: Each font size creates a separate atlas
- **Rendering**: Text is batched for efficient rendering

## Troubleshooting

### Common Issues

1. **Text Not Appearing**:
   - Check if font file exists at specified path
   - Verify font size is reasonable (8-200)
   - Ensure text is not behind camera

2. **Font Loading Errors**:
   - Verify TTF file is valid
   - Check file path is correct
   - Ensure font file is included in build

3. **Performance Issues**:
   - Limit number of text components
   - Use appropriate font sizes
   - Consider using fewer characters in atlas

### Debug Information

The component provides debug information in the editor:
- Text bounds display in Properties panel
- Font atlas generation messages in console
- Component state in inspector

## Examples

### Basic 3D Text
```cpp
auto textNode = scene->createNode("3DText");
auto text = textNode->addComponent<TextComponent>();
text->setText("3D World Text");
text->setFontSize(32.0f);
text->setColor(glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
text->setRenderMode(TextRenderMode::WORLD_SPACE);
```

### UI Text
```cpp
auto uiTextNode = scene->createNode("UIText");
auto uiText = uiTextNode->addComponent<TextComponent>();
uiText->setText("Score: 1000");
uiText->setFontSize(24.0f);
uiText->setRenderMode(TextRenderMode::SCREEN_SPACE);
uiText->setAlignment(TextAlignment::LEFT);
```

### Multi-line Text
```cpp
auto multiTextNode = scene->createNode("MultiLineText");
auto multiText = multiTextNode->addComponent<TextComponent>();
multiText->setText("Line 1\nLine 2\nLine 3");
multiText->setLineSpacing(1.5f);
multiText->setAlignment(TextAlignment::CENTER);
```

## Integration with Scene System

The TextComponent integrates seamlessly with the engine's scene system:

- **Transform Support**: Text respects node transform (position, rotation, scale)
- **Hierarchy**: Can be parented to other nodes
- **Serialization**: Text properties are saved/loaded with scenes
- **Editor Support**: Full editor integration with property inspector

## Cross-Platform Support

The TextComponent works on both supported platforms:

- **Linux**: Uses OpenGL for rendering
- **PS Vita**: Uses Vita-specific rendering pipeline
- **Shaders**: Platform-specific shaders are automatically selected
- **Fonts**: Same font files work on both platforms
