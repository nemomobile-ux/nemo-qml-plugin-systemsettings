TEMPLATE = lib
 equals(QT_MAJOR_VERSION, 5):TARGET = systemsettings
 equals(QT_MAJOR_VERSION, 6):TARGET = systemsettings-qt6

CONFIG += qt create_pc create_prl no_install_prl
QT +=  core qml dbus
QT -= gui

CONFIG += hide_symbols link_pkgconfig

PKGCONFIG += profile mlite$${QT_MAJOR_VERSION} mce timed-qt$${QT_MAJOR_VERSION} blkid libcrypto connman-qt$${QT_MAJOR_VERSION} glib-2.0
PKGCONFIG += libsystemd qofono-qt$${QT_MAJOR_VERSION}

CONFIG(DEVELOPER_MODE_ENABLED) {
    message("Developer mode plugin enabled")
    PKGCONFIG += packagekitqt5
    DEFINES += DEVELOPER_MODE_ENABLED
} else {
    warning("Developer mode plugin disabled")
}

CONFIG(SFOS_USER_MODE) {
    message("Users managmend plugin enabled")
    PKGCONFIG += sailfishusermanager sailfishaccesscontrol
    DEFINES += USER_MODE_ENABLED
} else {
    warning("User managment plugin disabled")
}

CONFIG(SAILFISHKEYPROVIDER_ENABLED) {
    message("Sailfish key provider enabled")
    PKGCONFIG += libsailfishkeyprovider
    DEFINES += SAILFISHKEYPROVIDER_ENABLED
} else {
    warning("Sailfish key provider disabled")
}

CONFIG(USE_SSU) {
    PKGCONFIG += ssu-sysinfo
    DEFINES += HAS_SSUSYSINFO
}

system($$[QT_INSTALL_BINS]/qdbusxml2cpp -p mceiface.h:mceiface.cpp mce.xml)

SOURCES += \
    languagemodel.cpp \
    localeconfig.cpp \
    logging.cpp \
    datetimesettings.cpp \
    nfcsettings.cpp \
    profilecontrol.cpp \
    alarmtonemodel.cpp \
    mceiface.cpp \
    displaysettings.cpp \
    aboutsettings.cpp \
    certificatemodel.cpp \
    batterystatus.cpp \
    partition.cpp \
    partitionmanager.cpp \
    partitionmodel.cpp \
    deviceinfo.cpp \
    locationsettings.cpp \
    timezoneinfo.cpp \
    udisks2block.cpp \
    udisks2blockdevices.cpp \
    udisks2job.cpp \
    udisks2monitor.cpp \
    permissionsmodel.cpp

PUBLIC_HEADERS = \
    languagemodel.h \
    datetimesettings.h \
    profilecontrol.h \
    alarmtonemodel.h \
    mceiface.h \
    displaysettings.h \
    aboutsettings.h \
    certificatemodel.h \
    batterystatus.h \
    udisks2block_p.h \
    udisks2defines.h \
    partition.h \
    partitionmanager.h \
    partitionmodel.h \
    systemsettingsglobal.h \
    deviceinfo.h \
    locationsettings.h \
    timezoneinfo.h \
    permissionsmodel.h

HEADERS += \
    $$PUBLIC_HEADERS \
    aboutsettings_p.h \
    localeconfig.h \
    batterystatus_p.h \
    logging_p.h \
    locationsettings_p.h \
    logging_p.h \
    nfcsettings.h \
    partition_p.h \
    partitionmanager_p.h \
    udisks2blockdevices_p.h \
    udisks2job_p.h \
    udisks2monitor_p.h

CONFIG(DEVELOPER_MODE_ENABLED) {
    SOURCES += developermodesettings.cpp
    PUBLIC_HEADERS += developermodesettings.h
}

CONFIG(USER_MODE_ENABLED) {
    SOURCES +=  userinfo.cpp \
                usermodel.cpp
    PUBLIC_HEADERS += userinfo.h \
                      usermodel.h
    HEADERS += userinfo_p.h
}

DEFINES += \
    SYSTEMSETTINGS_BUILD_LIBRARY

equals(QT_MAJOR_VERSION, 5): develheaders.path = /usr/include/systemsettings
equals(QT_MAJOR_VERSION, 6): develheaders.path = /usr/include/systemsettings-qt6
develheaders.files = $$PUBLIC_HEADERS

target.path = $$[QT_INSTALL_LIBS]
pkgconfig.files = $$PWD/pkgconfig/systemsettings.pc
pkgconfig.path = $$target.path/pkgconfig

locationconfig.files = $$PWD/location.conf
locationconfig.path = /var/lib/location

compat_locationconfig.files = $$PWD/location.conf
compat_locationconfig.path = /etc/location

QMAKE_PKGCONFIG_NAME = lib$$TARGET
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_DESCRIPTION = System settings application development files
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$develheaders.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

equals(QT_MAJOR_VERSION, 5): QMAKE_PKGCONFIG_REQUIRES = Qt$${QT_MAJOR_VERSION}Core Qt$${QT_MAJOR_VERSION}DBus profile connman-qt$${QT_MAJOR_VERSION} nemodbus
equals(QT_MAJOR_VERSION, 6): QMAKE_PKGCONFIG_REQUIRES = profile nemodbus

packagesExist(libsailfishkeyprovider) {
    QMAKE_PKGCONFIG_REQUIRES += libsailfishkeyprovider
}


INSTALLS += target develheaders pkgconfig locationconfig compat_locationconfig
