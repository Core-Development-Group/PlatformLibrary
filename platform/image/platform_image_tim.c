/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/

#include "image_private.h"

/*  http://rewiki.regengedanken.de/wiki/.TIM
 *  https://mrclick.zophar.net/TilEd/download/timgfx.txt
 */

typedef struct TIMHeader {
    uint8_t flag1;
    uint8_t flag2;
    uint8_t flag3;
    uint8_t flag4;
} TIMHeader;

#define TIM_FLAG1_TYPE_MASK (1 | 2)
#define TIM_FLAG1_CLP       8

typedef struct TIMPaletteInfo {
    uint32_t palette_size;   /* Length of the palette header and pixel data, in bytes. */
    uint16_t palette_org_x;
    uint16_t palette_org_y;
    uint16_t palette_width;  /* "width" and "height" of palette, multiply these together to get the */
    uint16_t palette_height; /* ...length of the palette in 16-bit words. */
} TIMPaletteInfo;

typedef struct TIMImageInfo {
    uint32_t image_size; /* Length of image header and pixel data, in bytes */
    uint16_t org_x;
    uint16_t org_y;
    uint16_t width;      /* Length of each row, in 16-bit words */
    uint16_t height;     /* Height of the image, in pixels */
} TIMImageInfo;

enum TIMType {
    TIM_TYPE_4BPP   = 0,
    TIM_TYPE_8BPP   = 1,
    TIM_TYPE_16BPP  = 2,
    TIM_TYPE_24BPP  = 3,
};

#define TIM_IDENT   16

static bool TIM_FormatCheck(PLFile *fin) {
  plRewindFile(fin);

    uint32_t ident;
    if(plReadFile(fin, &ident, sizeof(uint32_t), 1) != 1) {
        return false;
    }

    return (bool)(ident == TIM_IDENT);
}

/* Convert a 16-bit TIM colour value to RGB5A1. */
static uint16_t _tim16toRGB51A(uint16_t colour_in) {
    uint16_t colour_out = 0;

    uint8_t *cin  = (uint8_t*)(&colour_in);
    uint8_t *cout = (uint8_t*)(&colour_out);

    /* Shift the colour bits around:
     * GGGRRRRR:ABBBBBGG => RRRRRGGG:GGBBBBBA
    */

    /* Red */
    cout[0] |= (cin[0] & 0x1F) << 3;

    /* Green */
    cout[0] |= (cin[1] & 0x03) << 1;
    cout[0] |= (cin[0] & 0x80) >> 7;
    cout[1] |= (cin[0] & 0x60) << 1;

    /* Blue */
    cout[1] |= (cin[1] & 0x7C) >> 1;

    /* Handle the alpha channel... if the "STP" bit in the TIM data is on, the colour is
     * transparent, unless the colour is black, in which case the bit is inverted.
     */

    bool is_black = (colour_out == 0);
    bool stp_on   = (cin[1] & 0x80);

    if((is_black && stp_on) || (!is_black && !stp_on)) {
        cout[1] |= 0x01;
    }

    return colour_out;
}

static bool TIM_ReadFile(PLFile *fin, PLImage *out) {
	if ( !TIM_FormatCheck( fin ) ) {
		ReportError( PL_RESULT_FILETYPE, "invalid/unexpected identifier for TIM" );
		return false;
	}

    uint16_t *palette = NULL;
    uint8_t *image_data  = NULL;

    TIMHeader header;
    if (plReadFile(fin, &header, sizeof(TIMHeader), 1) != 1) {
        goto UNEXPECTED_EOF;
    }

    uint32_t palette_size = 0; /* Number of colours in palette */

    if(header.flag1 & TIM_FLAG1_CLP) {
        /* File has a palette (CLUT), read it it in */

        TIMPaletteInfo palette_info;
        if(plReadFile(fin, &palette_info, sizeof(TIMPaletteInfo), 1) != 1) {
            goto UNEXPECTED_EOF;
        }

        palette_size = ((uint32_t)(palette_info.palette_width))
            * ((uint32_t)(palette_info.palette_height));

        /* Check the size and width/height values in the header match. */
        if(palette_size >= palette_info.palette_size
            || (palette_size * sizeof(uint16_t)) != (palette_info.palette_size - sizeof(palette_info)))
        {
            ReportError(PL_RESULT_FILETYPE, "invalid size/width/height in TIM palette header");
            goto ERR_CLEANUP;
        }

        palette = pl_calloc(palette_size, sizeof(uint16_t));
        if(palette == NULL) {
            goto ERR_CLEANUP;
        }

        if(plReadFile(fin, palette, sizeof(uint16_t), palette_size) != palette_size) {
            goto UNEXPECTED_EOF;
        }
    }

    TIMImageInfo image_info;
    if(plReadFile(fin, &image_info, sizeof(TIMImageInfo), 1) != 1) {
        goto UNEXPECTED_EOF;
    }

    /* Check the size and width/height values in the header match. */
    {
        uint32_t image_width_bytes = ((uint32_t)(image_info.width)) * 2;
        if(image_width_bytes >= image_info.image_size
            || (image_width_bytes * image_info.height) != (image_info.image_size - sizeof(image_info)))
        {
            ReportError(PL_RESULT_FILETYPE, "invalid size/width/height in TIM image header");
            goto ERR_CLEANUP;
        }
    }

    /* Read in the image data. */
    size_t image_data_len = image_info.image_size - sizeof(image_info);
    image_data            = pl_malloc(image_data_len);
    if(image_data == NULL) {
        goto ERR_CLEANUP;
    }

    if(plReadFile(fin, image_data, image_data_len, 1) != 1) {
        goto UNEXPECTED_EOF;
    }

    /* Prepare the metadata and image buffer in the PLImage structure. */

    uint8_t type = (uint8_t) (header.flag1 & TIM_FLAG1_TYPE_MASK);
    switch(type) {
        case TIM_TYPE_4BPP: {
            out->width = (unsigned int) (image_info.width * 4);
            out->height = image_info.height;
            out->format = PL_IMAGEFORMAT_RGB5A1;
        } break;

        case TIM_TYPE_8BPP: {
            out->width = (unsigned int) (image_info.width * 2);
            out->height = image_info.height;
            out->format = PL_IMAGEFORMAT_RGB5A1;
        } break;

        case TIM_TYPE_16BPP: {
            out->width = image_info.width;
            out->height = image_info.height;
            out->format = PL_IMAGEFORMAT_RGB5A1;
        } break;

        case TIM_TYPE_24BPP: {
            out->width = image_info.width / 1.5;
            out->height = image_info.height;
            out->format = PL_IMAGEFORMAT_RGB8;
        } break;

        default: {
            ReportError(PL_RESULT_IMAGEFORMAT, "invalid image format");
            goto ERR_CLEANUP;
        }
    }

    out->size   = plGetImageSize(out->format, out->width, out->height);
    out->levels = 1;

    out->data = pl_calloc(out->levels, sizeof(uint8_t*));
    if(out->data == NULL) {
        goto ERR_CLEANUP;
    }

    out->data[0] = pl_calloc(out->size, sizeof(uint8_t));
    if(out->data[0] == NULL) {
        goto ERR_CLEANUP;
    }

    /* Copy the image data into the PLImage buffer. */

    switch(type) {
        case TIM_TYPE_4BPP: {
            uint8_t  *indata  = image_data;
            uint16_t *outdata = (uint16_t*)(out->data[0]);

            for(; indata < (uint8_t*)(image_data + image_data_len); ++indata) {
                uint8_t p1 = (*indata & 0x0F);
                uint8_t p2 = (*indata & 0xF0) >> 4;

                if(p1 >= palette_size || p2 >= palette_size) {
                    ReportError(PL_RESULT_FILETYPE, "out-of-range palette index in TIM image");
                    goto ERR_CLEANUP;
                }

                *(outdata++) = _tim16toRGB51A(palette[p1]);
                *(outdata++) = _tim16toRGB51A(palette[p2]);
            }

            break;
        }

        case TIM_TYPE_8BPP: {
            uint8_t  *indata  = image_data;
            uint16_t *outdata = (uint16_t*)(out->data[0]);

            for(; indata < (uint8_t*)(image_data + image_data_len); ++indata) {
                uint8_t p = *indata;

                if(p >= palette_size) {
                    ReportError(PL_RESULT_FILETYPE, "out-of-range palette index in TIM image");
                    goto ERR_CLEANUP;
                }

                *(outdata++) = _tim16toRGB51A(palette[p]);
            }

            break;
        }

        case TIM_TYPE_16BPP: {
            uint8_t *indata  = image_data;
            uint8_t *outdata = out->data[0];

            for(; indata < (uint8_t*)(image_data + image_data_len); ++indata) {
                uint16_t colour = (uint16_t)((*indata) + ((*indata) << 8));
                *(outdata++) = (uint8_t) (colour);  //(_tim16toRGB51A(colour));
            }

            break;
        }

        case TIM_TYPE_24BPP:
        default:
            ReportError(PL_RESULT_IMAGEFORMAT, "unsupported tim type (%d)", type);
            goto ERR_CLEANUP;
    }

    out->colour_format = PL_COLOURFORMAT_ABGR;

    pl_free(image_data);
    pl_free(palette);

    return true;

    UNEXPECTED_EOF:
    ReportError(PL_RESULT_FILEREAD, "unexpected EOF when loading TIM file");

    ERR_CLEANUP:

    if(out->data != NULL) {
        pl_free(out->data[0]);
        pl_free(out->data);
    }

    pl_free(image_data);
    pl_free(palette);

    return false;
}

PLImage *plLoadTimImage( const char *path ) {
	PLFile *file = plOpenFile( path, false );
	if ( file == NULL ) {
		return NULL;
	}

	if ( !TIM_FormatCheck( file ) ) {
		plCloseFile( file );
		return NULL;
	}

	plRewindFile( file );

	PLImage *image = pl_calloc( 1, sizeof( PLImage ) );
	if ( !TIM_ReadFile( file, image ) ) {
		pl_free( image );
		image = NULL;
	}

	plCloseFile( file );

	return image;
}
