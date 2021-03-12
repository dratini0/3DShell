#include <cassert>
#include <cstring>

// BMP
#include <libnsbmp.h>

// GIF
#include <libnsgif.h>

// JPEG
#include <turbojpeg.h>

// PNG
#include <png.h>

#include "fs.h"
#include "log.h"
#include "sprites.h"
#include "textures.h"

#define MAX_IMAGE_BYTES (48 * 1024 * 1024)

C2D_Image file_icons[NUM_ICONS], icon_dir, icon_dir_dark, wifi_icons[4], \
    battery_icons[6], battery_icons_charging[6], icon_check, icon_uncheck, icon_check_dark, icon_uncheck_dark, \
    icon_radio_off, icon_radio_on, icon_radio_dark_off, icon_radio_dark_on, icon_toggle_on, icon_toggle_dark_on, \
    icon_toggle_off, dialog, options_dialog, properties_dialog, dialog_dark, options_dialog_dark, properties_dialog_dark, \
    icon_home, icon_home_dark, icon_home_overlay, icon_options, icon_options_dark, icon_options_overlay, \
    icon_settings, icon_settings_dark, icon_settings_overlay, icon_ftp, icon_ftp_dark, icon_ftp_overlay, \
    icon_sd, icon_sd_dark, icon_sd_overlay, icon_secure, icon_secure_dark, icon_secure_overlay, icon_search, \
    icon_nav_drawer, icon_actions, icon_back;

static const u32 BYTES_PER_PIXEL = 4;

namespace BMP {
    static void *bitmap_create(int width, int height, [[maybe_unused]] unsigned int state) {
        /* ensure a stupidly large (>50Megs or so) bitmap is not created */
        if ((static_cast<long long>(width) * static_cast<long long>(height)) > (MAX_IMAGE_BYTES/BYTES_PER_PIXEL))
            return nullptr;
        
        return std::calloc(width * height, BYTES_PER_PIXEL);
    }
    
    static unsigned char *bitmap_get_buffer(void *bitmap) {
        assert(bitmap);
        return static_cast<unsigned char *>(bitmap);
    }
    
    static size_t bitmap_get_bpp([[maybe_unused]] void *bitmap) {
        return BYTES_PER_PIXEL;
    }
    
    static void bitmap_destroy(void *bitmap) {
        assert(bitmap);
        std::free(bitmap);
    }
}

namespace GIF {
    static void *bitmap_create(int width, int height) {
        /* ensure a stupidly large bitmap is not created */
        if ((static_cast<long long>(width) * static_cast<long long>(height)) > (MAX_IMAGE_BYTES/BYTES_PER_PIXEL))
            return nullptr;
        
        return std::calloc(width * height, BYTES_PER_PIXEL);
    }
    
    static void bitmap_set_opaque([[maybe_unused]] void *bitmap, [[maybe_unused]] bool opaque) {
        assert(bitmap);
    }
    
    static bool bitmap_test_opaque([[maybe_unused]] void *bitmap) {
        assert(bitmap);
        return false;
    }
    
    static unsigned char *bitmap_get_buffer(void *bitmap) {
        assert(bitmap);
        return static_cast<unsigned char *>(bitmap);
    }
    
    static void bitmap_destroy(void *bitmap) {
        assert(bitmap);
        std::free(bitmap);
    }
    
    static void bitmap_modified([[maybe_unused]] void *bitmap) {
        assert(bitmap);
        return;
    }
}

namespace Textures {
    typedef enum ImageType {
        ImageTypeBMP,
        ImageTypeGIF,
        ImageTypeJPEG,
        ImageTypePNG,
        ImageTypeWEBP,
        ImageTypeOther
    } ImageType;

    static C2D_SpriteSheet spritesheet;
    static const u32 TRANSPARENT_COLOR = 0xFFFFFFFF;

    static Result ReadFile(const std::string &path, u8 **buffer, u64 *size) {
        Result ret = 0;
        Handle file;
        
        if (R_FAILED(ret = FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, path.c_str()), FS_OPEN_READ, 0))) {
            Log::Error("FSUSER_OpenFile(%s) failed: 0x%x\n", path.c_str(), ret);
            return ret;
        }
        
        if (R_FAILED(ret = FSFILE_GetSize(file, size))) {
            Log::Error("FSFILE_GetSize(%s) failed: 0x%x\n", path.c_str(), ret);
            FSFILE_Close(file);
            return ret;
        }
        
        *buffer = new u8[*size];
        u32 bytes_read = 0;
        
        if (R_FAILED(ret = FSFILE_Read(file, &bytes_read, 0, *buffer, static_cast<u32>(*size)))) {
            Log::Error("FSFILE_Read(%s) failed: 0x%x\n", path.c_str(), ret);
            FSFILE_Close(file);
            return ret;
        }
        
        if (bytes_read != static_cast<u32>(*size)) {
            Log::Error("bytes_read(%lu) does not match file size(%llu)\n", bytes_read, *size);
            FSFILE_Close(file);
            return -1;
        }
        
        FSFILE_Close(file);
        return 0;
    }
    
    static u32 GetNextPowerOf2(u32 v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return (v >= 64 ? v : 64);
    }

    static bool C3DTexToC2DImage(C2D_Image *texture, u32 width, u32 height, u8 *buf) {
        if (width >= 1024 || height >= 1024)
            return false;
        
        C3D_Tex *tex = new C3D_Tex[sizeof(C3D_Tex)];
        Tex3DS_SubTexture *subtex = new Tex3DS_SubTexture[sizeof(Tex3DS_SubTexture)];
        subtex->width = static_cast<u16>(width);
        subtex->height = static_cast<u16>(height);

        // RGBA -> ABGR
        for (u32 row = 0; row < subtex->width; row++) {
            for (u32 col = 0; col < subtex->height; col++) {
                u32 z = (row + col * subtex->width) * BYTES_PER_PIXEL;
                
                u8 r = *(u8 *)(buf + z);
                u8 g = *(u8 *)(buf + z + 1);
                u8 b = *(u8 *)(buf + z + 2);
                u8 a = *(u8 *)(buf + z + 3);
                
                *(buf + z) = a;
                *(buf + z + 1) = b;
                *(buf + z + 2) = g;
                *(buf + z + 3) = r;
            }
        }
        
        u32 w_pow2 = Textures::GetNextPowerOf2(subtex->width);
        u32 h_pow2 = Textures::GetNextPowerOf2(subtex->height);

        subtex->left = 0.f;
        subtex->top = 1.f;
        subtex->right = (subtex->width /static_cast<float>(w_pow2));
        subtex->bottom = (1.0 - (subtex->height / static_cast<float>(h_pow2)));

        C3D_TexInit(tex, static_cast<u16>(w_pow2), static_cast<u16>(h_pow2), GPU_RGBA8);
        C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
        
        std::memset(tex->data, 0, tex->size);
        
        for (u32 x = 0; x < subtex->width; x++) {
            for (u32 y = 0; y < subtex->height; y++) {
                u32 dst_pos = ((((y >> 3) * (w_pow2 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))) * BYTES_PER_PIXEL;
                u32 src_pos = (y * subtex->width + x) * BYTES_PER_PIXEL;
                std::memcpy(&(static_cast<u8 *>(tex->data))[dst_pos], &(static_cast<u8 *>(buf))[src_pos], BYTES_PER_PIXEL);
            }
        }
        
        C3D_TexFlush(tex);
        tex->border = TRANSPARENT_COLOR;
        C3D_TexSetWrap(tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);

        if (tex && subtex) {
            texture->tex = tex;
            texture->subtex = subtex;
            return true;
        }

        return false;
    }
    
    static bool LoadImageBMP(u8 **data, u64 *size, C2D_Image *texture) {
        bmp_bitmap_callback_vt bitmap_callbacks = {
            BMP::bitmap_create,
            BMP::bitmap_destroy,
            BMP::bitmap_get_buffer,
            BMP::bitmap_get_bpp
        };
        
        bmp_result code = BMP_OK;
        bmp_image bmp;
        bmp_create(&bmp, &bitmap_callbacks);
        
        code = bmp_analyse(&bmp, *size, *data);
        if (code != BMP_OK) {
            bmp_finalise(&bmp);
            return false;
        }
        
        code = bmp_decode(&bmp);
        if (code != BMP_OK) {
            if ((code != BMP_INSUFFICIENT_DATA) && (code != BMP_DATA_ERROR)) {
                bmp_finalise(&bmp);
                return false;
            }
            
            /* skip if the decoded image would be ridiculously large */
            if ((bmp.width * bmp.height) > 200000) {
                bmp_finalise(&bmp);
                return false;
            }
        }

        bool ret = Textures::C3DTexToC2DImage(texture, static_cast<u32>(bmp.width), static_cast<u32>(bmp.height), static_cast<u8 *>(bmp.bitmap));
        bmp_finalise(&bmp);
        return ret;
    }
    
    static bool LoadImageGIF(u8 **data, u64 *size, C2D_Image *texture) {
        gif_bitmap_callback_vt bitmap_callbacks = {
            GIF::bitmap_create,
            GIF::bitmap_destroy,
            GIF::bitmap_get_buffer,
            GIF::bitmap_set_opaque,
            GIF::bitmap_test_opaque,
            GIF::bitmap_modified
        };
        
        bool ret = false;
        gif_animation gif;
        gif_result code = GIF_OK;
        gif_create(&gif, &bitmap_callbacks);
        
        do {
            code = gif_initialise(&gif, *size, *data);
            if (code != GIF_OK && code != GIF_WORKING) {
                Log::Error("gif_initialise failed: %d\n", code);
                gif_finalise(&gif);
                return ret;
            }
        } while (code != GIF_OK);
        
        code = gif_decode_frame(&gif, 0);
        if (code != GIF_OK) {
            Log::Error("gif_decode_frame failed: %d\n", code);
            return false;
        }
        
        ret = Textures::C3DTexToC2DImage(texture, static_cast<u32>(gif.width), static_cast<u32>(gif.height), static_cast<u8 *>(gif.frame_image));
        gif_finalise(&gif);
        return ret;
    }

    static bool LoadImageJPEG(u8 **data, u64 *size, C2D_Image *texture) {
        tjhandle jpeg = tjInitDecompress();
        int width = 0, height = 0, jpegsubsamp = 0;
        tjDecompressHeader2(jpeg, *data, *size, &width, &height, &jpegsubsamp);
        u8 *buffer = new u8[width * height * BYTES_PER_PIXEL];
        tjDecompress2(jpeg, *data, *size, buffer, width, 0, height, TJPF_RGBA, TJFLAG_FASTDCT);
        bool ret = Textures::C3DTexToC2DImage(texture, static_cast<u32>(width), static_cast<u32>(height), buffer);
        tjDestroy(jpeg);
        delete[] buffer;
        return ret;
    }

    static bool LoadImagePNG(u8 **data, u64 *size, C2D_Image *texture) {
        bool ret = false;
        png_image image;
        std::memset(&image, 0, (sizeof image));
        image.version = PNG_IMAGE_VERSION;
        
        if (png_image_begin_read_from_memory(&image, *data, *size) != 0) {
            png_bytep buffer;
            image.format = PNG_FORMAT_RGBA;
            buffer = new png_byte[PNG_IMAGE_SIZE(image)];
            
            if (buffer != nullptr && png_image_finish_read(&image, nullptr, buffer, 0, nullptr) != 0) {
                ret = Textures::C3DTexToC2DImage(texture, image.width, image.height, buffer);
                delete[] buffer;
                png_image_free(&image);
            }
            else {
                if (buffer == nullptr)
                    png_image_free(&image);
                else
                    delete[] buffer;
            }
        }
        
        return ret;
    }
    
    ImageType GetImageType(const std::string &filename) {
        std::string ext = FS::GetFileExt(filename);
        
        if (!ext.compare(".BMP"))
            return ImageTypeBMP;
        else if (!ext.compare(".GIF"))
            return ImageTypeGIF;
        else if ((!ext.compare(".JPG")) || (!ext.compare(".JPEG")))
            return ImageTypeJPEG;
        else if (!ext.compare(".PNG"))
            return ImageTypePNG;
        else if (!ext.compare(".WEBP"))
            return ImageTypeWEBP;
            
        return ImageTypeOther;
    }

    bool LoadImageFile(const std::string &path, C2D_Image *texture) {
        bool ret = false;
        u8 *data = nullptr;
        u64 size = 0;
        
        if (R_FAILED(Textures::ReadFile(path, &data, &size))) {
            delete[] data;
            return ret;
        }
        
        // Resize to 1 initially. If the file is a GIF it will be resized accordingly.
        ImageType type = GetImageType(path);
        
        switch(type) {
            case ImageTypeBMP:
                ret = Textures::LoadImageBMP(&data, &size, texture);
                break;

            case ImageTypeGIF:
                ret = Textures::LoadImageGIF(&data, &size, texture);
                break;
            
            case ImageTypeJPEG:
                ret = Textures::LoadImageJPEG(&data, &size, texture);
                break;
                
            case ImageTypePNG:
                ret = Textures::LoadImagePNG(&data, &size, texture);
                break;
                
            default:
                break;
        }
        
        delete[] data;
        return ret;
    }

    void Init(void) {
        spritesheet = C2D_SpriteSheetLoad("romfs:/res/drawable/sprites.t3x");

        file_icons[0] = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_default_idx);
        file_icons[1] = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_type_compress_idx);
        file_icons[2] = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_type_image_idx);
        file_icons[3] = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_type_text_idx);
        icon_dir = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_folder_idx);
        icon_dir_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_fso_folder_dark_idx);
        icon_check = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_check_on_normal_idx);
        icon_check_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_check_on_normal_dark_idx);
        icon_uncheck = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_check_off_normal_idx);
        icon_uncheck_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_check_off_normal_dark_idx);
        dialog = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_dialog_idx);
        options_dialog = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_options_dialog_idx);
        properties_dialog = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_properties_dialog_idx);
        dialog_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_dialog_dark_idx);
        options_dialog_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_options_dialog_dark_idx);
        properties_dialog_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_properties_dialog_dark_idx);
        icon_radio_off = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_radio_off_normal_idx);
        icon_radio_on = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_radio_on_normal_idx);
        icon_radio_dark_off = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_radio_off_normal_dark_idx);
        icon_radio_dark_on = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_radio_on_normal_dark_idx);
        icon_toggle_on = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_toggle_on_normal_idx);
        icon_toggle_dark_on = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_toggle_on_normal_dark_idx);
        icon_toggle_off = C2D_SpriteSheetGetImage(spritesheet, sprites_btn_material_light_toggle_off_normal_idx);
        icon_home = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_home_idx);
        icon_home_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_home_dark_idx);
        icon_home_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_home_overlay_idx);
        icon_options = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_filesystem_idx);
        icon_options_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_filesystem_dark_idx);
        icon_options_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_filesystem_overlay_idx);
        icon_settings = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_settings_idx);
        icon_settings_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_settings_dark_idx);
        icon_settings_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_settings_overlay_idx);
        icon_ftp = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_remote_idx);
        icon_ftp_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_remote_dark_idx);
        icon_ftp_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_remote_overlay_idx);
        icon_sd = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_sdcard_idx);
        icon_sd_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_sdcard_dark_idx);
        icon_sd_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_sdcard_overlay_idx);
        icon_secure = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_secure_idx);
        icon_secure_dark = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_secure_dark_idx);
        icon_secure_overlay = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_secure_overlay_idx);
        icon_search = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_search_idx);
        icon_nav_drawer = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_navigation_drawer_idx);
        icon_actions = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_material_light_contextual_action_idx);
        icon_back = C2D_SpriteSheetGetImage(spritesheet, sprites_ic_arrow_back_normal_idx);
        wifi_icons[0] = C2D_SpriteSheetGetImage(spritesheet, sprites_stat_sys_wifi_signal_0_idx);
        wifi_icons[1] = C2D_SpriteSheetGetImage(spritesheet, sprites_stat_sys_wifi_signal_1_idx);
        wifi_icons[2] = C2D_SpriteSheetGetImage(spritesheet, sprites_stat_sys_wifi_signal_2_idx);
        wifi_icons[3] = C2D_SpriteSheetGetImage(spritesheet, sprites_stat_sys_wifi_signal_3_idx);
        battery_icons[0] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_20_idx);
        battery_icons[1] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_30_idx);
        battery_icons[2] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_60_idx);
        battery_icons[3] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_80_idx);
        battery_icons[4] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_90_idx);
        battery_icons[5] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_full_idx);
        battery_icons_charging[0] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_20_charging_idx);
        battery_icons_charging[1] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_30_charging_idx);
        battery_icons_charging[2] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_60_charging_idx);
        battery_icons_charging[3] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_80_charging_idx);
        battery_icons_charging[4] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_90_charging_idx);
        battery_icons_charging[5] = C2D_SpriteSheetGetImage(spritesheet, sprites_battery_full_charging_idx);
    }

    void Exit(void) {
        C2D_SpriteSheetFree(spritesheet);
    }
}
