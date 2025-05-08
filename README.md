# Mandrill

This is an education and research graphics framework written and used at Lund University.
Mandrill uses the Vulkan API and lightly abstracts some of its most commonly used features without restricting the low-level controll offered by using Vulkan.
This repository contains the main framework together with some sample apps that are located in the `app` folder.
New apps can be created in the same folder, but it is recommened to create a new repository for each project or application, and accessing the Mandrill framework as a submodule.

Mandrill uses [GLFW](https://github.com/glfw/glfw) for window handling and [Dear ImGUI](https://github.com/ocornut/imgui) for user interaction through an immediate mode user interface.
Image loading is provided through [stb](https://github.com/nothings/stb).

## Build

Clone the repo, and make sure to also initialize the submodules:

	git clone --recurse-submodules https://github.com/rikardolajos/Mandrill.git

If you cloned the repo but forgot to initialize the submodules, use:

	git submodule update --init --recursive

This project uses CMake to generate project files, and has been tested on Windows with Visual Studio and on Linux with Make.
Use Visual Studio's internal CMake support for setting up the project: after cloning the git repo as instructed above, start Visual studio and pick `Open a local folder` and pick the folder where the repo was cloned to.

On other platforms, use the CMake command line interface to setup the project.
Navigate to the folder where the repo resides:

	cd Mandrill

Create a build folder:

	mkdir build

Change into the build folder and initialize CMake:

	cd build
	cmake ..

To build and compile the project use:

	cmake --build .

To run the project, change to the binary output folder and run a executable file:

	cd {Debug,Release}/bin
	./SampleApp.bin

## Setting up a new project with Mandrill


## Apps

The applications provided in this repository can be found in the `app` folder, and showcase different ways of using Mandrill.

### SampleApp

This is a minimal app, with rendering of a single textured billboard.
Custom vertex and index buffers are used to draw the mesh, and the Mandrill descriptor abstraction is used. 

### SceneViewer

The scene viewer uses the `Scene.cpp` scene abstraction that allows for loading meshes from Wavefront OBJ files.
To upload the scene from host (CPU) to device (GPU), the scene has to be compiled and synced to device.
See the `loadScene()` function and the `Scene.h` documentation for proper use.
The scene has a predetermined set of descritors that are used for rendering materials and so on.

### RayTracer

Mandrill abstracts some of the ray tracing handling as well (pipeline, shader binding table, and acceleration structure creation).
This ray tracer app shows how this functionality can be used.

### VolumeViewer

This application implements a basic ray marcher for rendering of volumetric entities.
**(Requires OpenVDB, see below.)**

## OpenVDB

Mandrill supports the use of VDB files.
For this to work, OpenVDB has to be available on your system so that CMake can find it during project generation.
If it is not found, a warning message will be given and the use of `Texture3D` will not be available.
See `app/VolumeViewer` for an example of loading a volume from a VDB file and then rendering it with ray marching.

On Windows, the easiest way of setting up OpenVDB for Mandrill, is to use [vcpkg](https://github.com/microsoft/vcpkg):

	vcpkg install openvdb
