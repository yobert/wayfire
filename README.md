# Wayfire
# Introduction

[![Join the chat at https://gitter.im/Wayfire-WM/Lobby](https://badges.gitter.im/Wayfire-WM/Lobby.svg)](https://gitter.im/Wayfire-WM/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Wayfire is a wayland compositor based on libweston. It aims to create a customizable, extendable and lightweight environment without sacrificing its appearance. If you want to gain a better impression at what it can do, see [screenshots](https://github.com/ammen99/wayfire/wiki/Screenshots), [list of plugins](https://github.com/ammen99/wayfire/wiki/Configuration-&-Plugins)

# Build and install

Unfortunately, the upstream libweston library doesn't have all the necessary functionality needed by wayfire(mainly it doesn't support custom rendering), so you need to compile a patched version, which you can find here: [Patched libweston](https://github.com/ammen99/weston).

Note: if you install this repo, it will overwrite the weston package provided by your distro. If you don't want to overwrite it, then you can install the patched weston in another location, for example `/opt/weston`. However, in this case don't forget to specify `PKG_CONFIG_PATH` when configuring and compiling Wayfire:
```
git clone https://github.com/ammen99/weston && cd weston
./autogen.sh --prefix=/opt/weston
make -j4 && sudo make install
```

Here is a list of the changes included in the repo: [applied patches](https://github.com/ammen99/wayfire/wiki/Libweston-changes)

Now, in order to build wayfire, you'll need cmake and the build dependencies for libweston, which you should already have installed. Simply clone this repo and execute the following commands

```
git clone https://github.com/ammen99/wayfire && cd wayfire
mkdir build && cd build
PKG_CONFIG_PATH=/opt/weston/lib/pkgconfig cmake ../ -DCMAKE_BUILD_TYPE=Release -DUSE_GLES32=False
make -j4 && sudo make install
```
If your system has OpenGL ES 3.2 headers and supports tesselation shaders, then you can also compile with support for it(currently just adds deformation to the cube plugin):
```
git clone https://github.com/ammen99/wayfire && cd wayfire
mkdir build && cd build
PKG_CONFIG_PATH=/opt/weston/lib/pkgconfig cmake ../ -DCMAKE_BUILD_TYPE=Release -DUSE_GLES32=True
make -j4 && sudo make install
```

**Before running Wayfire, copy the default configuration file which is located in the root of the repository and place it in:**
```
cp wayfire.ini.default ~/.config/wayfire.ini
```

You can adjust background, panel properties (font family/size, which launchers to use, etc.) in this file to your liking. To start wayfire, just execute `wayfire` from a TTY. If you encounter any issues, please read [debug report guidelines](https://github.com/ammen99/wayfire/wiki/Debugging-problems) and open a bug in this repo. You can also write in the gitter chat.
# Project status

**IMPORTANT**: Although many of the features one can expect from a WM are implemented, Wayfire should be considered as **WIP** and **pre-alpha**. In my setup it works flawlessly, but this project hasn't been thoroughly tested, so there are a lot of bugs to be expected and to be fixed.

Currently supported:
1. Seamless integration of both native wayland & Xwayland clients
2. Workspaces (or more like viewports if you are familiar with compiz)
3. Configurable bindings
4. Floating and tiling mode(the latter being experimental)
5. Various plugins: Desktop cube, Expo(live workspace previews), Grid(arrange floating windows in a grid), Auto snap at edges, etc. See *plugins*
6. Shell panel with launchers, date, internet connection & battery support
7. Basic touchscreen gestures

Experimental/WIP:
1. On-screen keyboard
2. Tiling window mode
3. Window-rules plugin
4. Output rotation & scaling
5. Multiple outputs - currently supports static configurations, hotplugging is not tested very well 
6. Sane default config

Future plans:
1. Panel improvement
2. Better touchscreen abilities
3. Out-of-tree plugins
4. Packaging & better feature detection when building
5. Wayland and X11 nested backends - currently rendering is "a bit" off, so these are not really usable
6. Anything else


# Contributing to the project

I am currently developing Wayfire alone, but contributions of any kind(bug reports and/or fixes/new features/documentation/whatever) are very welcome. If you want to write your own plugin, I'll try to provide more documentation sometime in the future. In the mean time a general outline of how the plugin system works is here: [plugin architecture](https://github.com/ammen99/wayfire/wiki/Plugin-architecture). You can also have a look at some of the simpler plugins (located in plugins/single_plugins/command.cpp for ex.). Don't hesitate to ask in gitter if you have any questions.
