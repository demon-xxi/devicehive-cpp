export STAGING_DIR=/data/linino/openwrt-yun/staging_dir/

export ALLJOYN_DIST=/data/alljoyn/core/alljoyn/build/openwrt/openwrt/release/dist/cpp

export CROSS_COMPILE=mips-openwrt-linux-uclibc-
export PLATFORM=mips

export TARGET_MIPS=$STAGING_DIR/target-mips_r2_uClibc-0.9.33.2
export TOOLCHAIN_MIPS=$STAGING_DIR/toolchain-mips_r2_gcc-4.6-linaro_uClibc-0.9.33.2
export CXXFLAGS=-I$TARGET_MIPS/usr/include
export LIBS=-L$TARGET_MIPS/usr/lib
export PATH=$PATH:$TOOLCHAIN_MIPS/bin

make yun_package

#DEST=/home/dataart/demo
#mkdir -p $DEST
#cp -fv ./AJ_serial ./DH_alljoyn $DEST/
#cp -fv $ALLJOYN_DIST/lib/liballjoyn.so $DEST/
#cp -fv $ALLJOYN_DIST/bin/alljoyn-daemon $DEST/
#cp -fv $TOOLCHAIN_MIPS/lib/libstdc++.so.6 $DEST/


#tar -cvf $DEST/demo.tar $DEST/DH_alljoyn $DEST/AJ_serial $DEST/liballjoyn.so $DEST/alljoyn-daemon $DEST/libstdc++.so.6
