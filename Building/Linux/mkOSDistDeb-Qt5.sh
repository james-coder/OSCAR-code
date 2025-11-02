#! /bin/bash
# No parameter is not required
# This script will identify the distribution and release version
#

function gene_script () {
  # generate script shell from 2 files
  # clean_rm
  if [ -f "clean_rm-result-NN.sh" ]; then
    rm clean_rm-result-NN.sh
  fi
  cat clean_rm-NN.sh clean_rm-common-NN.sh > clean_rm-result-NN.sh
  chmod +x clean_rm-result-NN.sh

  if [ -f "clean_rm-result-NN-test.sh" ]; then
    rm clean_rm-result-NN-test.sh
  fi
  cat clean_rm-NN-test1.sh clean_rm-common-NN.sh clean_rm-NN-test2.sh clean_rm-common-NN.sh > clean_rm-result-NN-test.sh
  chmod +x clean_rm-result-NN-test.sh

  # ln_usrbin

  if [ -f "ln_usrbin-result-NN.sh" ]; then
    rm ln_usrbin-result-NN.sh
  fi
  cat ln_usrbin-NN.sh ln_usrbin-common-NN.sh > ln_usrbin-result-NN.sh
  chmod +x ln_usrbin-result-NN.sh

  if [ -f "ln_usrbin-result-NN-test.sh" ]; then
    rm ln_usrbin-result-NN-test.sh
  fi
  cat ln_usrbin-NN-test.sh ln_usrbin-common-NN.sh > ln_usrbin-result-NN-test.sh
  chmod +x ln_usrbin-result-NN-test.sh

  # rm_usrbin
  if [ -f "rm_usrbin-result-NN.sh" ]; then
    rm rm_usrbin-result-NN.sh
  fi
  cat rm_usrbin-NN.sh rm_usrbin-common-NN.sh > rm_usrbin-result-NN.sh
  chmod +x rm_usrbin-result-NN.sh

  if [ -f "rm_usrbin-result-NN-test.sh" ]; then
    rm rm_usrbin-result-NN-test.sh
  fi
  cat rm_usrbin-NN-test.sh rm_usrbin-common-NN.sh > rm_usrbin-result-NN-test.sh
  chmod +x rm_usrbin-result-NN-test.sh
}

function getOS () {
  rel=$(lsb_release -r | awk '{print $2}')
  os=$(lsb_release -i | awk '{print $3}')
  tmp2=${os:0:3}
  echo "tmp2 = '$tmp2'"
  if [ "$tmp2" = "Ubu" ] ; then
    OSNAME=$os${rel:0:2}
  elif [ "$tmp2" = "Deb" ];then
    OSNAME=$os$rel
  elif [ "$tmp2" = "Ras" ];then
    OSNAME="RasPiOS"
  else
    OSNAME="unknown"
  fi
}

function getPkg () {
    unset PKGNAME
    unset PKGVERS
    while read stat pkg ver other ;
        do
            if [[ ${stat} == "ii" ]] ; then
                PKGNAME=`awk -F: '{print $1}' <<< ${pkg}`
                PKGVERS=`awk -F. '{print $1 "." $2}' <<< ${ver}`
                break
            fi ;
        done <<< $(dpkg -l | grep $1)
}

# generate the script from sources
gene_script

ITERATION=$1
if [  -z ${ITERATION} ]; then
    ITERATION="1"
fi

#SRC=/home/$USER/OSCAR/OSCAR-code/oscar
SRC="$(pwd)/../../oscar"

VERSION=`awk '/#define VERSION / { gsub(/"/, "", $3); print $3 }' ${SRC}/VERSION`
if [[ ${VERSION} == *-* ]]; then
    # Use ~ for prerelease information so that it sorts correctly compared to release
    # versions. See https://www.debian.org/doc/debian-policy/ch-controlfields.html#version
    IFS="-" read -r VERSION PRERELEASE <<< ${VERSION}
    if [[ ${PRERELEASE} == *rc* ]]; then
        RC=1
    fi
    VERSION="${VERSION}-${PRERELEASE}"
fi
GIT_REVISION=`awk '/#define GIT_REVISION / { gsub(/"/, "", $3); print $3 }' ${SRC}/git_info.h`
echo Version: ${VERSION}

# application name
appli_name="OSCAR"
package_name="oscar"
pre_inst="tst_user.sh"
# build folder (absolute path is better)
build_folder="/home/$USER/OSCAR/build"
if [[ -n ${PRERELEASE}  && -z ${RC} ]] ; then
    appli_name=${appli_name}-test
    package_name=${package_name}-test
    post_inst="ln_usrbin-result-NN-test.sh"
    pre_rem="rm_usrbin-result-NN-test.sh"
    post_rem="clean_rm-result-NN-test.sh"
else
    post_inst="ln_usrbin-result-NN.sh"
    pre_rem="rm_usrbin-result-NN.sh"
    post_rem="clean_rm-result-NN.sh"
fi

# temporary folder (absolute path is better)
temp_folder="/home/$USER/tmp_deb_${appli_name}/"

# destination folder in the .deb file
dest_folder="/usr/"

# the .deb file mustn't exist
archi_tmp=$(lscpu | grep -i architecture | awk -F: {'print $2'} | tr -d " ")
echo "archi_tmp = '${archi_tmp}'"
if [ "$archi_tmp" = "x86_64" ];then
    archi="amd64"
 elif [ "$archi_tmp" = "armv7l" ];then
   archi="armhf"
elif [ "$archi_tmp" = "aarch64" ];then
   archi="arm64"
elif [ "$archi_tmp" = "i686" ];then
   archi="i386"
elif [ "$archi_tmp" = "arm64" ];then
   archi="arm64"
else
    archi="arch_unknown"
fi

# detection of the OS with version for Ubuntu
getOS
echo "osname='$OSNAME'"

deb_file="${package_name}_${VERSION}-${OSNAME}_$archi-Qt5.deb"

# if deb file exists, fatal error
if [ -f "./$deb_file" ]; then
    echo "destination file (./$deb_file) exists. fatal error"
    exit
fi

## retrieve packages version for the dependencies
getPkg libqt5core
qtver=$PKGVERS
corePkg=$PKGNAME

getPkg libdouble
dblPkg=$PKGNAME

getPkg libpcre
libpcrePkg=$PKGNAME

echo "QT name version " $corePkg $qtver
echo "DblConv package " $dblPkg
echo "libcrePkg package " $libpcrePkg

# clean folders need to create the package
if [ -d "${temp_folder}" ]; then
    rm -r ${temp_folder}
fi
mkdir ${temp_folder}
if [ ! -d "${temp_folder}" ]; then
    echo "Folder (${temp_folder}) not created : fatal error."
    exit
fi
chmod 0755 ${temp_folder}
# save current value of umask (for u=g and not g=o)
current_value=$(umask)
umask 022
mkdir ${temp_folder}/bin
mkdir ${temp_folder}/share
mkdir ${temp_folder}/share/${appli_name}
mkdir ${temp_folder}/share/doc
share_doc_folder="${temp_folder}/share/doc/${package_name}"
mkdir ${share_doc_folder}
mkdir ${temp_folder}/share/icons
mkdir ${temp_folder}/share/icons/hicolor
mkdir ${temp_folder}/share/icons/hicolor/48x48
mkdir ${temp_folder}/share/icons/hicolor/48x48/apps
mkdir ${temp_folder}/share/icons/hicolor/scalable
mkdir ${temp_folder}/share/icons/hicolor/scalable/apps
mkdir ${temp_folder}/share/applications

# must delete debug symbol in OSCAR binary file
strip -s -o ${temp_folder}/bin/${appli_name} ${build_folder}/oscar/OSCAR

# 2>/dev/null : errors does not appear : we don't care about them
cp -r ${build_folder}/oscar/Help ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Html ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Translations ${temp_folder}/share/${appli_name} 2>/dev/null
cp ./${appli_name}.png ${temp_folder}/share/icons/hicolor/48x48/apps/${appli_name}.png
cp ./${appli_name}.svg ${temp_folder}/share/icons/hicolor/scalable/apps/${appli_name}.svg
cp ./${appli_name}.desktop ${temp_folder}/share/applications/${appli_name}.desktop

#echo "Copyright 2019-2020 oscar-team.org <oscar@oscar-team.org>" > $share_doc_folder/copyright
#echo "Licensed under /usr/share/common-licenses/GPL-3" >> $share_doc_folder/copyright
cp ./copyright $share_doc_folder/copyright

changelog_file="./changelog"

#automatic changelog as a bad name
# need to generate one and say fpm to use it instead of create one
# it seems that it needs both of them...

# creation of the Debian changelog
echo "$appli_name (${VERSION}-${ITERATION}) whatever; urgency=medium" > $changelog_file
echo "" >> $changelog_file
echo "  * Package created with FPM." >> $changelog_file
echo "" >> $changelog_file
echo "  * See the Release Notes under Help/About menu" >> $changelog_file
echo "" >> $changelog_file
echo -n " -- oscar-team.org <oscar@oscar-team.org> " >> $changelog_file
date -Iminutes >> $changelog_file
cp $changelog_file $share_doc_folder/changelog
gzip --best $share_doc_folder/changelog

description='Open Source CPAP Analysis Reporter\nProvides graphical and statistical display of the CPAP stored data'
# trick for dummies : need to use echo -e to take care of \n (cariage return to slipt description and extra one)
description=$(echo -e $description)

echo "appli (replaces) : '${package_name}'"

# restore umask value
umask $current_value

# create the .deb file (litian test show juste a warning with a man that doesn't exists : don't care about that)
fpm --input-type dir --output-type deb  \
    --prefix ${dest_folder} \
    --before-install ${pre_inst} \
    --after-install ${post_inst}   \
    --before-remove ${pre_rem}   \
    --after-remove ${post_rem} \
    --name ${package_name} --version ${VERSION} --iteration ${OSNAME} \
    --category misc               \
    --deb-priority optional \
    --maintainer " -- oscar-team.org <oscar@oscar-team.org>"   \
    --license GPL3+             \
    --vendor oscar-team.org    \
    --description "$description" \
    --url https://sleepfiles.com/OSCAR  \
    --replaces "${package_name} ( << ${VERSION})" \
    --deb-no-default-config-files   \
    --depends $dblPkg \
    --depends ${libpcrePkg} \
    --depends qttranslations5-l10n \
    --depends "${corePkg} >= ${qtver}"   \
    --depends libqt5serialport5     \
    --depends libqt5xml5            \
    --depends libqt5network5        \
    --depends libqt5gui5            \
    --depends libqt5widgets5        \
    --depends libqt5opengl5         \
    --depends libqt5printsupport5   \
    --depends libglu1-mesa          \
    --depends libgl1                \
    --depends libc6                 \
    --no-deb-generate-changes \
    -C ${temp_folder} \
    -p ${deb_file} \
    .

# Suppress the *result* files : if not, it can make trouble with git
rm *result*
