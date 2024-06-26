QT = core sql network
QT -= gui
CONFIG += c++11 cmdline

LIBS += -lgepcalcmodule -lgepclientmodule -lgdal -lqgis_core -lproj
INCLUDEPATH += /usr/include/gepcalcmodule /usr/include/gepclientmodule /usr/include/gdal /usr/include/qgis/

SOURCES += \
        imageStitcher.cpp \
        main.cpp

RESOURCES +=

TARGET = imageStitcher

UI_DIR = uics
MOC_DIR = mocs
OBJECTS_DIR = objs
RCC_DIR = resources
DESTDIR = bin

pathApp = /var/www/cgi
target.path = $${pathApp}

INSTALLS += target

HEADERS += \
    imageStitcher.hpp

