#include "ShapeTexture.h"

#include <Image.hpp>
#include <ImageTexture.hpp>

#include "gamefile.h"
#include "lcw.h"

using namespace godot;

void ShapeTexture::_register_methods() {
    register_method("load_from_mix", &ShapeTexture::load_from_mix);
}

ShapeTexture::ShapeTexture() {
}

ShapeTexture::~ShapeTexture() {
}

void ShapeTexture::_init() {
}

void* ShapeTexture::extract_shape(const void* buffer, int shape) {
    shp_header* block = (shp_header*)buffer;
    uint32_t offset;
    uint8_t* byte_buffer = (uint8_t*)buffer;

    int size = sizeof(uint16_t);

    if (!buffer || shape < 0 || shape >= block->num_shapes)
        return(nullptr);

    offset = block->offsets[shape];

    return(byte_buffer + 2 + offset);
}

uint8_t ShapeTexture::g_mouseShapeBuffer[65000];

#define MOUSE_MAX_WIDTH 64
#define MOUSE_MAX_HEIGHT 64

Image* ShapeTexture::decode_d2_shape(const void* buffer) {
    d2_shape_header* shape = (d2_shape_header*)buffer;
    uint8_t* byte_buffer = (uint8_t*)buffer;

    uint8_t* decmp_buff;
    uint8_t* lcw_buff;

    int width = shape->width;
    int height = shape->height;
    int uncompsz = shape->datasize;

    unsigned char palette[256][3];
    CNC_Get_Palette((unsigned char(&)[256][3])palette);

    PoolByteArray image_buffer;
    {
        image_buffer.resize(MOUSE_MAX_WIDTH * MOUSE_MAX_HEIGHT * 4);
        PoolByteArray::Write image_buffer_write = image_buffer.write();
        unsigned char* image_buffer_data = image_buffer_write.ptr();

        uint16_t frame_flags = shape->shape_type;

        if (width <= MOUSE_MAX_WIDTH && height <= MOUSE_MAX_HEIGHT) {
            //Flag bit 2 is flag for no compression on frame, decompress to
            //intermediate buffer if flag is clear
            if (!(frame_flags & 2)) {
                decmp_buff = g_mouseShapeBuffer;
                lcw_buff = reinterpret_cast<uint8_t*>(shape);
                frame_flags = shape->shape_type | 2;

                memcpy(decmp_buff, lcw_buff, sizeof(d2_shape_header));
                decmp_buff += sizeof(d2_shape_header);
                lcw_buff += sizeof(d2_shape_header);

                //Copies a small lookup table if it exists, probably not in RA.
                if (frame_flags & 1) {
                    memcpy(decmp_buff, lcw_buff, 16);
                    decmp_buff += 16;
                    lcw_buff += 16;
                }

                LCW_Uncomp(lcw_buff, decmp_buff, uncompsz);
                byte_buffer = g_mouseShapeBuffer;
            }

            if (frame_flags & 1) {
                // We don't need to handle this for now
            }
            else {
                uint8_t* data_start = byte_buffer + sizeof(d2_shape_header);
                int image_size = height * width;
                int run_len = 0;

                while (image_size) {
                    uint8_t current_byte = *data_start++;
                    if (current_byte) {
                        for (int component = 0; component < 3; component++) {
                            *image_buffer_data++ = palette[current_byte][component] << 2;
                        }
                        *image_buffer_data++ = 0xFF;
                        --image_size;
                        continue;
                    }

                    if (!image_size) {
                        break;
                    }

                    run_len = *data_start;
                    image_size -= run_len;
                    ++data_start;

                    while (run_len--) {
                        for (int component = 0; component < 3; component++) {
                            *image_buffer_data++ = 0;
                        }
                        *image_buffer_data++ = 0;
                    }
                }
            }
        }
    }

    image_buffer.resize(width * height * 4);

    Image* image = Image::_new();
    image->create_from_data(width, height, false, Image::FORMAT_RGBA8, image_buffer);
    image->resize((int64_t)width * 2, (int64_t)height * 2, Image::INTERPOLATE_NEAREST);

    return image;
}

bool ShapeTexture::load_from_mix(String filename) {
    char* name;
    name = filename.alloc_c_string();

    uint8_t* shp_data = (uint8_t*)GameMixFile::Retrieve(name);
    if (name != nullptr) godot::api->godot_free(name);

    shp_header* block = (shp_header*)shp_data;
    for (int i = 0; i < block->num_shapes; i++)
    {
        void* shape_data = extract_shape(shp_data, i);
        Image* image = decode_d2_shape(shape_data);
        ImageTexture* texture = ImageTexture::_new();
        texture->create_from_image(image, 0);
        this->set_frame_texture(i, texture);
    }

    this->set_frames(block->num_shapes);

    return true;
}
