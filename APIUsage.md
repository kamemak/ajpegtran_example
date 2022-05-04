## Overview

Functions called from Android are contained in [`app/src/main/cpp/ajpegtran.c`](app/src/main/cpp/ajpegtran.c).
This sample library provide the follwoing two functions.

- ajpegtranhead()  
Get properties about specified JPEG file.

- ajpegtran()  
Main function execute lossless JPEG operations.


## ajpegtranhead()
By calling this function, the JPEG property can be get.

`ajpegtranhead( JNIEnv* env,
                                         jobject thiz,
                                         jint fd,
                                         jintArray jParamArray
                                                  )`

### Argument
- jint fd  
File descripor to read JPEG file.
- jintArray jParamArray  
Integer array of size 10 to return parameters.

|Index|Value|
|:-:|:-|
|0|Image width (pixel)|
|1|Image height (pixel)|
|2|Color component number (1,3,4)|
|3|MCU width (block)|
|4|MCU height (block)|
|5|Color space (Value is defined in jpeglib.h line 220)|
|6|Quantization value for DC of component 0|
|7|Quantization value for DC of component 1|
|8|Quantization value for DC of component 2|
|9|Quantization value for DC of component 3|


### Return value
This function returns the following string.
- "OK"  
Successed.
- Another  
Error message.

### Note
The sample application call this function before ajpegtran() always.
But you can call ajpegtran() directly.

The quantization value is useful for option 'offset' about ajpegtran().
Refer explanation of option, 'offset'.

## ajpegtran()
This function execute lossless operation on JPEG image.

`ajpegtran( JNIEnv* env,
                                         jobject thiz,
                                         jint rfd,
                                         jint wfd,
                                         jstring jOptions
                                                  )`

### Argument
- jint rfd  
File descripor to read JPEG file.
- jint wfd  
File descripor to write JPEG file.
- jstring jOptions  
This string is for specifing option like command line below.  
`-optimize -copy all -crop 640x480+0+0`

### Return value
This function returns the following string.
- "OK"  
Successed.
- Another  
Error message.

### Available existing options
The following option can be used.
- copy [all|default|none]
- grayscale
- optimize
- crop
- flip [horizontal|vertical] 
- roate [90|180|270] 
- wipe
- restart

The following options can be used maybe.
- trim
- perfect
- transpose
- transverse
- maxmemory

Refer to IJG document about detail of the options.



### Additional Options
The follwoing options are added for this implement.
- pixelize  
Pixlize the specified area. Pixelization size is 8x8 only. 
The method of specifying an area is the same as the 'crop' option like below.  
`-pixelize 640x480+0+0`  
The area specified above is that size is 640x480, coordicate of upper left corner is (0,0).
- offset  
Add offset value to DC coefficients. This makes brightness control and color adjustment.
The offset values are specified like below.  
`-offset 128 32 64 0`  
The offset values are specified for each component.
Four numbers must be specified, regardless of the number of color components.
When color components are less than 4, specify dummies.  
The offsets are added to the quantized values, so the effect changes in proportion to the quantization values.  
For example.  
	- Color Format : YCbCr  
	- Quantization value : 6,7,7,0  
	- Specified option : -offset 5 0 3 0  
DC coefficient of 'Y' is added by 30, JPEG image becomes slightly brighter.
DC coefficient of 'Cb' is added by 21, JPEG image becomes slightly redish.  
- monochrome  
This option is similar to '-grayscale'.
The '-grayscale' option removes Cb and Cr component.
This option clear all coefficeits of 2nd(Cb) and 3rd(Cr) component, but data area for each component remains.
So you can make sepia image, when using this option combined with '-offset' option like below.  
`-monochrome -offset 0 -18 35 0`  
- rmorientation  
Clear EXIF orientation information. If orientation information is exist, image viewer rotate image automatically.
This option is intended to avoid such behavior.
- rmthumbnail  
Clear EXIF thumbnail. This library execute image transform to main image only.
It does not process thumbnails. So mismatch will be occurred between them.
Avoiding these situation, this option is exist.  
This option fill thumbnail area with zeros, instead of front-packing to simplify implementation.
If this behavior is not preferred, use external EXIF app.
- rmgeotag  
Clear GEOTAGs.
This option fill GEOTAG area with zeros, instead of front-packing too.
This option exists to protect personal information.

