¡(https://i.ibb.co/s65SmZf/logo.webp)
# What is BennuGD2?

BennuGD2 is a game development engine designed to facilitate the creation of 2D games.


## Features

- SDL2: BennuGD2 utilizes SDL2 (Simple DirectMedia Layer) as its multimedia library, allowing it to efficiently handle graphics, sound, and user input across multiple platforms. SDL2 is widely used in game development and multimedia applications.

- Cross-Platform: The engine supports multiple platforms, including Windows, Linux, and some game consoles. This means developers can create games that run on different operating systems without needing significant modifications to the code.

- Active Community: BennuGD2 has an active community that contributes tutorials, libraries, and resources. This helps with learning and problem-solving for developers using the engine.

- Ease of Use: Its design and features are geared toward allowing developers to focus more on creativity and game design rather than the complexity of the underlying code.

- 2D Graphics Support: BennuGD2 specializes in 2D graphics, making it ideal for platformers, graphic adventures, and other types of games that do not require complex 3D graphics.

- Integration with Other Libraries: In addition to SDL2, BennuGD2 can integrate with other libraries and tools, expanding its capabilities and allowing developers to customize their projects as needed.


# Setup environment

## For Unix

To set up your environment for BennuGD2, add the following environment variables to your shell profile (e.g., .bashrc, .zshrc):
~~~
# BENNUGD2 DEV

# Set the path to your BennuGD2 installation directory
export BGD2DEV=<path_to_your_BennuGD2_installation_directory>

# Setup for Linux systems
export PATH=$BGD2DEV/build/linux-gnu/bin:$PATH
export LD_LIBRARY_PATH=$BGD2DEV/build/linux-gnu/bin:$LD_LIBRARY_PATH

# For other platforms:
# Replace `linux-gnu` in the paths according to the desired platform.

# aarch64-none-elf (Nintendo Switch, unofficial scene version)
# export PATH=$BGD2DEV/build/aarch64-none-elf/bin:$PATH
# export LD_LIBRARY_PATH=$BGD2DEV/build/aarch64-none-elf/bin:$LD_LIBRARY_PATH

# i386-linux-gnu
# export PATH=$BGD2DEV/build/i386-linux-gnu/bin:$PATH
# export LD_LIBRARY_PATH=$BGD2DEV/build/i386-linux-gnu/bin:$LD_LIBRARY_PATH

# i686-w64-mingw32
# export PATH=$BGD2DEV/build/i686-w64-mingw32/bin:$PATH

# x86_64-apple-darwin14 (macOS)
# export PATH=$BGD2DEV/build/x86_64-apple-darwin14/bin:$PATH
# export DYLD_LIBRARY_PATH=$BGD2DEV/build/x86_64-apple-darwin14/bin:$DYLD_LIBRARY_PATH

# x86_64-w64-mingw32
# export PATH=$BGD2DEV/build/x86_64-w64-mingw32/bin:$PATH
~~~

Replace <path_to_your_BennuGD2_installation_directory> with the actual path where BennuGD2 is installed on your system.



## Deployment

How to Build BennuGD2
Unix - Ubuntu/Debian

Install Essential Packages

For Ubuntu, you may need to install the following packages:
~~~
sudo apt install binutils git cmake build-essential zlib1g-dev libsdl2-dev libglu1-mesa-dev libsdl2-image-dev libsdl2-mixer-dev libtheora-dev libogg-dev libvorbis-dev
~~~
For other distributions, install the equivalent packages:

    Fedora:

    sudo dnf install binutils git cmake gcc-c++ zlib-devel SDL2-devel mesa-libGLU-devel SDL2_image-devel SDL2_mixer-devel libtheora-devel libogg-devel libvorbis-devel

Arch Linux:
~~~
sudo pacman -S git cmake gcc zlib sdl2 glu sdl2_image sdl2_mixer libtheora libogg libvorbis
~~~
openSUSE:
~~~
sudo zypper install git cmake gcc-c++ zlib-devel SDL2-devel libglvnd-devel libSDL2_image-devel libSDL2_mixer-devel libtheora-devel libogg-devel libvorbis-devel
~~~
~~~
Clone and Build BennuGD2

Run the following commands to clone the repository and build the dependencies:

git clone https://github.com/SplinterGU/BennuGD2.git

cd BennuGD2/

git submodule update --init --recursive

cd vendor

./build-sdl-gpu.sh linux clean

cd ..
./build.sh linux clean
~~~

Windows 64
~~~
Install MSYS2

    Download the latest version of MSYS2 from the official website.
    Follow the installation instructions provided on the website for your specific operating system.
    Open the MSYS2 terminal to begin the installation of BennuGD2.

Build BennuGD2

Copy and paste the following script into your MSYS2 terminal:

pacman -S --needed --noconfirm git patch cmake mingw-w64-x86_64-toolchain mingw-w64-x86_64-pkg-config mingw-w64-x86_64-cmake mingw-w64-x86_64-make mingw-w64-x86_64-emacs mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-libpng mingw-w64-x86_64-zlib mingw-w64-x86_64-libogg mingw-w64-x86_64-libvorbis mingw-w64-x86_64-libtheora mingw-w64-x86_64-libmodplug mingw-w64-x86_64-libmikmod mingw-w64-x86_64-libtre-git mingw-w64-x86_64-flac mingw-w64-x86_64-openal mingw-w64-x86_64-libxml2 mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libwebp

git clone https://github.com/SplinterGU/BennuGD2.git

cd BennuGD2/

git submodule update --init --recursive

cd vendor

./build-sdl-gpu.sh windows

cd ..
./build.sh windows
cp dependencies/x86_64-w64-mingw32/* build/x86_64-w64-mingw32/bin/
~~~
Note: Adjust the cp command above to match the platform on which the build was performed
# Screenshots
## Windows
![Josekiller: El Ataque de los clones](https://i.ibb.co/KcPnsVY/Captura-de-pantalla-2024-10-10-214452.png)

![Goody: The Remake](https://i.ibb.co/JFhHRvW/Captura-de-pantalla-2024-10-10-220022.png)

## MacOSX
![Marron´s Day](https://i.ibb.co/K74RMck/Captura-de-pantalla-2024-10-10-220434.png)
## Support

https://forum.bennugd.org/


## Authors

- [@SplinterGU](https://www.github.com/SplinterGU)

