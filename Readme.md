## What is this
This repository contains sample application code for Android uses jpegtran via JNI.

## What is Jpegtran
Jpegtran is a command line program which can exetute the following operations to JPEG image without loss.
- Crop
- Rotate (90,180,270 degrees)
- Flip (horizontal,vertical)
- To Grayscale

It is a part of the IJG(Independet JPEG Group) code.

## File list
- This folder  
In Android Studio, you can import projects by specifying this folder.
- [app/src/main/cpp](app/src/main/cpp)  
This folder contains IJG code.
Some files are modified and removed for this app.
- [ChangesIJG.md](ChangesIJG.md)  
This file describes the changes from the original IJG code.
- [License.md](License.md)  
License terms for this codes
- [APIUsage.md](APIUsage.md)  
How to use the API. Please read this file when importing this library into your project.

## How to build
Import this folder from Android studio, and build.  
NDK (Native Development Kit) is required to build. Any version can be used, maybe.  
SDK version 29 or later can be used.

## Application usage
1. Tap [OPEN] button. The file selector will come up, and select file.  
JPEG file properties will be shown under the button, [SAVE] button will appear right of [OPEN].
2. Tap [SAVE] button, File selector will come up, and specify output file.  
A grayscaled and flipped JPEG image will be output.  
To change the operation, change line 214 of [`MainActivity.java`](app/src/main/java/github/kamemak/ajpegtran_example/MainActivity.java).
