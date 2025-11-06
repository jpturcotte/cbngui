# Cataclysm: Bright Nights GUI

This repository contains a simple GUI overlay for the game Cataclysm: Bright Nights, built using Dear ImGui and SDL2.

## Building

To build the project, you will need to have the following dependencies installed:

*   A C++17 compiler
*   CMake
*   SDL2

On Debian/Ubuntu systems, you can install these dependencies with the following command:

```bash
sudo apt-get install build-essential cmake libsdl2-dev
```

Once you have the dependencies installed, you can build the project with the following commands:

```bash
git submodule update --init --recursive
rm -rf code/gui/build
mkdir -p code/gui/build
cd code/gui/build
cmake ..
make
```

## Running

To run the test application, you can use the following command:

```bash
cd code/gui/build
./test_gui
```
