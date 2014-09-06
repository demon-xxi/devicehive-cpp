export ALLJOYN_DIST=~/alljoyn/alljoyn_core/build/linux/arm/release/dist/cpp

export CROSS_COMPILE=" "
export PLATFORM=arm

make rpi_package

#DEST=~/demo
#mkdir -p $DEST
#cp -fv ./AJ_serial ./DH_alljoyn $DEST/
#cp -fv $ALLJOYN_DIST/lib/liballjoyn.so $DEST/
#cp -fv $ALLJOYN_DIST/bin/alljoyn-daemon $DEST/
#cp -fv /usr/lib/arm-linux-gnueabihf/libstdc++.so.6 $DEST/

#tar -cvf $DEST/demo.tar $DEST/DH_alljoyn $DEST/AJ_serial $DEST/liballjoyn.so $DEST/alljoyn-daemon
