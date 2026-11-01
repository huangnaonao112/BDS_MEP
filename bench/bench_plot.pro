QT += gui
QT -= widgets

TEMPLATE = app
TARGET = bench_plot
CONFIG += c++17 console
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../src \
               $$PWD/../src/core \
               $$PWD/../src/core/gnss \
               $$PWD/../src/core/track \
               $$PWD/../src/widgets \
               $$PWD/../src/widgets/plot

SOURCES += \
    bench_plot.cpp \
    ../src/widgets/plot/camera2d.cpp \
    ../src/widgets/plot/trackseries.cpp \
    ../src/widgets/plot/trackindex.cpp \
    ../src/widgets/plot/tracklod.cpp \
    ../src/widgets/plot/trackrendermodel.cpp \
    ../src/widgets/plot/plotrenderer.cpp \
    ../src/core/gnss/gnssdefs.cpp \
    ../src/core/gnss/coordinates.cpp \
    ../src/core/gnss/timeutil.cpp \
    ../src/core/gnss/epochdata.cpp \
    ../src/core/gnss/ggaparser.cpp \
    ../src/core/gnss/posparser.cpp \
    ../src/core/gnss/tabularparser.cpp \
    ../src/core/gnss/parsercommon.cpp \
    ../src/core/track/trajectorydata.cpp \
    ../src/core/track/fileparser.cpp \
    ../src/core/gnss/textcodechelper.cpp \
    ../src/core/gnss/textfilereader.cpp

HEADERS += \
    ../src/widgets/rtkplottypes.h \
    ../src/widgets/plot/camera2d.h \
    ../src/widgets/plot/trackseries.h \
    ../src/widgets/plot/trackindex.h \
    ../src/widgets/plot/tracklod.h \
    ../src/widgets/plot/renderframe.h \
    ../src/widgets/plot/plotstyle.h \
    ../src/widgets/plot/trackrendermodel.h \
    ../src/widgets/plot/plotrenderer.h \
    ../src/core/gnss/gnssparser.h \
    ../src/core/track/trajectorydata.h \
    ../src/core/track/fileparser.h \
    ../src/core/track/trackmeta.h

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
