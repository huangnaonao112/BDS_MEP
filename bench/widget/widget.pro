QT += core gui widgets

TEMPLATE = app
TARGET = bench_widget
CONFIG += c++17 console
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../../src/widgets \
               $$PWD/../../src/widgets/plot

SOURCES += \
    bench_widget.cpp \
    ../../src/widgets/plot/rtkplotwidget.cpp \
    ../../src/widgets/plot/camera2d.cpp \
    ../../src/widgets/plot/trackseries.cpp \
    ../../src/widgets/plot/trackindex.cpp \
    ../../src/widgets/plot/tracklod.cpp \
    ../../src/widgets/plot/trackrendermodel.cpp \
    ../../src/widgets/plot/plotrenderer.cpp

HEADERS += \
    ../../src/widgets/plot/rtkplotwidget.h \
    ../../src/widgets/rtkplottypes.h \
    ../../src/widgets/plot/camera2d.h \
    ../../src/widgets/plot/trackseries.h \
    ../../src/widgets/plot/trackindex.h \
    ../../src/widgets/plot/tracklod.h \
    ../../src/widgets/plot/renderframe.h \
    ../../src/widgets/plot/plotstyle.h \
    ../../src/widgets/plot/trackrendermodel.h \
    ../../src/widgets/plot/plotrenderer.h

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
