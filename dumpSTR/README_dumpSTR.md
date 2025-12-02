# **dumpSTR**



dumpSTR formats the contents of a STR.EDF file as created by ResMed on their CPAP machines.

dumpSTR is intended for developer use to examine STR.EDF files. It is not be fully supported. It was last successfully compiled and tested in December 2025.

dumpSTR is intended to be run from a command prompt and does not have a graphical user interface. Start the program without parameters for usage instructions.

Source code is located in OSCAR-code/dumpSTR. The Qt project file is located in the OSCAR-code directory.

##### Special  note to Windows users:

The dumpSTR source code uses links to connect to some files in the OSCAR project. 'git clone' does not restore links if you download in user mode. Instead, it creates text files with the needed link in the file. There are two solutions to this problem: 1) start CMD.EXE and run as an Administrator. Links are properly restored by 'git clone' when running as an Administrator. 2) run the fixlinks.bat file in the dumpSTR source directory to replace the text files with the proper links.

