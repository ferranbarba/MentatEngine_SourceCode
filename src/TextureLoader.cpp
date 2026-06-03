#include <../include/MentatEngine/TextureLoader.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>
#include <filesystem>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#endif
#include <../include/MentatEngine/OpenGl/RenderizableComponent.hpp>
#include <../include/MentatEngine/ECSManager.hpp>
#include <../include/MentatEngine/StreamingManager.hpp>

namespace {
#ifdef __ANDROID__
    constexpr const char* kTextureLoaderLogTag = "MentatTextureLoader";

    std::string NormalizeAndroidAssetPath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');

        while (path.rfind("./", 0) == 0) {
            path.erase(0, 2);
        }

        const std::string assetsToken = "assets/";
        const std::size_t assetsPos = path.find(assetsToken);
        if (assetsPos != std::string::npos) {
            path = path.substr(assetsPos + assetsToken.size());
        }

        while (!path.empty() && path.front() == '/') {
            path.erase(path.begin());
        }

        return path;
    }

    bool ReadAndroidAsset(AAssetManager* assetManager, const std::string& path, std::vector<unsigned char>& outBytes) {
        if (assetManager == nullptr || path.empty()) {
            return false;
        }

        const std::string normalizedPath = NormalizeAndroidAssetPath(path);
        const std::string candidates[] = {
            normalizedPath,
            std::string("assets/") + normalizedPath
        };

        for (const std::string& candidate : candidates) {
            AAsset* asset = AAssetManager_open(assetManager, candidate.c_str(), AASSET_MODE_BUFFER);
            if (asset == nullptr) {
                continue;
            }

            const off_t size = AAsset_getLength(asset);
            if (size <= 0) {
                AAsset_close(asset);
                continue;
            }

            outBytes.resize(static_cast<std::size_t>(size));
            const int bytesRead = AAsset_read(asset, outBytes.data(), static_cast<std::size_t>(size));
            AAsset_close(asset);

            if (bytesRead == size) {
                __android_log_print(ANDROID_LOG_INFO, kTextureLoaderLogTag, "Loaded asset texture: %s", candidate.c_str());
                return true;
            }

            outBytes.clear();
        }

        __android_log_print(ANDROID_LOG_ERROR, kTextureLoaderLogTag, "Texture asset not found in APK: %s", path.c_str());
        return false;
    }
#endif
}


namespace ME {
    JobSystem* TextureLoader::js_ref_;
    std::vector<TextureLoader::PendingTexture> TextureLoader::pending_textures_;
    std::vector<std::string> TextureLoader::currently_loading_textures_;
    std::unordered_map<std::string, std::vector<unsigned long>> TextureLoader::pending_textures_callbacks_;
    std::unordered_map<std::string, std::weak_ptr<glTexture>> TextureLoader::uploaded_textures_;

#ifdef __ANDROID__
    AAssetManager* TextureLoader::android_asset_manager_ = nullptr;

    void TextureLoader::SetAndroidAssetManager(AAssetManager* asset_manager)
    {
        android_asset_manager_ = asset_manager;
    }
#endif

    TextureLoader::TextureLoader()
	{
	}

    TextureLoader::~TextureLoader()
    {
    }

    unsigned int TextureLoader::GenerateMipMaps(ME::LoadedTexture& tex, const std::string& filepath) {
        
        int mipCount = 0;
        int width = tex.width;
        int height = tex.height;
        int channels = tex.channels;

        std::filesystem::path p(filepath);
        std::string baseName = p.stem().string();
        std::string folderPath = "./tempTextures/" + baseName;

        if (std::filesystem::exists(folderPath) && std::filesystem::is_directory(folderPath)) {
            int w = width;
            int h = height;
            int count = 0;
            while (w > 1 || h > 1) {
                w = std::max(1, w / 2);
                h = std::max(1, h / 2);
                count++;
            }
            return count;
        }

        std::filesystem::create_directories(folderPath);

        std::vector<unsigned char> current = tex.pixels;

        int mipLevel = 1;
        int newWidth = std::max(1, width / 2);
        int newHeight = std::max(1, height / 2);

        std::vector<unsigned char> next(newWidth * newHeight * channels);

        stbir_resize_uint8(
            current.data(), width, height, 0,
            next.data(), newWidth, newHeight, 0,
            channels
        );

        current = std::move(next);
        width = newWidth;
        height = newHeight;

        while (width > 1 || height > 1)
        {
            std::string outPath = folderPath + "/" + baseName + "_mip" + std::to_string(mipLevel) + ".png";

            stbi_flip_vertically_on_write(true);
            // Guardar mip actual
            stbi_write_png(
                outPath.c_str(),
                width,
                height,
                channels,
                current.data(),
                width * channels
            );

            mipCount++;

            // Calcular siguiente mip
            int newWidth = std::max(1, width / 2);
            int newHeight = std::max(1, height / 2);

            std::vector<unsigned char> next(newWidth * newHeight * channels);

            stbir_resize_uint8(
                current.data(), width, height, 0,
                next.data(), newWidth, newHeight, 0,
                channels
            );

            current = std::move(next);
            width = newWidth;
            height = newHeight;
            mipLevel++;
        }

        // Guardar �ltimo nivel (1x1 si no se guard�)
        if (width == 1 && height == 1)
        {
            std::string outPath =
                folderPath + "/" + baseName + "_mip" + std::to_string(mipLevel) + ".png";

            stbi_flip_vertically_on_write(true);

            stbi_write_png(
                outPath.c_str(),
                width,
                height,
                channels,
                current.data(),
                width * channels
            );

            mipCount++;
        }

        return mipCount;
    }

    LoadedTexture TextureLoader::LoadTexture(const std::string& filepath, bool create_mipmaps, bool flipVertically) {
        LoadedTexture result;
        stbi_set_flip_vertically_on_load(flipVertically);

        int w = 0;
        int h = 0;
        int c = 0;
        unsigned char* data = nullptr;

#ifdef __ANDROID__
        std::vector<unsigned char> assetBytes;
        if (ReadAndroidAsset(android_asset_manager_, filepath, assetBytes)) {
            data = stbi_load_from_memory(
                assetBytes.data(),
                static_cast<int>(assetBytes.size()),
                &w,
                &h,
                &c,
                0
            );
        }
        else {
            data = stbi_load(filepath.c_str(), &w, &h, &c, 0);
        }
#else
        data = stbi_load(filepath.c_str(), &w, &h, &c, 0);
#endif

        if (!data) {
            printf("[TextureLoader] Error loading texture: %s\n", filepath.c_str());
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_ERROR, kTextureLoaderLogTag, "stbi failed for texture: %s", filepath.c_str());
#endif
            return result;
        }

        result.width = w;
        result.height = h;
        result.channels = c;
        result.pixels.assign(data, data + (w * h * c));

#ifdef __ANDROID__
        // Android assets are packaged inside the APK. The old mipmap generator writes
        // temporary files with std::filesystem, so disable that path on Android.
        result.mipmapsQuantity = 0;
#else
        if (create_mipmaps) {
            result.mipmapsQuantity = GenerateMipMaps(result, filepath);
        }
        else {
            result.mipmapsQuantity = 0;
        }
#endif

        printf("[TextureLoader] W: %d - H: %d - CHANNELS: %d - PIXELS: %zu\n", result.width, result.height, result.channels, result.pixels.size());

        stbi_image_free(data);
        return result;
    }

    void TextureLoader::AddTextureToPending(std::string path, unsigned long id)
    {
        for (auto& clm : currently_loading_textures_) {
            if (clm == path) {
                pending_textures_callbacks_[path].push_back(id);
                return;
            }
        }

#ifdef __ANDROID__
        bool createMipMaps = false;
#else
        bool createMipMaps = true;
#endif
        for (auto& upm : uploaded_textures_) {
            if (upm.first == path) {
                std::shared_ptr<glTexture> ptr = upm.second.lock();
                if (ptr) {
                    if (ptr->IsMipMapPending()) {
                        createMipMaps = false;
                        if (ptr->GetMipMap() != 0) { // ONLY GOES IN IF THE MIPMAP IS DIFFERENT THAN 0 (THE ORIGINAL ONE)
                            std::filesystem::path p(path);
                            std::string baseName = p.stem().string();
                            std::string mipmappath = "./tempTextures/" + baseName + "/" + baseName + "_mip" + std::to_string(ptr->GetMipMap()) + ".png";

                            auto task = [mipmappath, createMipMaps]() {
                                return ME::TextureLoader::LoadTexture(mipmappath, createMipMaps);
                            };

                            pending_textures_.push_back({ id, js_ref_->tEnqueue(std::move(task)), path });
                            return;
                        }
                        else { // 0 CASE (BACK TO ORIGINAL BUT WITHOUT ADDING TO CURRENTLY LOAD TEXTURES)
                            auto task = [path, createMipMaps]() {
                                return ME::TextureLoader::LoadTexture(path, createMipMaps);
                            };

                            pending_textures_.push_back({ id, js_ref_->tEnqueue(std::move(task)), path });
                            return;
                        }
                    }
                    else {
                        auto* renc = ME::ECS::GetComponent<RenderizableComponent>(id);
                        renc->SetTexture(ptr);
                        return;
                    }
                }
            }
        }

        auto task = [path, createMipMaps]() {
            return ME::TextureLoader::LoadTexture(path, createMipMaps);
        };

        currently_loading_textures_.push_back(path);
        pending_textures_.push_back({ id, js_ref_->tEnqueue(std::move(task)), path });
    }

    void TextureLoader::UpdatePendingTextures(){
        for (auto it = pending_textures_.begin(); it != pending_textures_.end(); ) {
            auto status = it->future.wait_for(std::chrono::seconds(0));

            if (status == std::future_status::ready) {
                ME::LoadedTexture data = it->future.get();

                if (auto* render = ECS::GetComponent<ME::RenderizableComponent>(it->entityID)) {

                    bool foundInCache = false;
                    for (auto& upt : uploaded_textures_) {
                        if (upt.first == it->path) {
                            std::shared_ptr<glTexture> ptr = upt.second.lock();
                            if (ptr) {
                                foundInCache = true;
                                if (ptr->IsMipMapPending()) {
                                    render->GetTexture()->UploadTexture(data.pixels.data(), data.width, 
                                        data.height, data.channels, ptr->GetMipMapsQuantity(), it->path);
                                }
                                else {
                                    render->SetTexture(ptr);
                                }
                                break;
                            }
                        }
                    }

                    // CREATE A NEW TEXTURE TO SUBSTITUTE THE PREVIOUS, SHARED PTR CHANGED AND RELEASED IF NO REF TO IT
                    if (!foundInCache) {
                        auto newTex = std::make_shared<glTexture>();
                        newTex->UploadTexture(data.pixels.data(), data.width, data.height, data.channels, data.mipmapsQuantity, it->path);
                        render->SetTexture(newTex);
                        uploaded_textures_.insert({ it->path, std::weak_ptr<glTexture>(newTex) });
                    
                        for (unsigned long entityID : pending_textures_callbacks_[it->path]) {
                            if (auto* other_render = ECS::GetComponent<ME::RenderizableComponent>(entityID)) {
                                other_render->SetTexture(render->GetTexture());
                            }
                        }

                        pending_textures_callbacks_.erase(it->path);
                        currently_loading_textures_.erase(std::remove(currently_loading_textures_.begin(), 
                            currently_loading_textures_.end(),  it->path), currently_loading_textures_.end());
                    }

                    ME::StreamingManager::UpdateEntityTexture(it->entityID);
                }
                else {
                    printf("[TextureLoader] RenderizableComponent not found with ID: %lu\n", it->entityID);
                }
                it = pending_textures_.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = uploaded_textures_.begin(); it != uploaded_textures_.end(); ) {
            if (it->second.expired()) {
                it = uploaded_textures_.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

