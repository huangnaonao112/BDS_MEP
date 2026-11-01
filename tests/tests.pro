QT += core testlib

TEMPLATE = app
TARGET = tst_bdsmep
CONFIG += c++17 console testcase
CONFIG -= app_bundle

# 测试只链接纯 core（无 widgets），可以在 CI/无显示环境跑
INCLUDEPATH += $$PWD/../src \
               $$PWD/../src/core \
               $$PWD/../src/core/gnss \
               $$PWD/../src/core/evaluation \
               $$PWD/../src/core/project \
               $$PWD/../src/core/track

SOURCES += \
    tst_bdsmep.cpp \
    ../src/core/gnss/gnssdefs.cpp \
    ../src/core/gnss/coordinates.cpp \
    ../src/core/gnss/timeutil.cpp \
    ../src/core/gnss/epochdata.cpp \
    ../src/core/gnss/ggaparser.cpp \
    ../src/core/gnss/posparser.cpp \
    ../src/core/gnss/tabularparser.cpp \
    ../src/core/gnss/parsercommon.cpp \
    ../src/core/evaluation/evaluationresult.cpp \
    ../src/core/evaluation/errorstats.cpp \
    ../src/core/evaluation/evaluationengine.cpp \
    ../src/core/project/projectmanager.cpp \
    ../src/core/track/trajectorydata.cpp \
    ../src/core/track/fileparser.cpp \
    ../src/core/gnss/textcodechelper.cpp \
    ../src/core/gnss/textfilereader.cpp

HEADERS += \
    ../src/core/gnss/gnssdefs.h \
    ../src/core/gnss/coordinates.h \
    ../src/core/gnss/timeutil.h \
    ../src/core/gnss/epochdata.h \
    ../src/core/gnss/gnssparser.h \
    ../src/core/evaluation/evaluationresult.h \
    ../src/core/evaluation/errorstats.h \
    ../src/core/evaluation/evaluationengine.h \
    ../src/core/project/projectmanager.h \
    ../src/core/track/trackmeta.h \
    ../src/core/track/trajectorydata.h \
    ../src/core/track/fileparser.h

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
