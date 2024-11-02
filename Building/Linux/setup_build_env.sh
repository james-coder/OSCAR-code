# You might be prompted for your administrator password, if you haven't run sudo before.
# One or more of the install commands will ask for confirmation, so script cannot be run unattended.
sudo apt update
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt-get install build-essential libgl1-mesa-dev
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt-get install qtcreator
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt-get install qttools5-dev
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt install libqt5serialport5
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt-get install libqt5serialport5-dev
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
sudo apt-get install libz-dev
    if [ "$?" -ne 0 ] ; then exit -6 ; fi
