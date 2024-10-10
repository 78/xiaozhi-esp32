#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdint.h"



/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#include "jpeglib.h"

/*
 * <setjmp.h> is used for the optional error recovery mechanism shown in
 * the second part of the example.
 */

#include <setjmp.h>

/******************** JPEG DECOMPRESSION SAMPLE INTERFACE *******************/

/* This half of the example shows how to read data from the JPEG decompressor.
 * It's a bit more refined than the above, in that we show:
 *   (a) how to modify the JPEG library's standard error-reporting behavior;
 *   (b) how to allocate workspace using the library's memory manager.
 *
 * Just to make this example a little different from the first one, we'll
 * assume that we do not intend to put the whole image into an in-memory
 * buffer, but to send it line-by-line someplace else.  We need a one-
 * scanline-high JSAMPLE array as a work buffer, and we will let the JPEG
 * memory manager allocate it for us.  This approach is actually quite useful
 * because we don't need to remember to deallocate the buffer separately: it
 * will go away automatically when the JPEG object is cleaned up.
 */


/*
 * ERROR HANDLING:
 *
 * The JPEG library's standard error handler (jerror.c) is divided into
 * several "methods" which you can override individually.  This lets you
 * adjust the behavior without duplicating a lot of code, which you might
 * have to update with each future release.
 *
 * Our example here shows how to override the "error_exit" method so that
 * control is returned to the library's caller when a fatal error occurs,
 * rather than calling exit() as the standard error_exit method does.
 *
 * We use C's setjmp/longjmp facility to return control.  This means that the
 * routine which calls the JPEG library must first execute a setjmp() call to
 * establish the return point.  We want the replacement error_exit to do a
 * longjmp().  But we need to make the setjmp buffer accessible to the
 * error_exit routine.  To do this, we make a private extension of the
 * standard JPEG error handler object.  (If we were using C++, we'd say we
 * were making a subclass of the regular error handler.)
 *
 * Here's the extended error handler struct:
 */

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static uint8_t  *jpegbuf;
static uint32_t jbufsize;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


//初始化资源,不执行任何操作
static void init_source(j_decompress_ptr cinfo)
{
    //不需要做任何事情.
    return;
} 
//填充输入缓冲区,一次性读取整帧数据
static boolean fill_input_buffer(j_decompress_ptr cinfo)
{  
	if(jbufsize==0)//结束了
	{
    printf("jd read off\r\n");
    //填充结束符
    jpegbuf[0] = (uint8_t) 0xFF;
    jpegbuf[1] = (uint8_t) JPEG_EOI;
    cinfo->src->next_input_byte =jpegbuf;
    cinfo->src->bytes_in_buffer = 2; 
	}else
	{
		cinfo->src->next_input_byte =jpegbuf;
		cinfo->src->bytes_in_buffer = jbufsize;
		jbufsize-=jbufsize;
	}
  return TRUE;
}
//在文件里面,跳过num_bytes个数据
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{ 
  /* Just a dumb implementation for now.  Could use fseek() except
  * it doesn't work on pipes.  Not clear that being smart is worth
  * any trouble anyway --- large skips are infrequent.
  */
  if (num_bytes > 0)
  {
    while(num_bytes>(long) cinfo->src->bytes_in_buffer)
    {
      num_bytes-=(long)cinfo->src->bytes_in_buffer;
      (void)cinfo->src->fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never
      * return FALSE, so suspension need not be handled.
      */
    }
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
  }
} 
//在解码结束后,被jpeg_finish_decompress函数调用
static void term_source(j_decompress_ptr cinfo)
{
    //不做任何处理
    return;
}
//初始化jpeg解码数据源
static void jpeg_filerw_src_init(j_decompress_ptr cinfo)
{ 
    if (cinfo->src == NULL)     /* first time for this JPEG object? */
    {
        cinfo->src = (struct jpeg_source_mgr *)
                     (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                              sizeof(struct jpeg_source_mgr)); 
    } 
    cinfo->src->init_source = init_source;
    cinfo->src->fill_input_buffer = fill_input_buffer;
    cinfo->src->skip_input_data = skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
    cinfo->src->term_source = term_source;
    cinfo->src->bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
    cinfo->src->next_input_byte = NULL; /* until buffer loaded */
} 

/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */
void mjpegdraw(uint8_t *mjpegbuffer, uint32_t size, uint8_t *outbuffer)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */
  jpegbuf=mjpegbuffer;
  jbufsize=size;
  
  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    //jpeg_abort_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);
  
  /* Step 2: specify data source (eg, a file) */

  jpeg_filerw_src_init(&cinfo); ;

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */
  cinfo.dct_method = JDCT_IFAST;
	cinfo.do_fancy_upsampling = 0; 
  cinfo.out_color_space=JCS_RGB;
  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   *///printf("w=%d, h=%d\n", cinfo.output_width, cinfo.output_height);
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    uint8_t *color=buffer[0];
    uint32_t index;
    uint32_t y = cinfo.output_scanline - 1;
    uint16_t *out = (uint16_t*)outbuffer + (y * cinfo.output_width);
    for(index = 0; index < cinfo.output_width; index++)
    {
      uint16_t c = ((*color) >> 3)<<11 | ((*(color+1))>> 2) << 5 | (*(color+2)) >> 3;
      out[index] = c>>8 | c<<8;
      color+=3;
    }
      // g_lcd.draw_bitmap(0, y, 240, 1, g_line_buffer);
  }


  /* Step 7: Finish decompression */
  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);
}

