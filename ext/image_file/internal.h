#include <ruby.h>

#include <assert.h>

RUBY_EXTERN VALUE rb_image_file_mImageFile;
RUBY_EXTERN VALUE rb_image_file_cImageFileImage;

#define mImageFile rb_image_file_mImageFile
#define cImageFileImage rb_image_file_cImageFileImage

typedef enum {
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID = -1,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32 = 0,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24 = 1,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565 = 4,
} rb_image_file_image_pixel_format_t;

void rb_image_file_Init_image_file_image(void);
