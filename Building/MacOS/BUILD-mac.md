# OSCAR Build Instructions for Mac

## Prerequisites

- [Qt 5] 
- [macOS 10.12 Sierra] or higher for building (recommended: Qt 5.12)
- Xcode command line tools. (Download instructions are below.)
- Xcode - Website for the most current version for your mac distro: https://xcodereleases.com/

NOTE: Official Qt5 builds are currently made with [macOS 10.14 Catalina] and Command-Line Tools for [Xcode] 12.4.

- [Qt 6] 
- [macOS 13 Ventura] or higher for building (recommended: Qt 6.9 or higher) 
- Xcode command line tools. (Download instructions are below.)
- Xcode - Website for the most current version for your mac distro: https://xcodereleases.com/


## Setup
1. Apply all updates to your OS.  

2. (Optional) Install Xcode.  
    A. Open the Xcode's .xip file to extract it with Archive Utility, or double-click on the file itself.  
    B. Delete the .xip archive.  
    C. Move Xcode.app into /Applications.  
    D. Launch Xcode.app and agree to the license.  
    E. Uncheck "Show this window..." and close the window.  
    F. Xcode > Quit  
 
3. Install the command-line developer tools, approx. 0.6GB:  
    A. Launch Terminal.app and run:  
>		    sudo xcode-select --install  

   B. Click "Install"  
   C. Click "Agree"  

   This will download and install the latest version of the Command-Line Tools for Xcode for your version of macOS, without requiring a developer account.  

   _Alternatively, the command-line tools installer .dmg can be downloaded from the [Xcode] download site, but you will need a (free) developer account and will need to pick the appropriate download for your version of macOS.  

4. Install the Qt Opensource IDE platform  
    A. Use the Qt Online Opensource Installer: https://www.qt.io/download-qt-installer-oss.  
    
    B. Select the version you wish.  For QT5 versions, you may need to search the Archive section.  
    
    C. You do not need the following packages  
	-    Sources  
	-    iOS   
	-    Any web development package  
	
    D. Additional Libraries needed:  Qt Serial Port  

5. Install Qt   
    A. Mount and launch qt-opensource-mac dmg file.  This can be done by double-clicking on the file name.  
    B. Follow the installer's instructions. (optionally you can change the installation directory)  
        * Qt is entirely self-contained and can be installed   anywhere. It defaults to the ~/Qt/version number directory.  
        * If you only have the command-line tools installed, the Qt installer will complain that "You need to install Xcode and set up Xcode command line tools." Simply click OK.  
    C. Expand Qt select "macOS", Continue  
    D. Select "I have read and agree..." and Continue, Install  
    E. Uncheck "Launch Qt Creator", Done  
    F. Eject qt-opensource-mac installer file.  

## Build

1. Build OSCAR from the command line (All Qt versions - except where noted):  

    Launch Terminal.app  
    Type the following commands:  
> 	      mkdir YourDirectoryName  
> 	      cd YourDirectoryName  
> 	      git clone https://gitlab.com/CrimsonNape/OSCAR-code.git  
> 	      mkdir build  
> 	      cd build  
> 	      Qt5:  ~/Qt/Qt version number/clang_64/bin/qmake ../OSCAR-code/oscar/oscar.pro  
> 	      Qt6:  ~/Qt/Qt version number/macos/bin/qmake ../OSCAR-code/oscar/oscar.pro  
> 	      make  

2. (Optional) Package for distribution:

>        make dist-mac  

   The dmg is at OSCAR.dmg in the build directory.  

## (Optional) Using Qt Creator

1. Launch Terminal.app  
2. Type the following commands:  
>             mkdir YourDirectoryName  
>             cd YourDirectoryName  
>             git clone https://gitlab.com/CrimsonNape/OSCAR-code.git  
>             Exit Terminal app.  

3. Launch Qt Creator (located in the App folder or the Qt folder).  
4. File > Open File or Project... and select ~/YourDirectoryName/OSCAR-code/oscar/oscar.pro (or wherever you cloned it above), then click "Configure Project".  
5. Configure building:  
    A. Click on "Projects" in the left panel.  
    B. Under **Build Settings**, in the "Edit build configuration" drop-down menu, select "Release".  
    C. Click to expand "Details" for the **qmake** build step.  
    D. Uncheck "Enable Qt Quick Compiler", click "No" to defer recompiling.  
6. Configure packaging for distribution:  
    A. Click "Clone..." to the right of the "Edit build configuration" drop-down menu.  
    B. Name the new configuration "Deploy".  
    C. Click to expand "Details" for the **Make** build step.  
    D. Set the Make arguments for the Make step to "dist-mac".  
7. To build OSCAR, select "Release" from the "oscar" button in the left panel. Then select Build > Build Project "oscar". The application is in OSCAR.app.  
8. To build OSCAR and package for distribution, select "Deploy" from the "oscar" button in the left panel. Then select Build > Build Project "oscar". The dmg is at OSCAR.dmg.  
    * Progress in "Compile Output" will pause for several seconds while "Creating .dmg". This is normal.  

[Qt 5.12.8]: http://download.qt.io/archive/qt/5.12/5.12.8/qt-opensource-mac-x64-5.12.8.dmg  



