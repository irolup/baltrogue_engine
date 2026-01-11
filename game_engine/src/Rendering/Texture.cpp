 #include "Rendering/Texture.h"
#include <iostream>
#include <fstream>

#ifdef LINUX_BUILD
    #include <png.h>
    // Use STB Image from tinygltf (already included)
    #include "../../vendor/tinygltf/stb_image.h"
#else
    #include <psp2/io/fcntl.h>  // Vita SDK file I/O
    #include <psp2/io/stat.h>   // Vita SDK file stat
    #include <cstring>  // For memcpy
    #include <cstdint>  // For uint32_t, uint16_t
    // Use STB Image from tinygltf to avoid multiple definitions
    #include "../../vendor/tinygltf/stb_image.h"
#endif

namespace GameEngine {

Texture::Texture()
    : textureID(0), width(0), height(0), format(TextureFormat::RGBA)
{
}

Texture::~Texture() {
    if (textureID) {
        glDeleteTextures(1, &textureID);
    }
}

#ifndef LINUX_BUILD

bool Texture::loadSTBImage(const std::string& filepath) {
    this->filepath = filepath; // Store the filepath
    
    // Read file into memory first (STB Image needs this)
    SceUID fd = sceIoOpen(filepath.c_str(), SCE_O_RDONLY, 0);
    if (fd < 0) {
        std::cerr << "Failed to open image file: " << filepath << std::endl;
        return false;
    }
    
    SceOff fileSize = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    
    if (fileSize <= 0) {
        std::cerr << "Invalid file size: " << filepath << std::endl;
        sceIoClose(fd);
        return false;
    }
    
    unsigned char* fileData = new unsigned char[fileSize];
    int bytesRead = sceIoRead(fd, fileData, fileSize);
    sceIoClose(fd);
    
    if (bytesRead != fileSize) {
        std::cerr << "Failed to read complete file: " << filepath << std::endl;
        delete[] fileData;
        return false;
    }
    
    int channels;
    unsigned char* imageData = stbi_load_from_memory(fileData, fileSize, &width, &height, &channels, 0);
    delete[] fileData;
    
    if (!imageData) {
        std::cerr << "STB Image failed to load: " << filepath << " - " << stbi_failure_reason() << std::endl;
        return false;
    }
    
    if (channels == 3) {
        format = TextureFormat::RGB;
    } else if (channels == 4) {
        format = TextureFormat::RGBA;
    } else {
        std::cerr << "Unsupported channel count: " << channels << std::endl;
        stbi_image_free(imageData);
        return false;
    }
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum glFormat = (channels == 3) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    stbi_image_free(imageData);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Successfully loaded STB image: " << filepath << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
    return true;
}

#endif

#ifdef LINUX_BUILD
bool Texture::loadSTBImage(const std::string& filepath) {
    this->filepath = filepath;
    
    FILE* file = fopen(filepath.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open image file: " << filepath << std::endl;
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize <= 0) {
        std::cerr << "Invalid file size: " << filepath << std::endl;
        fclose(file);
        return false;
    }
    
    // Read file data
    unsigned char* fileData = new unsigned char[fileSize];
    if (fread(fileData, 1, fileSize, file) != static_cast<size_t>(fileSize)) {
        std::cerr << "Failed to read file data: " << filepath << std::endl;
        delete[] fileData;
        fclose(file);
        return false;
    }
    fclose(file);
    
    // Use STB Image to load the image
    int channels;
    unsigned char* imageData = stbi_load_from_memory(fileData, fileSize, &width, &height, &channels, 0);
    delete[] fileData; // Free file data
    
    if (!imageData) {
        std::cerr << "STB Image failed to load: " << filepath << " - " << stbi_failure_reason() << std::endl;
        return false;
    }
    
    // Determine format based on channel count
    if (channels == 4) {
        format = TextureFormat::RGBA;
    } else if (channels == 3) {
        format = TextureFormat::RGB;
    } else {
        std::cerr << "Unsupported channel count: " << channels << std::endl;
        stbi_image_free(imageData);
        return false;
    }
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    GLenum glFormat = (format == TextureFormat::RGBA) ? GL_RGBA : GL_RGB;
    GLenum internalFormat = glFormat;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, imageData);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    stbi_image_free(imageData);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Successfully loaded texture with STB Image: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    return true;
}
#endif

bool Texture::loadFromFile(const std::string& filepath) {
    this->filepath = filepath;
    
#ifdef LINUX_BUILD
    if (loadSTBImage(filepath)) {
        return true;
    }
    
    std::cout << "STB Image failed, trying PNG-specific loading for: " << filepath << std::endl;
    
    FILE* file = fopen(filepath.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open texture file: " << filepath << std::endl;
        return false;
    }
    
    unsigned char header[8];
    if (fread(header, 1, 8, file) != 8) {
        std::cerr << "Failed to read PNG header: " << filepath << std::endl;
        fclose(file);
        return false;
    }
    if (png_sig_cmp(header, 0, 8)) {
        std::cerr << "File is not a valid PNG: " << filepath << std::endl;
        fclose(file);
        return false;
    }
    
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "Failed to create PNG read struct" << std::endl;
        fclose(file);
        return false;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "Failed to create PNG info struct" << std::endl;
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        fclose(file);
        return false;
    }
    
    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "PNG error occurred while reading: " << filepath << std::endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(file);
        return false;
    }
    
    png_init_io(png_ptr, file);
    png_set_sig_bytes(png_ptr, 8);
    
    png_read_info(png_ptr, info_ptr);
    
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        std::cerr << "Invalid image dimensions: " << width << "x" << height << std::endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(file);
        return false;
    }
    
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    
    // Only add alpha channel for specific cases, not for RGB
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    
    png_read_update_info(png_ptr, info_ptr);
    
    png_bytep* row_pointers = new png_bytep[height];
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    
    if (rowbytes <= 0 || rowbytes > width * 4) {
        std::cerr << "Invalid rowbytes: " << rowbytes << std::endl;
        delete[] row_pointers;
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(file);
        return false;
    }
    
    for (int i = 0; i < height; i++) {
        row_pointers[i] = new png_byte[rowbytes];
        if (!row_pointers[i]) {
            std::cerr << "Failed to allocate memory for row " << i << std::endl;
            for (int j = 0; j < i; j++) {
                delete[] row_pointers[j];
            }
            delete[] row_pointers;
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            fclose(file);
            return false;
        }
    }
    
    png_read_image(png_ptr, row_pointers);
    
    int channels = png_get_channels(png_ptr, info_ptr);
    if (channels == 4) {
        format = TextureFormat::RGBA;
    } else if (channels == 3) {
        format = TextureFormat::RGB;
    } else {
        format = TextureFormat::RGB;
    }
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum glFormat = getGLFormat(format);
    GLenum internalFormat = glFormat;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, nullptr);
    
    for (int i = 0; i < height; i++) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, height - 1 - i, width, 1, glFormat, GL_UNSIGNED_BYTE, row_pointers[i]);
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glGenerateMipmap(GL_TEXTURE_2D);
    
    for (int i = 0; i < height; i++) {
        delete[] row_pointers[i];
    }
    delete[] row_pointers;
    
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(file);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Successfully loaded texture: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    return true;
#else
    std::string vitaPath = filepath;
    
    if (filepath.find("assets/textures/") != std::string::npos) {
        size_t lastSlash = filepath.find_last_of("/");
        if (lastSlash != std::string::npos) {
            vitaPath = "app0:/" + filepath.substr(lastSlash + 1);
        } else {
            vitaPath = "app0:/" + filepath;
        }
    } else if (filepath.find("app0:/") == std::string::npos) {
        vitaPath = "app0:/" + filepath;
    }
    
    std::cout << "Vita: Attempting to load texture from: " << vitaPath << std::endl;
    
    if (loadSTBImage(vitaPath)) {
        return true;
    }
    
    std::cerr << "STB Image loading failed for: " << vitaPath << std::endl;
    return false;
#endif
}

bool Texture::createEmpty(int w, int h, TextureFormat fmt) {
    width = w;
    height = h;
    format = fmt;
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum glFormat = getGLFormat(format);
    GLenum internalFormat = glFormat;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

bool Texture::createFromData(const void* data, int w, int h, TextureFormat fmt) {
    width = w;
    height = h;
    format = fmt;
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLenum glFormat = getGLFormat(format);
    GLenum internalFormat = glFormat;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glFormat, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

void Texture::bind(int textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setFilter(TextureFilter minFilter, TextureFilter magFilter) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getGLFilter(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getGLFilter(magFilter));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setWrap(TextureWrap wrapS, TextureWrap wrapT) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, getGLWrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, getGLWrap(wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::generateMipmaps() {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::shared_ptr<Texture> Texture::getWhiteTexture() {
    static std::shared_ptr<Texture> whiteTexture = nullptr;
    
    if (!whiteTexture) {
        whiteTexture = std::make_shared<Texture>();
        unsigned char whitePixel[4] = {255, 255, 255, 255};
        whiteTexture->createFromData(whitePixel, 1, 1, TextureFormat::RGBA);
    }
    
    return whiteTexture;
}

std::shared_ptr<Texture> Texture::getBlackTexture() {
    static std::shared_ptr<Texture> blackTexture = nullptr;
    
    if (!blackTexture) {
        blackTexture = std::make_shared<Texture>();
        unsigned char blackPixel[4] = {0, 0, 0, 255};
        blackTexture->createFromData(blackPixel, 1, 1, TextureFormat::RGBA);
    }
    
    return blackTexture;
}

std::shared_ptr<Texture> Texture::getErrorTexture() {
    static std::shared_ptr<Texture> errorTexture = nullptr;
    
    if (!errorTexture) {
        errorTexture = std::make_shared<Texture>();
        unsigned char errorPixels[16] = {
            255, 0, 255, 255,
            0, 0, 0, 255,
            0, 0, 0, 255,
            255, 0, 255, 255
        };
        errorTexture->createFromData(errorPixels, 2, 2, TextureFormat::RGBA);
    }
    
    return errorTexture;
}

GLenum Texture::getGLFormat(TextureFormat fmt) const {
    switch (fmt) {
        case TextureFormat::RGB: return GL_RGB;
        case TextureFormat::RGBA: return GL_RGBA;
        case TextureFormat::DEPTH_COMPONENT: return GL_DEPTH_COMPONENT;
        case TextureFormat::DEPTH_STENCIL: return GL_DEPTH24_STENCIL8;
        default: return GL_RGBA;
    }
}

GLenum Texture::getGLFilter(TextureFilter filter) const {
    switch (filter) {
        case TextureFilter::NEAREST: return GL_NEAREST;
        case TextureFilter::LINEAR: return GL_LINEAR;
        case TextureFilter::NEAREST_MIPMAP_NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
        case TextureFilter::LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
        case TextureFilter::NEAREST_MIPMAP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
        case TextureFilter::LINEAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
        default: return GL_LINEAR;
    }
}

GLenum Texture::getGLWrap(TextureWrap wrap) const {
    switch (wrap) {
        case TextureWrap::REPEAT: return GL_REPEAT;
        case TextureWrap::MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
        case TextureWrap::CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
        case TextureWrap::CLAMP_TO_BORDER: return GL_CLAMP_TO_EDGE;
        default: return GL_REPEAT;
    }
}

} // namespace GameEngine
