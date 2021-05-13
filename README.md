# The Forge Samples

Unofficial implementation of multiple rendering techniques using [The Forge](https://github.com/ConfettiFX/The-Forge) rendering engine. Based on the repository [the-forge-glfw](https://github.com/rextimmy/the-forge-glfw) by Tim Barnes for ease of use.

Follow step by step tutorials for these samples on [learn-computer-graphics](https://learn-computer-graphics.com/rendering-techniques/).

## 🚩 Table of Contents

<details><summary>🏗️ Show Build steps</summary>
<p>

* Prerequisites
  * [Linux](#Linux)
  * [Windows](#Windows)

* Build and Run
  * [Option 1 : CLI](#Option-1-CLI)
  * [Option 2 : Visual Studio](#Option-2-Visual-Studio)
  * [Option 3 : VSCode](#Option-3-VSCode)

</p>
</details>

<details><summary>🧩 Show Samples</summary>
<p>

* Getting Started
  * [Hello Triangle](#Hello-Triangle-source)
  * [Moving Cube](#Moving-Cube-source)

</p>
</details>

## Build steps

### Prerequisites

You need to install [CMake](https://cmake.org/) to build the project, and a C++ compiler which handles C++14.

#### Linux

```
sudo apt-get install build-essential cmake
```

#### Windows

To get the MSVC compiler, you need to install [Visual Studio](https://visualstudio.microsoft.com/) (and not VSCode), and select C++ development during installation.

### Build and Run

You can handle the `CMakeLists.txt` in any way you like, here's some way to use it :

#### `Option 1: CLI`

If you are on windows you can run the `GEN_CMAKE.bat` file. Else you can also open a command line and run the following commands :

```bash
mkdir build
cd build
cmake .. -A x64 -DD3D12=OFF -DVULKAN=ON
make
```

To run the project, go to the build folder and run the executable fo your wanted sample

#### `Option 2: Visual Studio`

Open this folder with the `CMake...` option in file->open on Visual Studio. Press `f7` to build and `f5` to run the project.

#### `Option 3: VSCode`

Use the `CMakeTools` plugin, build with `f7` then run with `f5` (but be careful to be on the right platform, there is a launch file for windows and for linux).

**Warning** : Currently there is a link error when building on windows with vscode

## Samples

### Getting Started

#### [Hello Triangle](http://rendering-techniques.learn-computer-graphics.com/doc/getting-started/triangle.html) [(source)](src/main.cpp)

This sample show the most basic example with a triangle rendered on screen

<img src="https://github.com/DiligentGraphics/DiligentSamples/raw/master/Tutorials/Tutorial01_HelloTriangle/Screenshot.png" width="50px"/>

#### [Moving Cube](http://rendering-techniques.learn-computer-graphics.com/doc/getting-started/moving-cube.html) [(source)](src/main.cpp)

This tutorial show how to render a rotating 3D object

<img src="https://github.com/DiligentGraphics/DiligentSamples/raw/master/Tutorials/Tutorial02_Cube/Animation_Large.gif" width="50px"/>
