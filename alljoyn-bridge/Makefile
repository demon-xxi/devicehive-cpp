# use CROSS_COMPILE variable to set toolchain (empty by default):
#  >make CROSS_COMPILE=arm-linux-gnueabi-

# build variant: 'debug' or 'release' (by default)
# to change use VARIANT variable:
#  >make VARIANT=debug
#  >make VARIANT=release
variant:=release
ifdef VARIANT
  ifeq '${VARIANT}' 'release'
    variant:=release
  else ifeq '${VARIANT}' 'debug'
    variant:=debug
  else
    $(error '${VARIANT}' is unknown variant, expected: release or debug)
  endif
endif

# platform helper
ifdef PLATFORM
  platform=${PLATFORM}
else
  platform:=$(shell uname -m)
endif
ifndef CROSS_COMPILE
  # try to detect CROSS_COMPILE
  ifeq '${platform}' 'arm'
    CROSS_COMPILE=arm-unknown-linux-gnueabi-
  else ifeq '${platform}' 'arm-bb'
    CROSS_COMPILE=arm-angstrom-linux-gnueabi-
  endif
endif


#home_path:=${PWD}
#base_path:=$(shell dirname "${home_path}")
home_path:=.
include_dirs:=-I${home_path}/../include -I${home_path}/../externals/include
ex_libs:=${home_path}/../externals/lib.${platform}

defines+=-DBOOST_SYSTEM_NO_DEPRECATED
# disable SSL for tests
# defines+=-DHIVE_DISABLE_SSL

ifeq '${variant}' 'debug'
  defines+=-D_DEBUG
  defines+=-g
else # default
  defines+=-DNDEBUG
  defines+=-O3
endif


ALLJOYN_LIB := $(ALLJOYN_DIST)/lib/liballjoyn.a
LIBS += -lalljoyn -lstdc++ -lcrypto -lpthread -lrt -lssl -ldl
include_dirs+=-I$(ALLJOYN_DIST)/inc
defines+=-DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86
defines+=-DARDUINO_BRIDGE_ENABLED

CXXFLAGS+=-Wall ${include_dirs} ${defines}
CXXFLAGS+=-fdata-sections -ffunction-sections
LDFLAGS+=-Wl,--gc-sections -pthread -L${ex_libs} -L$(ALLJOYN_DIST)/lib $(LIBS)
#LIBS+=-lboost_system

all: AJ_serial DH_alljoyn

AJ_serial: ${home_path}/AJ_serial.cpp ${home_path}/AJ_serial.hpp
	${CROSS_COMPILE}${CXX} -o AJ_serial ${home_path}/AJ_serial.cpp ${CXXFLAGS} ${LDFLAGS} ${ex_libs}/libboost_system.a
	${CROSS_COMPILE}strip --strip-all AJ_serial

DH_alljoyn: ${home_path}/DH_alljoyn.cpp ${home_path}/DH_alljoyn.hpp
	${CROSS_COMPILE}${CXX} -o DH_alljoyn ${home_path}/DH_alljoyn.cpp ${CXXFLAGS} ${LDFLAGS} ${ex_libs}/libboost_system.a
	${CROSS_COMPILE}strip --strip-all DH_alljoyn

YUN_PACKAGE=${home_path}/yun.build
yun_package: AJ_serial DH_alljoyn
	@rm -rf ${YUN_PACKAGE}
	@mkdir -p ${YUN_PACKAGE}/usr/lib
	@cp -r ${home_path}/yun-ipkg/* ${YUN_PACKAGE}/
	@cp ${home_path}/DH_alljoyn ${YUN_PACKAGE}/usr/bin
	@cp ${home_path}/AJ_serial ${YUN_PACKAGE}/usr/bin
	@cp ${ALLJOYN_DIST}/lib/liballjoyn.so ${YUN_PACKAGE}/usr/lib/
	@cp ${ALLJOYN_DIST}/bin/alljoyn-daemon ${YUN_PACKAGE}/usr/bin/
	@chmod +x ${YUN_PACKAGE}/etc/init.d/devicehive-alljoyn
	@chmod +x ${YUN_PACKAGE}/etc/init.d/devicehive-gateway
	@chmod +x ${YUN_PACKAGE}/etc/init.d/devicehive-bridge
	@chmod +x ${YUN_PACKAGE}/usr/bin/wait_inet.sh
	@chmod +x ${YUN_PACKAGE}/CONTROL/preinst
	@chmod +x ${YUN_PACKAGE}/CONTROL/postinst
	@chmod +x ${YUN_PACKAGE}/CONTROL/prerm
	@chmod +x ${YUN_PACKAGE}/CONTROL/postrm
	@chmod +x ${home_path}/ipkg-build
	@${home_path}/ipkg-build -o root -g root ${YUN_PACKAGE}

RPI_PACKAGE=${home_path}/rpi.build
rpi_package: AJ_serial DH_alljoyn
	@rm -rf ${RPI_PACKAGE}
	@mkdir -p ${RPI_PACKAGE}/usr/bin
	@mkdir -p ${RPI_PACKAGE}/usr/lib
	@cp -r ${home_path}/rpi-dpkg/* ${RPI_PACKAGE}/
	@cp ${home_path}/DH_alljoyn ${RPI_PACKAGE}/usr/bin/
	@cp ${home_path}/AJ_serial ${RPI_PACKAGE}/usr/bin/
	@cp ${ALLJOYN_DIST}/lib/liballjoyn.so ${RPI_PACKAGE}/usr/lib/
	@cp ${ALLJOYN_DIST}/bin/alljoyn-daemon ${RPI_PACKAGE}/usr/bin/
	@cp ${home_path}/gateway_manager.py ${RPI_PACKAGE}/usr/bin/
	@chmod +x ${RPI_PACKAGE}/etc/init.d/devicehive-alljoyn
	@chmod +x ${RPI_PACKAGE}/etc/init.d/devicehive-gateway
	@chmod +x ${RPI_PACKAGE}/etc/init.d/gateway_manager
	@chmod +x ${RPI_PACKAGE}/usr/bin/gateway_manager.py
	@dpkg-deb --build ${RPI_PACKAGE} devicehive.RPi.deb

X86_PACKAGE=${home_path}/rpi.build
x86_package: AJ_serial DH_alljoyn
	@rm -rf ${X86_PACKAGE}
	@mkdir -p ${X86_PACKAGE}/usr/bin
	@mkdir -p ${X86_PACKAGE}/usr/lib
	@cp -r ${home_path}/x86-dpkg/* ${X86_PACKAGE}/
	@cp ${home_path}/DH_alljoyn ${X86_PACKAGE}/usr/bin/
	@cp ${home_path}/AJ_serial ${X86_PACKAGE}/usr/bin/
	@cp ${ALLJOYN_DIST}/lib/liballjoyn.so ${X86_PACKAGE}/usr/lib/
	@cp ${ALLJOYN_DIST}/bin/alljoyn-daemon ${X86_PACKAGE}/usr/bin/
	@cp ${home_path}/gateway_manager.py ${X86_PACKAGE}/usr/bin/
	@chmod +x ${X86_PACKAGE}/etc/init.d/devicehive-alljoyn
	@chmod +x ${X86_PACKAGE}/etc/init.d/devicehive-gateway
	@chmod +x ${X86_PACKAGE}/etc/init.d/gateway_manager
	@chmod +x ${X86_PACKAGE}/usr/bin/gateway_manager.py
	@dpkg-deb --build ${X86_PACKAGE} devicehive.i386.deb


#########################################################
# clean all the object files and applications
clean:
	@rm -rf *.o
	@rm -f AJ_serial DH_alljoyn
	@rm -f ${PCH_objects}


# PCH header files
PCH_sources+=${home_path}/hive/pch.hpp
PCH_objects:=${PCH_sources:.hpp=.hpp.gch}
PCH: ${PCH_objects}


# rule to build gch files
%.hpp.gch: %.hpp
	@echo "[CXX] $<"
	@${CROSS_COMPILE}${CXX} ${CXXFLAGS} -c $< -o $@
%.h.gch: %.h
	@echo " [CC] $<"
	@${CROSS_COMPILE}${CC} ${CXXFLAGS} -c $< -o $@

# rules to build c/cpp files
%.o: %.cpp
	@echo "[CXX] $<"
	@${CROSS_COMPILE}${CXX} ${CXXFLAGS} -c $< -o $@
%.o: %.c
	@echo " [CC] $<"
	@${CROSS_COMPILE}${CC} ${CXXFLAGS} -c $< -o $@


.PHONY: clean
