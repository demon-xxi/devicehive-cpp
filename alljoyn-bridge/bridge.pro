TEMPLATE=app
TARGET=bridge

DEFINES += BOOST_SYSTEM_NO_DEPRECATED
DEFINES += HIVE_DISABLE_SSL
INCLUDEPATH += $${PWD}/../include $${PWD}/../externals/include

INCLUDEPATH += /home/dataart/alljoyn/build/linux/x86/debug/dist/cpp/inc
DEFINES += QCC_OS_LINUX QCC_OS_GROUP_POSIX QCC_CPU_X86

HEADERS += $${PWD}/../include/DeviceHive/gateway.hpp
HEADERS += $${PWD}/../include/DeviceHive/restful.hpp
HEADERS += $${PWD}/../include/DeviceHive/service.hpp
HEADERS += $${PWD}/../include/DeviceHive/websocket.hpp
HEADERS += $${PWD}/../include/hive/bin.hpp
HEADERS += $${PWD}/../include/hive/http.hpp
HEADERS += $${PWD}/../include/hive/log.hpp

HEADERS += $${PWD}/AJ_serial.hpp
SOURCES += $${PWD}/AJ_serial.cpp
HEADERS += $${PWD}/DH_alljoyn.hpp
#SOURCES += $${PWD}/DH_alljoyn.cpp
HEADERS += $${PWD}/hexUtils.hpp

LIBS += /home/dataart/alljoyn/build/linux/x86/debug/dist/cpp/lib/liballjoyn.a \
    -L/home/dataart/alljoyn/build/linux/x86/debug/dist/cpp/lib \
    $${PWD}/../externals/lib.i686/libboost_system.a -lalljoyn -lstdc++ -lcrypto -lpthread -lrt


OTHER_FILES += $${PWD}/README
