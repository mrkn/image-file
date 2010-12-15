#include <ruby.h>

#include <assert.h>


RUBY_EXTERN VALUE rb_image_file_mImageFile;
RUBY_EXTERN VALUE rb_image_file_cImageFileImage;
RUBY_EXTERN VALUE rb_image_file_cImageFileJpegReader;
RUBY_EXTERN VALUE rb_image_file_eImageFileJpegReaderError;

#define mImageFile rb_image_file_mImageFile
#define cImageFileImage rb_image_file_cImageFileImage
#define cImageFileJpegReader rb_image_file_cImageFileJpegReader
#define eImageFileJpegReaderError rb_image_file_eImageFileJpegReaderError

typedef enum {
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID = -1,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32 = 0,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24 = 1,
    RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565 = 4,
} rb_image_file_image_pixel_format_t;

void rb_image_file_Init_image_file_image(void);
void rb_image_file_Init_image_file_jpeg_reader(void);

static inline int
file_p(VALUE fname)
{
    ID id_file_p;
    CONST_ID(id_file_p, "file?");
    return RTEST(rb_funcall(rb_cFile, id_file_p, 1, fname));
}

static inline int
check_file_not_found(VALUE fname)
{
    if (!file_p(fname)) {
	rb_raise(rb_eArgError, "file not found");
    }
}
