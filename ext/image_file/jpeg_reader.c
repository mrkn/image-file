#include "internal.h"

#undef EXTERN
#include <jpeglib.h>
#include <jerror.h>

static size_t const INPUT_BUFFER_SIZE = 4096U;

VALUE cImageFileJpegReader = Qnil;
VALUE eImageFileJpegReaderError = Qnil;

static ID id_GRAYSCALE;
static ID id_RGB;
static ID id_YCbCr;
static ID id_CMYK;
static ID id_YCCK;
static ID id_ISLOW;
static ID id_IFAST;
static ID id_FLOAT;
static ID id_NONE;
static ID id_ORDERED;
static ID id_FS;
static ID id_new;
static ID id_close;
static ID id_read;
static ID id_pixel_format;
static ID id_width;
static ID id_height;
static ID id_row_stride;

static inline char const*
j_color_space_name(J_COLOR_SPACE const color_space)
{
    switch (color_space) {
	case JCS_GRAYSCALE:
	    return "JCS_GRAYSCALE";

	case JCS_RGB:
	    return "JCS_RGB";

	case JCS_UNKNOWN:
	    return "JCS_UNKNOWN";

	case JCS_YCbCr:
	    return "JCS_YCbCr";

	case JCS_CMYK:
	    return "JCS_CMYK";

	case JCS_YCCK:
	    return "JCS_YCCK";

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return "(unknown color space)";
}

static rb_image_file_image_pixel_format_t
j_color_space_to_image_pixel_format(J_COLOR_SPACE const color_space)
{
    switch (color_space) {
	case JCS_GRAYSCALE:
	case JCS_RGB:
	    return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24;

	case JCS_UNKNOWN:
	case JCS_YCbCr:
	case JCS_CMYK:
	case JCS_YCCK:
	    return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID;

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID;
}

static J_COLOR_SPACE
image_pixel_format_to_j_color_space(rb_image_file_image_pixel_format_t const pf)
{
    switch (pf) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID:
	    return JCS_UNKNOWN;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    return JCS_RGB;

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return JCS_UNKNOWN;
}

static VALUE
color_space_to_symbol(J_COLOR_SPACE const color_space)
{
    switch (color_space) {
	case JCS_UNKNOWN:
	    return Qnil;

	case JCS_GRAYSCALE:
	    return ID2SYM(id_GRAYSCALE);

	case JCS_RGB:
	    return ID2SYM(id_RGB);

	case JCS_YCbCr:
	    return ID2SYM(id_YCbCr);

	case JCS_CMYK:
	    return ID2SYM(id_CMYK);

	case JCS_YCCK:
	    return ID2SYM(id_YCCK);

	default:
	    break;
    }
    assert(0); /* MUST NOT REACH HERE */
    return Qnil;
}

static J_COLOR_SPACE
id_to_color_space(ID const id)
{
    if (id == id_GRAYSCALE)
	return JCS_GRAYSCALE;
    if (id == id_RGB)
	return JCS_RGB;
    if (id == id_YCbCr)
	return JCS_YCbCr;
    if (id == id_CMYK)
	return JCS_CMYK;
    if (id == id_YCCK)
	return JCS_YCCK;
    return JCS_UNKNOWN;
}

static VALUE
dct_method_to_symbol(J_DCT_METHOD const dct_method)
{
    switch (dct_method) {
	case JDCT_ISLOW:
	    return ID2SYM(id_ISLOW);

	case JDCT_IFAST:
	    return ID2SYM(id_IFAST);

	case JDCT_FLOAT:
	    return ID2SYM(id_FLOAT);

	default:
	    break;
    }
    assert(0); /* DO NOT REACH HERE */
    return Qnil;
}

static VALUE
dither_mode_to_symbol(J_DITHER_MODE const dither_mode)
{
    switch (dither_mode) {
	case JDITHER_NONE:
	    return ID2SYM(id_NONE);

	case JDITHER_ORDERED:
	    return ID2SYM(id_ORDERED);

	case JDITHER_FS:
	    return ID2SYM(id_FS);

	default:
	    break;
    }
    assert(0); /* DO NOT REACH HERE */
    return Qnil;
}

enum jpeg_reader_state {
    READER_ALLOCATED = 0,
    READER_INITIALIZED,
    READER_RED_HEADER,
    READER_STARTED_DECOMPRESS,
    READER_FINISHED_DECOMPRESS
};

struct jpeg_reader_data {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr error;
    VALUE source;
    VALUE buffer;
    enum jpeg_reader_state state;
    unsigned close_source: 1;
    unsigned start_of_file: 1;
};

static void
jpeg_reader_mark(void* ptr)
{
    struct jpeg_reader_data* reader = (struct jpeg_reader_data*)ptr;
    rb_gc_mark(reader->source);
    rb_gc_mark(reader->buffer);
}

static void
jpeg_reader_free(void* ptr)
{
    struct jpeg_reader_data* reader = (struct jpeg_reader_data*)ptr;

#if 0
    if (reader->close_source && rb_respond_to(reader->source, id_close)) {
	rb_funcall(reader->source, id_close, 0);
    }
#endif
    reader->source = Qnil;
    jpeg_destroy_decompress(&reader->cinfo);
    xfree(ptr);
}

static size_t
jpeg_reader_memsize(void const* ptr)
{
    return ptr ? sizeof(struct jpeg_reader_data) : 0;
}

static rb_data_type_t const jpeg_reader_data_type = {
    "image_file::jpeg_reader",
#if RUBY_VERSION >= 193
    {
#endif
	jpeg_reader_mark,
	jpeg_reader_free,
	jpeg_reader_memsize,
#if RUBY_VERSION >= 193
    },
#endif
};

static VALUE
jpeg_reader_alloc(VALUE klass)
{
    struct jpeg_reader_data* reader;
    VALUE obj = TypedData_Make_Struct(
	    klass, struct jpeg_reader_data, &jpeg_reader_data_type, reader);
    reader->source = Qnil;
    reader->buffer = Qnil;
    reader->state = READER_ALLOCATED;
    reader->close_source = 0;
    reader->start_of_file = 0;
    return obj;
}

static struct jpeg_reader_data*
get_jpeg_reader_data(VALUE obj)
{
    struct jpeg_reader_data* reader;
    TypedData_Get_Struct(obj, struct jpeg_reader_data, &jpeg_reader_data_type, reader);
    return reader;
}

static void
error_exit(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX];
    (* cinfo->err->format_message)(cinfo, message);
    rb_raise(eImageFileJpegReaderError, "%s", message);
}

static void
output_message(j_common_ptr cinfo)
{
    char message[JMSG_LENGTH_MAX];
    (* cinfo->err->format_message)(cinfo, message);
    rb_warning("%s", message);
}

static struct jpeg_error_mgr*
init_error_mgr(struct jpeg_error_mgr* err)
{
    jpeg_std_error(err);
    err->error_exit = error_exit;
    err->output_message = output_message;
    return err;
}

static void
init_source(j_decompress_ptr cinfo)
{
    VALUE obj;
    struct jpeg_reader_data* reader;

    assert(cinfo != NULL);

    obj = (VALUE)cinfo->client_data;
    reader = get_jpeg_reader_data(obj);
    reader->start_of_file = 1;
}

static boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
    VALUE obj;
    struct jpeg_reader_data* reader;

    assert(cinfo != NULL);

    obj = (VALUE)cinfo->client_data;
    reader = get_jpeg_reader_data(obj);

    reader->buffer = rb_funcall(reader->source, id_read, 1, INT2FIX(INPUT_BUFFER_SIZE));
    if (NIL_P(reader->buffer)) { /* EOF */
	if (reader->start_of_file) { /* empty file */
	    ERREXIT(cinfo, JERR_INPUT_EMPTY);
	}
	WARNMS(cinfo, JWRN_JPEG_EOF);
	/* Insert fake EOI marker */
	reader->buffer = rb_str_tmp_new(2);
	RSTRING_PTR(reader->buffer)[0] = (JOCTET) 0xFF;
	RSTRING_PTR(reader->buffer)[1] = (JOCTET) JPEG_EOI;
    }

    cinfo->src->next_input_byte = (JOCTET const*)RSTRING_PTR(reader->buffer);
    cinfo->src->bytes_in_buffer = RSTRING_LEN(reader->buffer);
    reader->start_of_file = 0;

    return TRUE;
}

static void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    if (num_bytes > 0) {
	while (num_bytes > (long)cinfo->src->bytes_in_buffer) {
	    num_bytes -= (long)cinfo->src->bytes_in_buffer;
	    (* cinfo->src->fill_input_buffer)(cinfo);
	}
	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
    }
}

void
term_source(j_decompress_ptr cinfo ARG_UNUSED)
{
    /* nothing to do */
}

static void
init_source_mgr(struct jpeg_reader_data* reader)
{
    struct jpeg_source_mgr* src = NULL;

    assert(reader != NULL);

    if (reader->cinfo.src == NULL) {
	reader->cinfo.src = (struct jpeg_source_mgr*)
	    (* reader->cinfo.mem->alloc_small)(
		    (j_common_ptr)&reader->cinfo,
		    JPOOL_PERMANENT,
		    sizeof(struct jpeg_source_mgr));
    }

    src = (struct jpeg_source_mgr*)reader->cinfo.src;
    src->init_source = init_source;
    src->fill_input_buffer = fill_input_buffer;
    src->skip_input_data = skip_input_data;
    src->resync_to_restart = jpeg_resync_to_restart;
    src->term_source = term_source;
    src->bytes_in_buffer = 0;
    src->next_input_byte = NULL;
}

static VALUE
jpeg_reader_initialize(VALUE obj, VALUE source)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    reader->cinfo.err = init_error_mgr(&reader->error);
    jpeg_create_decompress(&reader->cinfo);
    init_source_mgr(reader);
    reader->source = source;
    reader->cinfo.client_data = (void*)obj;
    reader->state = READER_INITIALIZED;
    return obj;
}

static VALUE
jpeg_reader_s_open(VALUE klass, VALUE path)
{
    struct jpeg_reader_data* reader;
    VALUE obj, io;

    io = rb_file_open_str(path, "rb");
    obj = rb_funcall(klass, id_new, 1, io);
    reader = get_jpeg_reader_data(obj);
    reader->close_source = 1;

    return obj;
}

static VALUE
jpeg_reader_source_will_be_closed(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    return reader->close_source ? Qtrue : Qfalse;
}

static inline void
reader_check_initialized(struct jpeg_reader_data* reader)
{
    assert(reader != NULL);
    if (reader->state < READER_INITIALIZED) {
	rb_raise(eImageFileJpegReaderError, "reader not initialized");
    }
}

static inline void
read_header(struct jpeg_reader_data* reader)
{
    assert(reader != NULL);
    reader_check_initialized(reader);
    if (reader->state < READER_RED_HEADER) {
	jpeg_read_header(&reader->cinfo, TRUE);
	reader->state = READER_RED_HEADER;
    }
}

static VALUE
jpeg_reader_get_image_width(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return ULONG2NUM(reader->cinfo.image_width);
}

static VALUE
jpeg_reader_get_image_height(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return ULONG2NUM(reader->cinfo.image_height);
}

static VALUE
jpeg_reader_get_num_components(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return LONG2NUM(reader->cinfo.num_components);
}

static VALUE
jpeg_reader_get_jpeg_color_space(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return color_space_to_symbol(reader->cinfo.jpeg_color_space);
}

static VALUE
jpeg_reader_get_out_color_space(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return color_space_to_symbol(reader->cinfo.out_color_space);
}

static VALUE
jpeg_reader_get_scale(VALUE obj)
{
    struct jpeg_reader_data* reader;
    VALUE num, denom;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    num = UINT2NUM(reader->cinfo.scale_num);
    denom =  UINT2NUM(reader->cinfo.scale_denom);
    return rb_Rational(num, denom);
}

static VALUE
jpeg_reader_set_scale(VALUE obj, VALUE scale)
{
    struct jpeg_reader_data* reader;
    VALUE r;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    r = rb_Rational1(scale);
    reader->cinfo.scale_num   = NUM2UINT(RRATIONAL(r)->num);
    reader->cinfo.scale_denom = NUM2UINT(RRATIONAL(r)->den);
    return scale;
}

static VALUE
jpeg_reader_get_output_gamma(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return DBL2NUM(reader->cinfo.output_gamma);
}

static VALUE
jpeg_reader_set_output_gamma(VALUE obj, VALUE gamma)
{
    struct jpeg_reader_data* reader;
    VALUE f;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    f = rb_Float(gamma);
    reader->cinfo.output_gamma = RFLOAT_VALUE(f);
    return gamma;
}

static VALUE
jpeg_reader_is_buffered_image(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return reader->cinfo.buffered_image ? Qtrue : Qfalse;
}

static VALUE
jpeg_reader_get_dct_method(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return dct_method_to_symbol(reader->cinfo.dct_method);
}

static VALUE
jpeg_reader_is_quantize_colors(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return reader->cinfo.quantize_colors ? Qtrue : Qfalse;
}

static VALUE
jpeg_reader_get_dither_mode(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    read_header(reader);
    return dither_mode_to_symbol(reader->cinfo.dither_mode);
}

static inline void
calc_output_dimensions(struct jpeg_reader_data* reader)
{
    assert(reader != NULL);
    if (reader->state < READER_STARTED_DECOMPRESS) {
	read_header(reader);
	jpeg_calc_output_dimensions(&reader->cinfo);
    }
}

static VALUE
jpeg_reader_get_output_width(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    calc_output_dimensions(reader);
    return UINT2NUM(reader->cinfo.output_width);
}

static VALUE
jpeg_reader_get_output_height(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    calc_output_dimensions(reader);
    return UINT2NUM(reader->cinfo.output_height);
}

static VALUE
jpeg_reader_get_output_components(VALUE obj)
{
    struct jpeg_reader_data* reader;
    reader = get_jpeg_reader_data(obj);
    calc_output_dimensions(reader);
    return INT2NUM(reader->cinfo.output_components);
}

static inline void
start_decompress(struct jpeg_reader_data* reader)
{
    assert(reader != NULL);
    if (reader->state < READER_STARTED_DECOMPRESS) {
	read_header(reader);
	jpeg_start_decompress(&reader->cinfo);
	reader->state = READER_STARTED_DECOMPRESS;
    }
}

static inline uint32_t
cmyk_to_rgb24(JSAMPROW cmyk)
{
    double const c = cmyk[0]/255;
    double const m = cmyk[1]/255;
    double const y = cmyk[2]/255;
    double const k = cmyk[3]/255;
    double const r = c*(1 - k) + k;
    double const g = m*(1 - k) + k;
    double const b = y*(1 - k) + k;
    return ((uint32_t)(r*255) << 16) | ((uint32_t)(g*255) << 8) | ((uint32_t)(b*255));
}

static inline uint16_t
cmyk_to_rgb16_565(JSAMPROW cmyk)
{
    double const c = cmyk[0]/255;
    double const m = cmyk[1]/255;
    double const y = cmyk[2]/255;
    double const k = cmyk[3]/255;
    double const r = c*(1 - k) + k;
    double const g = m*(1 - k) + k;
    double const b = y*(1 - k) + k;
    return ((uint16_t)(r*31) << 11) | ((uint16_t)(g*63) << 6) | ((uint16_t)(b*31));
}

static void
convert_scanlines_from_CMYK(
	VALUE image_buffer, JSAMPARRAY rows,
	long const sl_beg, long const sl_end,
	rb_image_file_image_pixel_format_t const pixel_format,
	long const width, long const stride)
{
    long i, j;
    JSAMPROW src;

    assert(TYPE(image_buffer) == T_STRING);
    assert(rows != NULL);
    assert(sl_beg < sl_end);
    assert(pixel_format != RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID);

    switch (pixel_format) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    for (i = sl_beg; i < sl_end; ++i) {
		uint32_t* dst = (uint32_t*)(RSTRING_PTR(image_buffer) + i*stride*4);
		src = rows[i];
		for (j = 0; j < width; ++j) {
		    uint32_t const pixel = cmyk_to_rgb24(src + 4*j);
		    *dst++ = pixel;
		}
		while (j++ < stride) *dst++ = 0;
	    }
	    break;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    for (i = sl_beg; i < sl_end; ++i) {
		uint16_t* dst = (uint16_t*)(RSTRING_PTR(image_buffer) + i*stride*2);
		src = rows[i];
		for (j = 0; j < width; ++j) {
		    uint16_t const pixel = cmyk_to_rgb16_565(src + 4*j);
		    *dst++ = pixel;
		}
		while (j++ < stride) *dst++ = 0;
	    }
	    break;

	default:
	    rb_bug("invalid pixel format");
	    break;
    }
}

static void
convert_scanlines_from_RGB(
	VALUE image_buffer, JSAMPARRAY rows,
	long const sl_beg, long const sl_end,
	rb_image_file_image_pixel_format_t const pixel_format,
	long const width, long const stride)
{
    long i, j;
    JSAMPROW src;

    assert(TYPE(image_buffer) == T_STRING);
    assert(rows != NULL);
    assert(sl_beg < sl_end);
    assert(pixel_format != RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID);

    switch (pixel_format) {
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_ARGB32:
	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24:
	    for (i = sl_beg; i < sl_end; ++i) {
		uint32_t* dst = (uint32_t*)(RSTRING_PTR(image_buffer) + i*stride*4);
		src = rows[i];
		for (j = 0; j < width; ++j) {
		    uint32_t pixel = (uint32_t)(*src++) << 16;
		    pixel |= (uint32_t)(*src++) << 8;
		    pixel |= *src++;
		    *dst++ = pixel;
		}
		while (j++ < stride) *dst++ = 0;
	    }
	    break;

	case RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB16_565:
	    for (i = sl_beg; i < sl_end; ++i) {
		uint16_t* dst = (uint16_t*)(RSTRING_PTR(image_buffer) + i*stride*2);
		src = rows[i];
		for (j = 0; j < width; ++j) {
		    uint16_t pixel = (uint16_t)(*src++ >> 3) << 11;
		    pixel |= (uint16_t)(*src++ >> 2) << 5;
		    pixel |= *src++ >> 3;
		    *dst++ = pixel;
		}
		while (j++ < stride) *dst++ = 0;
	    }
	    break;

	default:
	    rb_bug("invalid pixel format");
	    break;
    }
}

static void
process_arguments_of_read_image(int argc, VALUE* argv, struct jpeg_reader_data* reader,
	VALUE* params_ptr,
	rb_image_file_image_pixel_format_t* pixel_format_ptr,
	long* width_ptr,
	long* height_ptr,
	long* stride_ptr
	)
{
    VALUE params, width, height;
    VALUE pixel_format = Qnil;
    VALUE stride = Qnil;

    rb_image_file_image_pixel_format_t pf;
    long st;

    assert(reader != NULL);
    assert(reader->state < READER_STARTED_DECOMPRESS);

    rb_scan_args(argc, argv, "01", &params);
    if (TYPE(params) == T_HASH) {
	pixel_format = rb_hash_lookup(params, ID2SYM(id_pixel_format));
	stride = rb_hash_lookup(params, ID2SYM(id_row_stride));
    }
    else {
	rb_warning("invalid arguments are ignored.");
	params = rb_hash_new();
    }

    if (!NIL_P(pixel_format) && TYPE(pixel_format) != T_SYMBOL) {
	rb_warning("pixel_format is not a symbol.");
	pixel_format = Qnil;
    }
    if (!NIL_P(pixel_format)) {
	pf = rb_image_file_image_symbol_to_pixel_format(pixel_format);
	if (RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID == pf) {
	    VALUE str = rb_id2str(SYM2ID(pixel_format));
	    rb_warning("invalid pixel_format (%s), use default instead.", StringValueCStr(str));
	    pixel_format = Qnil;
	}
	else {
	    J_COLOR_SPACE const jcs = image_pixel_format_to_j_color_space(pf);
	    if (JCS_UNKNOWN == jcs)
		rb_warning("unknown pixel format, use default instead.");
	    else
		reader->cinfo.out_color_space = jcs;
	}
    }

    if (!NIL_P(stride) && TYPE(stride) != T_FIXNUM && TYPE(stride) != T_BIGNUM) {
	rb_warning("stride is not an integer.");
	stride = Qnil;
    }

    start_decompress(reader);

    if (NIL_P(pixel_format)) {
	pf = j_color_space_to_image_pixel_format(reader->cinfo.out_color_space);
	if (RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_INVALID == pf) {
	    if (reader->cinfo.out_color_space != JCS_CMYK) {
		char const* jcs_name = j_color_space_name(reader->cinfo.out_color_space);
		rb_raise(eImageFileJpegReaderError,
			"unsupported output color space (%s)", jcs_name);
	    }
	    pf = RB_IMAGE_FILE_IMAGE_PIXEL_FORMAT_RGB24;
	}
	rb_hash_aset(params, ID2SYM(id_pixel_format),
		rb_image_file_image_pixel_format_to_symbol(pf));
    }
    else
	rb_hash_aset(params, ID2SYM(id_pixel_format), pixel_format);

    width = UINT2NUM(reader->cinfo.output_width);
    rb_hash_aset(params, ID2SYM(id_width), width);

    height = UINT2NUM(reader->cinfo.output_height);
    rb_hash_aset(params, ID2SYM(id_height), height);

    if (!NIL_P(stride)) {
	st = NUM2LONG(stride);
	if (st < (long)reader->cinfo.output_width) {
	    rb_warning("stride less than output_width.");
	    st = (long)reader->cinfo.output_width;
	    stride = LONG2NUM(st);
	}
    }
    else
	st = (long)reader->cinfo.output_width;
    rb_hash_aset(params, ID2SYM(id_row_stride), stride);

    *params_ptr = params;
    *pixel_format_ptr = pf;
    *width_ptr = (long)reader->cinfo.output_width;
    *height_ptr = (long)reader->cinfo.output_height;
    *stride_ptr = st;
}

static VALUE
jpeg_reader_read_image(int argc, VALUE* argv, VALUE obj)
{
    struct jpeg_reader_data* reader;
    VALUE params, image, image_buffer;
    rb_image_file_image_pixel_format_t pf;
    long wd, ht, st, i, nc;

    VALUE row_buffer;
    VALUE sample_buffer;
    JSAMPARRAY rows;

    reader = get_jpeg_reader_data(obj);

    process_arguments_of_read_image(argc, argv, reader, &params, &pf, &wd, &ht, &st);
    assert(reader->state >= READER_STARTED_DECOMPRESS);

    image = rb_funcall(cImageFileImage, id_new, 1, params);
    image_buffer = rb_image_file_image_get_buffer(image);

    row_buffer = rb_str_tmp_new(sizeof(JSAMPROW)*ht);
    rows = (JSAMPARRAY)(RSTRING_PTR(row_buffer));

    nc = (long)reader->cinfo.output_components;
    sample_buffer = rb_str_tmp_new(sizeof(JSAMPLE)*ht*wd*nc);
    for (i = 0; i < ht; ++i)
	rows[i] = (JSAMPROW)(RSTRING_PTR(sample_buffer) + i*wd*nc);

    while ((long)reader->cinfo.output_scanline < ht) {
	long sl_beg, sl_end;

	sl_beg = reader->cinfo.output_scanline;
	jpeg_read_scanlines(
		&reader->cinfo,
		rows + reader->cinfo.output_scanline,
		(JDIMENSION)ht - reader->cinfo.output_scanline);
	sl_end = reader->cinfo.output_scanline;

	switch (reader->cinfo.out_color_space) {
	    case JCS_RGB:
		convert_scanlines_from_RGB(image_buffer, rows, sl_beg, sl_end, pf, wd, st);
		break;
	    case JCS_CMYK:
		convert_scanlines_from_CMYK(image_buffer, rows, sl_beg, sl_end, pf, wd, st);
		break;
	    default:
		break;
	}
    }

    jpeg_finish_decompress(&reader->cinfo);
    reader->state = READER_FINISHED_DECOMPRESS;

    return image;
}

void
rb_image_file_Init_image_file_jpeg_reader(void)
{
    cImageFileJpegReader = rb_define_class_under(mImageFile, "JpegReader", rb_cObject);
    rb_define_alloc_func(cImageFileJpegReader, jpeg_reader_alloc);
    rb_define_singleton_method(cImageFileJpegReader, "open", jpeg_reader_s_open, 1);
    rb_define_method(cImageFileJpegReader, "initialize", jpeg_reader_initialize, 1);
    rb_define_method(cImageFileJpegReader, "source_will_be_closed?", jpeg_reader_source_will_be_closed, 0);

    rb_define_method(cImageFileJpegReader, "image_width", jpeg_reader_get_image_width, 0);
    rb_define_method(cImageFileJpegReader, "image_height", jpeg_reader_get_image_height, 0);
    rb_define_method(cImageFileJpegReader, "num_components", jpeg_reader_get_num_components, 0);
    rb_define_method(cImageFileJpegReader, "jpeg_color_space", jpeg_reader_get_jpeg_color_space, 0);

    rb_define_method(cImageFileJpegReader, "out_color_space", jpeg_reader_get_out_color_space, 0);
    rb_define_method(cImageFileJpegReader, "scale", jpeg_reader_get_scale, 0);
    rb_define_method(cImageFileJpegReader, "scale=", jpeg_reader_set_scale, 1);
    rb_define_method(cImageFileJpegReader, "output_gamma", jpeg_reader_get_output_gamma, 0);
    rb_define_method(cImageFileJpegReader, "output_gamma=", jpeg_reader_set_output_gamma, 1);
    rb_define_method(cImageFileJpegReader, "buffered_image?", jpeg_reader_is_buffered_image, 0);
    rb_define_method(cImageFileJpegReader, "dct_method", jpeg_reader_get_dct_method, 0);
    rb_define_method(cImageFileJpegReader, "quantize_colors?", jpeg_reader_is_quantize_colors, 0);
    rb_define_method(cImageFileJpegReader, "dither_mode", jpeg_reader_get_dither_mode, 0);

    rb_define_method(cImageFileJpegReader, "output_width", jpeg_reader_get_output_width, 0);
    rb_define_method(cImageFileJpegReader, "output_height", jpeg_reader_get_output_height, 0);
    rb_define_method(cImageFileJpegReader, "output_components", jpeg_reader_get_output_components, 0);

    rb_define_method(cImageFileJpegReader, "read_image", jpeg_reader_read_image, -1);

    eImageFileJpegReaderError = rb_define_class_under(
	    cImageFileJpegReader, "Error", rb_eStandardError);

    CONST_ID(id_GRAYSCALE, "GRAYSCALE");
    CONST_ID(id_RGB, "RGB");
    CONST_ID(id_YCbCr, "YCbCr");
    CONST_ID(id_CMYK, "CMYK");
    CONST_ID(id_YCCK, "YCCK");
    CONST_ID(id_ISLOW, "ISLOW");
    CONST_ID(id_IFAST, "IFAST");
    CONST_ID(id_FLOAT, "FLOAT");
    CONST_ID(id_NONE, "NONE");
    CONST_ID(id_ORDERED, "ORDERED");
    CONST_ID(id_FS, "FS");
    CONST_ID(id_new, "new");
    CONST_ID(id_close, "close");
    CONST_ID(id_read, "read");
    CONST_ID(id_pixel_format, "pixel_format");
    CONST_ID(id_width, "width");
    CONST_ID(id_height,"height");
    CONST_ID(id_row_stride, "row_stride");
}
