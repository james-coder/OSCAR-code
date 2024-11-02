You will need either qtcreator or qttools5-dev-tools and a C++ compiler and linker

Also, you need the packages qt5-default, libqt5serialport5, libqt5serialport5-dev and libqt5opengl5-dev (see ./setup_build_script.sh)

Oscar can be built without its Help module with Qt 5.8 SDK.

The Help module (The Help texts are incomplete, and language support is incomplete) requires Qt 5.9 SDK,
additionally, the QT modules libqt5help5, qttools5-dev, and qttools5-dev-tools are required.

Linux also needs libudev-dev and zlib1g-dev
The current pre-built downloads use the distribution-supplied version of Qt.

Shadow building is recommneded to avoid cluttering up the git source code folder.

The following does not use the QT creator package, and assumes the directoy structure shown.
After installing the required packages:

$ mkdir OSCAR
$ cd OSCAR
$ git clone https://gitlab.com/CrimsonNape/OSCAR-code.git
$ mkdir build
$ cd build
$ qmake ../OSCAR-code/OSCAR_QT.pro.
$ make

After successful compilation, you can execute in place with ./oscar/OSCAR,
or build an installable package with one of the mkXXXX.sh scripts. It is recommended to create a
Packages folder within OSCAR and copy the scripts and associated files to it to avoid cluttering up the git source tree.
