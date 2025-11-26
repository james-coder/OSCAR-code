# begin common script
#
# modify by untoutseul05 to search local name for Desktop Folder
# the package now suits the fhs

# application name
echo "appli_name='$appli_name', SUDO_USER='$SUDO_USER'"

if [ ! -z "$SUDO_USER" ]; then
    # find real name of the Desktop folder (Bureau for xubuntu french version)
    desktop_folder_name="/home/$SUDO_USER/Desktop"

    # if doesn't exist, try to find it translated name
    tmp_dir="" 
    if [ ! -d "$desktop_folder_name" ]; then
        tmp_dir=`cat /home/$SUDO_USER/.config/user-dirs.dirs | grep XDG_DESKTOP_DIR | awk -F= '{print $2}' | awk -F\" '{print $2}' | awk -F\/ '{print $2}'`
    fi

# if doesn't exist, try to find it translated name
    translate_file="/home/$SUDO_USER/.config/user-dirs.dirs"
    if [ -f $translate_file ]; then
        tmp_dir="" 
        if [ ! -d "$desktop_folder_name" ]; then
            tmp_dir=`cat /home/$translate_file | grep XDG_DESKTOP_DIR | awk -F= '{print $2}' | awk -F\" '{print $2}' | awk -F\/ '{print $2}'`
        fi

       # don't overwrite if translated name or doesn't exist
        if [ -n "$tmp_dir" ];  then
            # calculate the full folder
            tmp_dir_full="/home/${SUDO_USER}/${tmp_dir}"
            if [ -d "$tmp_dir_full" ]; then
                desktop_folder_name=$tmp_dir_full
            fi
        fi
    fi

    # info : /usr/share/applications/${appli_name}.desktop
    # copy icon file to the Desktop folder (even if it has been translated)
    file_from="/usr/share/applications/${appli_name}.desktop"
    file_to="$desktop_folder_name/${appli_name}.desktop"

    cp $file_from $file_to

    if [ -f "$file_to" ]; then
        chown $SUDO_USER:$SUDO_USER $file_to
        chmod a+x $file_to
    fi
fi

# end common script


