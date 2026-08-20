#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>

// bool loadTexFromImageFile(const std::filesystem::path& path, GLuint& texID) {
//     int width, height, channels;
//     uint8_t* data = stbi_load(path.string().c_str(), &width, &height,
//     &channels, 0); if (!data) {
//         return false;
//     }

//     glGenTextures(1, &texID);
//     glBindTexture(GL_TEXTURE_2D, texID);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//     glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//     if (channels == 4) {
//         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
//         GL_UNSIGNED_BYTE, data);
//     }
//     stbi_image_free(data);

//     return true;
// }
