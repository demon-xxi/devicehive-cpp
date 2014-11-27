TEMPLATE=app
TARGET=bridge

DEFINES += BOOST_SYSTEM_NO_DEPRECATED
#DEFINES += HIVE_DISABLE_SSL
INCLUDEPATH += $${PWD}/../include $${PWD}/../externals/include

AJ_ROOT=/home/dataart/alljoyn
AJ_CORE_CPP=$${AJ_ROOT}/build/linux/x86/debug/dist/cpp

INCLUDEPATH += $${AJ_CORE_CPP}/inc
DEFINES += QCC_OS_LINUX QCC_OS_GROUP_POSIX QCC_CPU_X86

HEADERS += $${PWD}/../include/DeviceHive/gateway.hpp
HEADERS += $${PWD}/../include/DeviceHive/restful.hpp
HEADERS += $${PWD}/../include/DeviceHive/service.hpp
HEADERS += $${PWD}/../include/DeviceHive/websocket.hpp
HEADERS += $${PWD}/../include/hive/bin.hpp
HEADERS += $${PWD}/../include/hive/http.hpp
HEADERS += $${PWD}/../include/hive/log.hpp

HEADERS += $${PWD}/AJ_serial.hpp
#SOURCES += $${PWD}/AJ_serial.cpp
HEADERS += $${PWD}/DH_alljoyn.hpp
#SOURCES += $${PWD}/DH_alljoyn.cpp
HEADERS += $${PWD}/hexUtils.hpp
SOURCES += $${PWD}/test_obj.cpp

LIBS += -L$${AJ_CORE_CPP}/lib -lalljoyn \
    -L$${AJ_CORE_CPP}/lib -lalljoyn_about \
    $${PWD}/../externals/lib.i686/libboost_system.a \
   -lstdc++ -lcrypto -lssl -lpthread -lrt


OTHER_FILES += $${PWD}/README
