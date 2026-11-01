QT += core
QT -= gui

TEMPLATE = app
TARGET = bdsmep-cli
CONFIG += c++17 console
CONFIG -= app_bundle

# 复用 core，不链接任何 UI 代码：可在服务器/CI 上跑批量评估
INCLUDEPATH += $$PWD/../../src \
               $$PWD/../../src/core \
               $$PWD/../../src/core/gnss \
               $$PWD/../../src/core/evaluation \
               $$PWD/../../src/core/project \
               $$PWD/../../src/core/track \
               $$PWD/../../src/utils

SOURCES += \
    main.cpp \
    ../../src/core/gnss/gnssdefs.cpp \
    ../../src/core/gnss/coordinates.cpp \
    ../../src/core/gnss/timeutil.cpp \
    ../../src/core/gnss/epochdata.cpp \
    ../../src/core/gnss/ggaparser.cpp \
    ../../src/core/gnss/posparser.cpp \
    ../../src/core/gnss/tabularparser.cpp \
    ../../src/core/gnss/parsercommon.cpp \
    ../../src/core/evaluation/evaluationresult.cpp \
    ../../src/core/evaluation/errorstats.cpp \
    ../../src/core/evaluation/evaluationengine.cpp \
    ../../src/core/evaluation/reportbuilder.cpp \
    ../../src/core/evaluation/comparisonwriter.cpp \
    ../../src/core/evaluation/evaluationpipeline.cpp \
    ../../src/utils/textcodechelper.cpp \
    ../../src/utils/textfilereader.cpp

HEADERS += \
    ../../src/core/gnss/gnssdefs.h \
    ../../src/core/gnss/coordinates.h \
    ../../src/core/gnss/timeutil.h \
    ../../src/core/gnss/epochdata.h \
    ../../src/core/gnss/gnssparser.h \
    ../../src/core/evaluation/evaluationresult.h \
    ../../src/core/evaluation/errorstats.h \
    ../../src/core/evaluation/evaluationengine.h \
    ../../src/core/evaluation/reportbuilder.h \
    ../../src/core/evaluation/comparisonwriter.h \
    ../../src/core/evaluation/evaluationpipeline.h

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
