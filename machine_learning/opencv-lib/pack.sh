#!/bin/bash
TARGET_PATH="opencv_target"
TARGET_LIB_PATH=$TARGET_PATH/lib
TARGET_SHARE_PATH=$TARGET_PATH/share

INSTALL_PATH="opencv-aarch64_install"
INSTALL_LIB_PATH=$INSTALL_PATH/lib
INSTALL_SHARE_PATH=$INSTALL_PATH/share

source /usr/local/oecore-x86_64/environment-setup-aarch64-poky-linux

mkdir -p $TARGET_PATH

cp -rf $INSTALL_LIB_PATH $TARGET_PATH
cp -rf $INSTALL_SHARE_PATH $TARGET_PATH


opencv_libs=`ls ${TARGET_LIB_PATH}/*.so`

#update library rpath to /usr/lib when create install package
#sudo apt install patchelf

for each_share_lib in $opencv_libs
do
	aarch64-poky-linux-strip $each_share_lib
	patchelf --set-rpath '/usr/lib' $each_share_lib
done

tar -czvf $TARGET_PATH.tar.gz $TARGET_PATH
rm -rf $TARGET_PATH
