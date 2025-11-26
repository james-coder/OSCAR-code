
if [ ! -z "$SUDO_USER" ]; then
    # find real name of the Desktop folder (Bureau for xubuntu french version)
    desktop_folder_name="/home/$SUDO_USER/Desktop"

    # si doesn't exist, try to find it translated name 
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

    # don't overwrite if tmp_dir empty or doesn't exist
    if [ -n "$tmp_dir" ];  then
        # calculate the full folder
        tmp_dir_full="/home/${SUDO_USER}/${tmp_dir}"
        if [ -d "$tmp_dir_full" ]; then
            desktop_folder_name=$tmp_dir_full
        fi
    fi

    file="$desktop_folder_name/${appli_name}.desktop"
    if [ -f "$file" ]; then
        rm $file
    fi
fi

# clean destination folders (bin + shortcut) + move from /opt/OSCAR (if exists)

if [ -d "/opt/${appli_name}" ]; then
    # --- old folder exists : move files to the new folder ---
    # Help folder
    folder_from="/opt/${appli_name}/Help"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # Html folder
    folder_from="/opt/${appli_name}/Html"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # Translations folder
    folder_from="/opt/${appli_name}/Translations"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # icon file : OSCAR.png
    file="/opt/${appli_name}/OSCAR.png"
    folder_to="/usr/share/icons/${appli_name}"
    if [ -f "$file" ]; then
        mkdir -p $folder_to
        mv $file ${folder_to}/${appli_name}.png
    fi
    # shortcut file : OSCAR.desktop
    file="/opt/${appli_name}/OSCAR.desktop"
    folder_to="/usr/share/applications/${appli_name}"
    if [ -f "$file" ]; then
        mkdir -p $folder_to
        mv $file ${folder_to}/${appli_name}.desktop
    fi

    # folder /opt can be deleted
    rm -R /opt/${appli_name}
fi

# clean the destination folder
file="/usr/bin/${appli_name}"
if [ -f "$file" ]; then
    rm $file
fi



