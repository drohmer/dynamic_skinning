# CGP - Computer Graphics Programming Library

<a name="Introduction"></a>

## Introduction

[CGP - Computer Graphics Programming -](https://imagecomputing.net/cgp/index.html) is a lightweight and minimalist C++ library using OpenGL to represent, animate, and interact with 3D scenes. 

It features a set of simple structures and functions (vectors, matrices, mesh structures, transforms, camera, etc) that are simple to use and read. The objective is to save time compared to raw OpenGL coding, while preserving the fundamental logic and comprehension of high-performance Graphics. The 3D scene can be exported as a Webpage via Emscripten scripts.

The main objective of CGP is to provide
* **Easy to use CG basic tools**. 
* **Data and structure ready for simple interaction and animation**. 
* **Helping structures and functions that do not impose a framework**.
* **Easy to read and understand source code**. 
* **Secured and easy to debug code**. 

### Additional documentation

* **[Library Webpage](https://imagecomputing.net/cgp/index.html)**

* **[Code Examples](https://imagecomputing.net/cgp/documentation/07_examples/index.html)**, [github-page](https://github.com/drohmer/cgp_examples)


<a name="Compilation"></a>

## Compile the library

The directory _library/_ contains the source code of CGP, while the directory _examples/_ contains a set of example scenes.
Each example is an independent program with its own CMakeLists.txt and/or Makefile. 

CGP requires
* A C++14 (or greater) compatible compiler (GCC/CLang, or a recent Visual Studio).
* An OpenGL 3.3 (or greater) compatible system.
* [libGLFW](https://www.glfw.org/) and [pkgconfig](https://www.freedesktop.org/wiki/Software/pkg-config/) installed on Linux system.

