/*
 * ajpegtran.c
 *
 * Copyright (C) 2018-2022, Kame
 * This file is based on the Independent JPEG Group's software.
 * For conditions of distribution and use, see the README.md
 *  and
 * the README file.
 *
 * This file contains a JPEG transcoding function
 * through JNI (Java Native Interface).
 * This file is modified from jpegtran.c which is for command line interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <android/log.h>
#ifdef NDEBUG
#define LOGD(...)
#else
#define LOG_TAG "JPEG_DEBUG"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#endif

#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */
#include "transupp.h"		/* Support routines for jpegtran */
#include "jversion.h"		/* for version message */

/* Note for ajpegtran
 *  Error check for configuration.
 *  This code can not support all functions.
 */
#ifdef C_PROGRESSIVE_SUPPORTED
#error C_PROGRESSIVE_SUPPORTED is not supported.
#endif
#ifdef C_MULTISCAN_FILES_SUPPORTED
#error C_MULTISCAN_FILES_SUPPORTED is not supported.
#endif
#if !TRANSFORMS_SUPPORTED
#error TRANSFORMS_SUPPORTED must be supported.
#endif
#ifdef PROGRESS_REPORT
#error PROGRESS_REPORT is not supported.
#endif
#ifdef NEED_SIGNAL_CATCHER
#error NEED_SIGNAL_CATCHER is not supported.
#endif


/*
 * Argument-parsing code.
 * The switch parser is designed to be useful with DOS-style command line
 * syntax, ie, intermixed switches and file names, where only the switches
 * to the left of a given file name affect processing of that file.
 * The main program in this file doesn't actually use this capability...
 */
/* Note for ajpegtran
 *  For implement more easy , options from Java are passed with string as
 *  same as command line one and decode them with IJG original parsing code.
 */
static JCOPY_OPTION copyoption;	/* -copy switch */
static jpeg_transform_info transformoption; /* image transformation options */

/* Note for ajpegtran
 *  For error handling, longjmp API is used. The valiables for longjmp is declared below.
 *  When error occured, a error message is contained to the char-array below.
 */
jmp_buf jbuf;
char errmsgbuffer[JMSG_LENGTH_MAX];

/* Note for ajpegtran
 *  The following variables are for extension functions, 'monochrome' and 'offset'.
 */
int coeff_adj;
int coeff_offset[4];
int monochrome;

LOCAL(void)
select_transform (JXFORM_CODE transform)
/* Silly little routine to detect multiple transform options,
 * which we can't handle.
 */
{
#if TRANSFORMS_SUPPORTED
  if (transformoption.transform == JXFORM_NONE ||
      transformoption.transform == transform) {
    transformoption.transform = transform;
  } else {
    LOGD("can only do one image transformation at a time");
  }
#else
  LOGD("sorry, image transformation was not compiled");
#endif
}

/* Note for ajpegtran
 *  The following function is modified for parsing a long string contain all
 *  options with strtok.
 *  (In the original code, arguments are divided to independent strings.)
 */
LOCAL(int)
parse_switches (j_compress_ptr cinfo, char *argstr,
		int last_file_arg_seen, boolean for_real)
/* Parse optional switches.
 * Returns argv[] index of first file-name argument (== argc if none).
 * Any file names with indexes <= last_file_arg_seen are ignored;
 * they have presumably been processed in a previous iteration.
 * (Pass 0 for last_file_arg_seen on the first or only iteration.)
 * for_real is FALSE on the first (dummy) pass; we may skip any expensive
 * processing.
 */
{
  char * arg;
  char * arg2;
  boolean simple_progressive;
  char * scansarg = NULL;	/* saves -scans parm if any */

  /* Set up default JPEG parameters. */
  simple_progressive = FALSE;
  copyoption = JCOPYOPT_DEFAULT;
  transformoption.transform = JXFORM_NONE;
  transformoption.perfect = FALSE;
  transformoption.trim = FALSE;
  transformoption.force_grayscale = FALSE;
  transformoption.crop = FALSE;
  cinfo->err->trace_level = 0;

  /* Scan command line options, adjust parameters */

  for (arg = strtok(argstr," ") ; arg ; arg = strtok(NULL," ") ) {
    if (*arg != '-') {
      /* error */
      strcpy(errmsgbuffer,"Parse error:file name exist?");
      return 0;
    }
    arg++;			/* advance past switch marker character */

    if (keymatch(arg, "arithmetic", 1)) {
      /* Use arithmetic coding. */
#ifdef C_ARITH_CODING_SUPPORTED
      cinfo->arith_code = TRUE;
#else
      strcpy(errmsgbuffer,"Parse error:arithmetic coding not supported");
      return 0;
#endif

    } else if (keymatch(arg, "copy", 2)) {
      /* Select which extra markers to copy. */
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
        /* error  */
	strcpy(errmsgbuffer,"Parse error:missed parameter(copy)");
	return 0;
      }
      if (keymatch(arg2, "none", 1)) {
	copyoption = JCOPYOPT_NONE;
      } else if (keymatch(arg2, "comments", 1)) {
	copyoption = JCOPYOPT_COMMENTS;
      } else if (keymatch(arg2, "all", 1)) {
	copyoption = JCOPYOPT_ALL;
      } else{	/* advance to next argument */
        /* error  */
	strcpy(errmsgbuffer,"Parse error:unknown parameter(copy)");
	return 0;
      }

    } else if (keymatch(arg, "crop", 2)) {
      /* Perform lossless cropping. */
#if TRANSFORMS_SUPPORTED
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
        /* error  */
	strcpy(errmsgbuffer,"Parse error:missed parameter(crop)");
	return 0;
      }
      if (transformoption.crop /* reject multiple crop/wipe requests */ ||
	  ! jtransform_parse_crop_spec(&transformoption, arg2)) {
	strcpy(errmsgbuffer,"Parse error:argument(crop)");
	return 0;
      }
#else
      select_transform(JXFORM_NONE);	/* force an error */
#endif

    } else if (keymatch(arg, "flip", 1)) {
      /* Mirror left-right or top-bottom. */
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
    strcpy(errmsgbuffer,"Parse error:missed parameter(flip)");
	return 0;
      }
      if (keymatch(arg2, "horizontal", 1))
	select_transform(JXFORM_FLIP_H);
      else if (keymatch(arg2, "vertical", 1))
	select_transform(JXFORM_FLIP_V);
      else{
	/* error  */
	strcpy(errmsgbuffer,"Parse error:argument(flip)");
	return 0;
      }

    } else if (keymatch(arg, "grayscale", 1) || keymatch(arg, "greyscale",1)) {
      /* Force to grayscale. */
#if TRANSFORMS_SUPPORTED
      transformoption.force_grayscale = TRUE;
#else
      select_transform(JXFORM_NONE);	/* force an error */
#endif

    } else if (keymatch(arg, "maxmemory", 3)) {
      /* Maximum memory in Kb (or Mb with 'm'). */
      long lval;
      char ch = 'x';

      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter");
	return 0;
      }
      if (sscanf(arg2, "%ld%c", &lval, &ch) < 1){
	/* error  */
	strcpy(errmsgbuffer,"Parse error:argument(maxmemory)");
	return 0;
      }
      if (ch == 'm' || ch == 'M')
	lval *= 1000L;
      cinfo->mem->max_memory_to_use = lval * 1000L;

    } else if (keymatch(arg, "optimize", 1) || keymatch(arg, "optimise", 1)) {
      /* Enable entropy parm optimization. */
#ifdef ENTROPY_OPT_SUPPORTED
      cinfo->optimize_coding = TRUE;
#else
      strcpy(errmsgbuffer,"Parse error:entropy optimization was not compiled");
      return 0;
#endif

    } else if (keymatch(arg, "perfect", 2)) {
      /* Fail if there is any partial edge MCUs that the transform can't
       * handle. */
      transformoption.perfect = TRUE;

    } else if (keymatch(arg, "progressive", 2)) {
      /* Select simple progressive mode. */
#ifdef C_PROGRESSIVE_SUPPORTED
      simple_progressive = TRUE;
      /* We must postpone execution until num_components is known. */
#else
      strcpy(errmsgbuffer,"Parse error:progressive output was not compiled");
      return 0;
#endif

    } else if (keymatch(arg, "restart", 1)) {
      /* Restart interval in MCU rows (or in MCUs with 'b'). */
      long lval;
      char ch = 'x';

      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter(restart)");
	return 0;
      }
      if (sscanf(arg2, "%ld%c", &lval, &ch) < 1){
	/* error  */
	strcpy(errmsgbuffer,"Parse error:argument(restart)");
	return 0;
      }
      if (lval < 0 || lval > 65535L){
	/* error  */
	strcpy(errmsgbuffer,"Parse error:argument(restart)");
	return 0;
      }
      if (ch == 'b' || ch == 'B') {
	cinfo->restart_interval = (unsigned int) lval;
	cinfo->restart_in_rows = 0; /* else prior '-restart n' overrides me */
      } else {
	cinfo->restart_in_rows = (int) lval;
	/* restart_interval will be computed during startup */
      }

    } else if (keymatch(arg, "rotate", 2)) {
      /* Rotate 90, 180, or 270 degrees (measured clockwise). */
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter(rotate)");
	return 0;
      }
      if (keymatch(arg2, "90", 2))
	select_transform(JXFORM_ROT_90);
      else if (keymatch(arg2, "180", 3))
	select_transform(JXFORM_ROT_180);
      else if (keymatch(arg2, "270", 3))
	select_transform(JXFORM_ROT_270);
      else{
	/* error  */
	strcpy(errmsgbuffer,"Parse error:argument(rotate)");
	return 0;
      }

    } else if (keymatch(arg, "scans", 1)) {
      /* Set scan script. */
#ifdef C_MULTISCAN_FILES_SUPPORTED
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter");
	return 0;
      }
      scansarg = arg2;
      /* We must postpone reading the file in case -progressive appears. */
#else
      strcpy(errmsgbuffer,"Parse error:multi-scan output was not compiled");
      return 0;
#endif

    } else if (keymatch(arg, "transpose", 1)) {
      /* Transpose (across UL-to-LR axis). */
      select_transform(JXFORM_TRANSPOSE);

    } else if (keymatch(arg, "transverse", 6)) {
      /* Transverse transpose (across UR-to-LL axis). */
      select_transform(JXFORM_TRANSVERSE);

    } else if (keymatch(arg, "trim", 3)) {
      /* Trim off any partial edge MCUs that the transform can't handle. */
      transformoption.trim = TRUE;

    } else if (keymatch(arg, "wipe", 1)) {
#if TRANSFORMS_SUPPORTED
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter(wipe)");
	return 0;
      }
      if (transformoption.crop /* reject multiple crop/wipe requests */ ||
	  ! jtransform_parse_crop_spec(&transformoption, arg2)) {
	strcpy(errmsgbuffer,"Parse error:argument(wipe)");
	return 0;
      }
      select_transform(JXFORM_WIPE);
#else
      select_transform(JXFORM_NONE);	/* force an error */
#endif

    } else if (keymatch(arg, "pixelize", 1)) {
#if TRANSFORMS_SUPPORTED
      arg2 = strtok(NULL," ");
      if (!arg2){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter(pixelize)");
	return 0;
      }
      if (transformoption.crop /* reject multiple crop/wipe/pixelize requests */ ||
	  ! jtransform_parse_crop_spec(&transformoption, arg2)) {
	strcpy(errmsgbuffer,"Parse error:argument(pixelize)");
	return 0;
      }
      select_transform(JXFORM_PIXELIZE);
#else
      select_transform(JXFORM_NONE);	/* force an error */
#endif
    } else if (keymatch(arg, "offset", 3)) {
      arg2 = strtok(NULL," ");
      char * arg3 = strtok(NULL," ");
      char * arg4 = strtok(NULL," ");
      char * arg5 = strtok(NULL," ");
      if (!arg2||!arg3||!arg4||!arg5){	/* advance to next argument */
	/* error */
	strcpy(errmsgbuffer,"Parse error:missed parameter(offset)");
	return 0;
      }
      if (( sscanf(arg2, "%d", coeff_offset+0) < 1) ||
          ( sscanf(arg3, "%d", coeff_offset+1) < 1) ||
          ( sscanf(arg4, "%d", coeff_offset+2) < 1) ||
          ( sscanf(arg5, "%d", coeff_offset+3) < 1) ){
	strcpy(errmsgbuffer,"Parse error:missed parameter(offset)");
	return 0;
      }
      coeff_adj = 1;
    } else if (keymatch(arg, "monochrome", 4)) {
        /* Trim off any partial edge MCUs that the transform can't handle. */
      monochrome = 1;
    } else if (keymatch(arg, "rmorientation", 3)) {
      /* Remove orientation information */
      cinfo->remove_orientation_info = TRUE;
    } else if (keymatch(arg, "rmthumbnail", 3)) {
      /* Remove thumbnail */
      cinfo->remove_thumbnail = TRUE;
    } else if (keymatch(arg, "rmgeotag", 3)) {
      /* Remove thumbnail */
      cinfo->remove_geotag = TRUE;

    } else {
      strcpy(errmsgbuffer,"Parse error:unknown switch");
      return 0;
    }
  }

  /* Post-switch-scanning cleanup */

  if (for_real) {

#ifdef C_PROGRESSIVE_SUPPORTED
    if (simple_progressive)	/* process -progressive; -scans can override */
      jpeg_simple_progression(cinfo);
#endif

#ifdef C_MULTISCAN_FILES_SUPPORTED
    if (scansarg != NULL)	/* process -scans if it was present */
      if (! read_scan_script(cinfo, scansarg)){
	strcpy(errmsgbuffer,"Parse error:argument error");
	return 0;
      }
#endif
  }

  return 1;			/* return index of next arg (file name) */
}

/**
 * Clear 2nd, 3rd and 4th component for coverting to monochrome image.
 * 
 * Note for ajpegtran
 *  This function is for extension function, '-monochrome'.
 *  This function shall be applied for YCbCr image only.
 *  For another image, the result will be not desiable.
 *  Error check is not implemented.
 */
void toMonochrome(
	j_decompress_ptr cinfo,
	jvirt_barray_ptr *src_coef_arrays
){
  int ci, offset_y,count;
  jpeg_component_info *compptr;
  JDIMENSION blk_y;
  JBLOCKARRAY dst_buffer;

  for (ci = 1; ci < cinfo->num_components; ci++) {
    compptr = cinfo->comp_info + ci;
    for (blk_y = 0; blk_y < compptr->height_in_blocks ; blk_y += compptr->v_samp_factor) {
      dst_buffer = (*cinfo->mem->access_virt_barray)
		    ((j_common_ptr) cinfo, src_coef_arrays[ci], blk_y,
		    (JDIMENSION) compptr->v_samp_factor, FALSE );
      for (offset_y = 0; offset_y < compptr->v_samp_factor; offset_y++) {
	JCOEFPTR datptr = (JCOEFPTR)(dst_buffer[offset_y]);
	memset((void *)datptr,0,sizeof(JCOEF)*DCTSIZE2*compptr->width_in_blocks);
      }
    }
  }
  return;
}

/**
 * Add offset to DC coefficient.
 * 
 * Note for ajpegtran
 *  This function is for extension function, '-offset'.
 *  Brightness and color can be adjustable with this.
 */
void brightnessControl(
	j_decompress_ptr cinfo,
	jvirt_barray_ptr *src_coef_arrays
){
  int ci, offset_y,count;
  jpeg_component_info *compptr;
  JDIMENSION blk_y;
  JBLOCKARRAY dst_buffer;

  for (ci = 0; ci < cinfo->num_components; ci++) {
    if (ci>4) break;
    if (!coeff_offset[ci]) continue;
    compptr = cinfo->comp_info + ci;
    for (blk_y = 0; blk_y < compptr->height_in_blocks ; blk_y += compptr->v_samp_factor) {
      dst_buffer = (*cinfo->mem->access_virt_barray)
		    ((j_common_ptr) cinfo, src_coef_arrays[ci], blk_y,
		    (JDIMENSION) compptr->v_samp_factor, FALSE );
      for (offset_y = 0; offset_y < compptr->v_samp_factor; offset_y++) {
	JCOEFPTR datptr = (JCOEFPTR)(dst_buffer[offset_y]);
	for( count = 0 ; count < compptr->width_in_blocks ; count++ ){
	  *datptr += coeff_offset[ci];
	  datptr += DCTSIZE2;
	}
      }
    }
  }
  return;
}

#define OPTTEMP_SIZE 256
/**
 * ajpegtran main entry.
 *
 * Execute Jpegtran.
 */
JNIEXPORT jstring JNICALL
Java_github_kamemak_ajpegtran_1example_MainActivity_ajpegtran( JNIEnv* env,
                                         jobject thiz,
                                         jint rfd,
                                         jint wfd,
                                         jstring jOptions
                                                  )
{
  struct jpeg_decompress_struct srcinfo;
  struct jpeg_compress_struct dstinfo;
  struct jpeg_error_mgr jsrcerr, jdsterr;
#ifdef PROGRESS_REPORT
  struct cdjpeg_progress_mgr progress;
#endif
  jvirt_barray_ptr * src_coef_arrays;
  jvirt_barray_ptr * dst_coef_arrays;
  /* We assume all-in-memory processing and can therefore use only a
   * single file pointer for sequential input and output operation. 
   */
  boolean simple_progressive = FALSE;

  const char* filePath;
  const char* optstr;
  char opttemp[OPTTEMP_SIZE];
  int parseResult;

  /* Note for ajpegtran
   *  Clear variables for extension functions.
   */
  monochrome = 0;
  coeff_adj = 0;
  coeff_offset[0] = 0;
  coeff_offset[1] = 0;
  coeff_offset[2] = 0;
  coeff_offset[3] = 0;
  errmsgbuffer[0]='\0';

  /* Note for ajpegtran
   *  Clear errno to get I/O error.
   */
  errno = 0;

  /* To handle a error, setjmp is used. */
  if ( setjmp( jbuf ) == 0 ) {
    /* Initialize the JPEG decompression object with default error handling. */
    srcinfo.err = jpeg_std_error(&jsrcerr);
    jpeg_create_decompress(&srcinfo);
    /* Initialize the JPEG compression object with default error handling. */
    dstinfo.err = jpeg_std_error(&jdsterr);
    jpeg_create_compress(&dstinfo);

    /* Now safe to enable signal catcher.
     * Note: we assume only the decompression object will have virtual arrays.
     */
#ifdef NEED_SIGNAL_CATCHER
    enable_signal_catcher((j_common_ptr) &srcinfo);
#endif

    /* Scan command line to find file names.
     * It is convenient to use just one switch-parsing routine, but the switch
     * values read here are mostly ignored; we will rescan the switches after
     * opening the input file.  Also note that most of the switches affect the
     * destination JPEG object, so we parse into that and then copy over what
     * needs to affects the source too.
     */
    optstr = (*env)->GetStringUTFChars(env, jOptions, NULL);
    if (!optstr||OPTTEMP_SIZE<=strlen(optstr)) {
      (*env)->ReleaseStringUTFChars(env, jOptions, optstr);
      strcpy(errmsgbuffer,"Argument error");
      longjmp(jbuf,1);
    }
    strcpy(opttemp,optstr);
    parseResult = parse_switches(&dstinfo, opttemp, 0, FALSE);
    (*env)->ReleaseStringUTFChars(env, jOptions, optstr);
    if ( !parseResult ){
      longjmp(jbuf,1);
    }

    jsrcerr.trace_level = jdsterr.trace_level;
    srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;
    LOGD("max_memory_to_use = %ld",srcinfo.mem->max_memory_to_use);

#ifdef PROGRESS_REPORT
    start_progress_monitor((j_common_ptr) &dstinfo, &progress);
#endif

    /* Specify data source for decompression */
    jpeg_stdio_src(&srcinfo, rfd);

    /* Enable saving of extra markers that we want to copy */
    jcopy_markers_setup(&srcinfo, copyoption);

    /* Read file header */
    (void) jpeg_read_header(&srcinfo, TRUE);

    /* Any space needed by a transform option must be requested before
     * jpeg_read_coefficients so that memory allocation will be done right.
     */
#if TRANSFORMS_SUPPORTED
    /* Fail right away if -perfect is given and transformation is not perfect.
     */
    if (!jtransform_request_workspace(&srcinfo, &transformoption)) {
      strcpy(errmsgbuffer,"Setup error:perfect option can't be executed");
      longjmp(jbuf,1);
    }
#endif

    /* Read source file as DCT coefficients */
    src_coef_arrays = jpeg_read_coefficients(&srcinfo);

    /* if monochrome option is specified, clear Cb and Cr coefficients */
    if (monochrome) {
      toMonochrome(&srcinfo,src_coef_arrays);
    }

    /* if brightness control option is specified, offset DC coefficient. */
    if (coeff_adj) {
      brightnessControl(&srcinfo,src_coef_arrays);
    }

    /* Initialize destination compression parameters from source values */
    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

    /* Adjust destination parameters if required by transform options;
     * also find out which set of coefficient arrays will hold the output.
     */
#if TRANSFORMS_SUPPORTED
    dst_coef_arrays = jtransform_adjust_parameters(&srcinfo, &dstinfo,
						 src_coef_arrays,
						 &transformoption);
#else
    dst_coef_arrays = src_coef_arrays;
#endif

    /* Close input file, if we opened it.
     * Note: we assume that jpeg_read_coefficients consumed all input
     * until JPEG_REACHED_EOI, and that jpeg_finish_decompress will
     * only consume more while (! cinfo->inputctl->eoi_reached).
     * We cannot call jpeg_finish_decompress here since we still need the
     * virtual arrays allocated from the source object for processing.
     */
    close(rfd);
    rfd = -1;

    /* Specify data destination for compression */
    jpeg_stdio_dest(&dstinfo, wfd);

    /* Start compressor (note no image data is actually written here) */
    jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

    /* Copy to the output file any extra markers that we want to preserve */
    jcopy_markers_execute(&srcinfo, &dstinfo, copyoption);

    /* Execute image transformation, if any */
#if TRANSFORMS_SUPPORTED
    jtransform_execute_transformation(&srcinfo, &dstinfo,
				    src_coef_arrays,
				    &transformoption);
#endif

    /* Finish compression and release memory */
    jpeg_finish_compress(&dstinfo);
    (void) jpeg_finish_decompress(&srcinfo);

    /* Close output file, if we opened it */
    /*  Moved to outside of braces */

#ifdef PROGRESS_REPORT
    end_progress_monitor((j_common_ptr) &dstinfo);
#endif
    strcpy(errmsgbuffer,"OK");
  }
  else{
    LOGD("longjmp was occured");
  }
  jpeg_destroy_compress(&dstinfo);
  jpeg_destroy_decompress(&srcinfo);
  if (rfd != -1) close(rfd);
  if (wfd != -1) close(wfd);
  /* All done. */
  if (*errmsgbuffer) {
    return (*env)->NewStringUTF(env, errmsgbuffer);
  }
  return (*env)->NewStringUTF(env, "Unknown Error");
}

/**
 * ajpegtranhead entry.
 *
 * Get JPEG property from specified file.
 */

JNIEXPORT jstring JNICALL
Java_github_kamemak_ajpegtran_1example_MainActivity_ajpegtranhead( JNIEnv* env,
                                         jobject thiz,
                                         jint fd,
                                         jintArray jParamArray
                                                  )
{
  struct jpeg_decompress_struct srcinfo;
  struct jpeg_error_mgr jsrcerr;
#ifdef PROGRESS_REPORT
  struct cdjpeg_progress_mgr progress;
#endif
  /* We assume all-in-memory processing and can therefore use only a
   * single file pointer for sequential input and output operation. 
   */

  /* Note for ajpegtran
   *  Clear errno to get I/O error.
   */
  errno = 0;

  errmsgbuffer[0]='\0';
  if( setjmp( jbuf ) == 0 ) {
    /* Initialize the JPEG decompression object with default error handling. */
    srcinfo.err = jpeg_std_error(&jsrcerr);
    jpeg_create_decompress(&srcinfo);

    /* Now safe to enable signal catcher.
     * Note: we assume only the decompression object will have virtual arrays.
     */
#ifdef NEED_SIGNAL_CATCHER
    enable_signal_catcher((j_common_ptr) &srcinfo);
#endif

    jsrcerr.trace_level = 0;
    srcinfo.mem->max_memory_to_use = 0;

    /* Specify data source for decompression */
    jpeg_stdio_src(&srcinfo, fd);

    /* Enable saving of extra markers that we want to copy */
    copyoption = JCOPYOPT_DEFAULT;
    jcopy_markers_setup(&srcinfo, copyoption);

    /* Read file header */
    (void) jpeg_read_header(&srcinfo, TRUE);

#ifndef NDEBUG
    LOGD("Componetnum = %d",srcinfo.num_components);
    if ( srcinfo.num_components && srcinfo.comp_info ) {
      int cnt;
      for( cnt = 0 ; cnt< srcinfo.num_components ; cnt++ ) {
	LOGD(" samp(%d) = %d,%d",cnt,srcinfo.comp_info[cnt].h_samp_factor,srcinfo.comp_info[cnt].v_samp_factor);
      }
    }
    LOGD("X = %d",srcinfo.image_width);
    LOGD("Y = %d",srcinfo.image_height);
#endif

    /* Set return array */
    {
      jint *paramArray;
      int cnt,hmax,vmax;
      if ( srcinfo.num_components == 0 ){
	strcpy(errmsgbuffer,"JPEG Error:No component");
	longjmp(jbuf,1);
      }
      /* Calculate MCU size from sampling factors */
      hmax = srcinfo.comp_info[0].h_samp_factor;
      vmax = srcinfo.comp_info[0].v_samp_factor;
      for(cnt=1;cnt< srcinfo.num_components ; cnt++ ) {
	if( hmax < srcinfo.comp_info[cnt].h_samp_factor ) hmax = srcinfo.comp_info[cnt].h_samp_factor;
	if( vmax < srcinfo.comp_info[cnt].v_samp_factor ) vmax = srcinfo.comp_info[cnt].v_samp_factor;
      }
      /* Check array size */
      paramArray = (*env)->GetIntArrayElements(env, jParamArray, NULL);
      if( (*env)->GetArrayLength(env,jParamArray) < 10 ){
	(*env)->ReleaseIntArrayElements(env, jParamArray, paramArray, 0);
	strcpy(errmsgbuffer,"IF Error:Short array");
	longjmp(jbuf,1);
      }
      /* Copy properties to array */
      paramArray[0] = srcinfo.image_width;
      paramArray[1] = srcinfo.image_height;
      paramArray[2] = srcinfo.num_components;
      paramArray[3] = hmax;
      paramArray[4] = vmax;
      paramArray[5] = srcinfo.jpeg_color_space;
      paramArray[6] = 0;
      paramArray[7] = 0;
      paramArray[8] = 0;
      paramArray[9] = 0;
      if( srcinfo.num_components > 0 ){
	paramArray[6] = srcinfo.quant_tbl_ptrs[srcinfo.comp_info[0].quant_tbl_no]->quantval[0];
      }
      if( srcinfo.num_components > 1 ){
	paramArray[7] = srcinfo.quant_tbl_ptrs[srcinfo.comp_info[1].quant_tbl_no]->quantval[0];
      }
      if( srcinfo.num_components > 2 ){
	paramArray[8] = srcinfo.quant_tbl_ptrs[srcinfo.comp_info[2].quant_tbl_no]->quantval[0];
      }
      if( srcinfo.num_components > 3 ){
	paramArray[9] = srcinfo.quant_tbl_ptrs[srcinfo.comp_info[3].quant_tbl_no]->quantval[0];
      }

      (*env)->ReleaseIntArrayElements(env, jParamArray, paramArray, 0);
    }

#ifdef PROGRESS_REPORT
    end_progress_monitor((j_common_ptr) &dstinfo);
#endif
    strcpy(errmsgbuffer,"OK");
  }
  else{
    LOGD("longjmp was occured");
  }
  if( fd != -1 ) close(fd);
  jpeg_destroy_decompress(&srcinfo);
  if(*errmsgbuffer){
    return (*env)->NewStringUTF(env, errmsgbuffer);
  }
  return (*env)->NewStringUTF(env, "Unknown Error");
}
