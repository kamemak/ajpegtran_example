## Overview
This document describes changing the IJG code.
The IJG code is in the following folder.  
[`app/src/main/cpp`](app/src/main/cpp)  
Base version of the IJG code is **9c**. (Not latest)

The changes were made from the following three points.
- Remove unused functions and files.
- Modify interfaces for Android.
- Add extension functions.

Almost modified parts are commented with keyword `ajpegtran`.
Execute `grep` to check.


## Remove unused
### Remove unused files
The following files are removed.
- makefile, cofig files and project files for another systems  
- files for unused functions  
image reader,image writer, DCT, iDCT, command line interface...

### Remove unused functions
Modified [`jmorecfg.h`](app/src/main/cpp/jmorecfg.h) for removing unused functions.
Some '#define' are commented out.

## Modify interfaces for Android.
### Add entry function for Android.
Added [`ajpegtran.c`](app/src/main/cpp/ajpegtran.c) based on `jpegtran.c`, a command line interface.
This file contains functions can be called from Android.

### Add .mk file
Added [`Android.mk`](app/src/main/cpp/Android.mk), a kind of makefile for th native build environment, ndkBuild.

### Modify error interface
In the original implementation, when an error occurs, exit() function is called and program terminates.
This implementation uses the longjmp() and setjmp() APIs.
When an error occurs, longjmp() is called, and back to root function which called setjmp().
And error message is set to global variable.
Refer [`jerror.c`](app/src/main/cpp/ajpegtran.c) and [`jerror.c`](app/src/main/cpp/ajpegtran.c) to check implementation.

### Modify file I/O interface
In the original implementation, file pathes are pathed to interface function and access with fopen(), fread() and fwrite() functions.
Ih this implementation, file descriptors are pathed, and access with read() and write() functions.
Because in Android system, user application can't obtain the file path.
Refer last part of [`jinclude.h`](app/src/main/cpp/jinclude.h) and [`ajpegtran.c`](app/src/main/cpp/ajpegtran.c) to check implementation.

## Add extension functions
### Pixelize
The pixelize function is implemented to do_pixelize() function in [`transupp.c`](app/src/main/cpp/transupp.c).
The function is based on do_flatten() function.
do_flatten() clears all DCT coefficent to fill block with gray color.
do_pixelize() clears AC coefficients only, this causes 8x8 pixlization.

### Offset and Monochrome
The offset and monochrome functions are implemented in [`ajpegtran.c`](app/src/main/cpp/ajpegtran.c).
Refer toMonochrome() and brightnessControl() function.

### Clear EXIF tags
Clear orientation information, thumbnail and GEOTAGs functions are implimented to scan_exif_parameters_for_clear() in [`transupp.c`](app/src/main/cpp/transupp.c).
This implementation fills these tags with zeros, instead of front-packing.






