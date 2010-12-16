#include <ruby.h>

#include <assert.h>

#undef ARG_UNUSED
#ifdef __GNUC__
# define ARG_UNUSED __attribute__ ((unused))
#else
# define ARG_UNUSED
#endif

#ifndef HAVE_UINT16_T
# if SIZEOF_SHORT == 2
typedef unsigned short uint16_t;
# elif SIZEOF_INT == 2
typedef unsigned int uint16_t;
# elif SIZEOF_CHAR == 2
typedef unsigned char uint16_t;
# elif SIZEOF_LONG == 2
typedef unsigned long uint16_t;
# else
#  error needs 16-bit integer type
# endif
#endif

#ifndef HAVE_UINT32_T
# if SIZEOF_INT == 4
typedef unsigned int uint32_t;
# elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
# elif SIZEOF_SHORT == 4
typedef unsigned short uint32_t;
# elif SIZEOF_LONG_LONG == 4
typedef unsigned LONG_LONG uint32_t;
# else
#  error needs 32-bit integer type
# endif
#endif

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

rb_image_file_image_pixel_format_t rb_image_file_image_symbol_to_pixel_format(VALUE symbol);
VALUE rb_image_file_image_pixel_format_to_symbol(rb_image_file_image_pixel_format_t const pf);
VALUE rb_image_file_image_get_buffer(VALUE obj);

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
