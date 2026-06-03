/**
 *
 * @brief Texture loader using tyniobjectloader.
 * @author Ferran Barba, ESAT 2025-2026
 * @subject Engine Programming
 *
 */

#ifndef __TEXTURE_LOADER_HPP__
#define __TEXTURE_LOADER_HPP__ 1


#include <GLFW/glfw3.h>
#include <MentatEngine/JobSystem.hpp>
#include <../include/MentatEngine/OpenGL/glTexture.hpp>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __ANDROID__
struct AAssetManager;
#endif

namespace ME {
    struct LoadedTexture {
        LoadedTexture() : width(0), height(0), channels(0), mipmapsQuantity(0) {}

        LoadedTexture(LoadedTexture&& other) noexcept
            : pixels(std::move(other.pixels)),
            width(other.width),
            height(other.height),
            channels(other.channels),
            mipmapsQuantity(other.mipmapsQuantity)
        {
            other.width = 0;
            other.height = 0;
            other.channels = 0;
            other.mipmapsQuantity = 0;
        }

        LoadedTexture& operator=(LoadedTexture&& other) noexcept {
            if (this != &other) {
                pixels = std::move(other.pixels);
                width = other.width;
                height = other.height;
                channels = other.channels;
                mipmapsQuantity = other.mipmapsQuantity;

                other.width = 0;
                other.height = 0;
                other.channels = 0;
                other.mipmapsQuantity = 0;
            }
            return *this;
        }

        std::vector<unsigned char> pixels;
        int width;
        int height;
        int channels;
        int mipmapsQuantity;
    };

    class TextureLoader
    {
        public:

            struct PendingTexture {
                unsigned long entityID;
                std::future<ME::LoadedTexture> future;
                std::string path;
            };

            TextureLoader();
            ~TextureLoader();

            static unsigned int GenerateMipMaps(ME::LoadedTexture& tex, const std::string& filepath);

            /**
             * @brief Carga un archivo de imagen (png, jpg, etc.) en memoria.
             * @param filepath Ruta al archivo de imagen.
             * @param flipVertically Si se debe invertir la imagen en el eje Y (com�n para OpenGL).
             * @return Una estructura LoadedTexture con los datos de los p�xeles.
             */
            static LoadedTexture LoadTexture(const std::string& filepath, bool create_lods, bool flipVertically = true);

#ifdef __ANDROID__
            /**
             * @brief Sets the Android asset manager used to load textures from APK assets.
             * Call this once from the Android entry point before loading any scene.
             */
            static void SetAndroidAssetManager(AAssetManager* asset_manager);
#endif

            /**
             * @brief Adds a texture to the job system to be updated.
             * @param path The path of the new texture.
             * @param id The id of the entity to update.
             */
            static void AddTextureToPending(std::string path, unsigned long id);

            /**
             * @brief Checks the pending textures loading with the jobsystem to load them.
             */
            static void UpdatePendingTextures();

            static JobSystem* js_ref_;
            static std::vector<PendingTexture> pending_textures_;
            static std::vector<std::string> currently_loading_textures_;
            static std::unordered_map<std::string, std::vector<unsigned long>> pending_textures_callbacks_;
            static std::unordered_map<std::string, std::weak_ptr<glTexture>> uploaded_textures_;

#ifdef __ANDROID__
            static AAssetManager* android_asset_manager_;
#endif
    };
}


#endif //__TEXTURE_LOADER_HPP__