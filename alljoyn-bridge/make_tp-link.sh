export STAGING_DIR=/home/dataart/openwrt.tp-link/staging_dir
export ALLJOYN_DIST=/home/dataart/alljoyn/alljoyn_core/build/openwrt/openwrt/release/dist/cpp

export CROSS_COMPILE=mips-openwrt-linux-
export PLATFORM=mips-yun

export TARGET_MIPS=$STAGING_DIR/target-mips_34kc_uClibc-0.9.33.2
export TOOLCHAIN_MIPS=$STAGING_DIR/toolchain-mips_34kc_gcc-4.8-linaro_uClibc-0.9.33.2
export CXXFLAGS=-I$TARGET_MIPS/usr/include
export LIBS=-L$TARGET_MIPS/usr/lib
export PATH=$PATH:$TOOLCHAIN_MIPS/bin

make yun_package

