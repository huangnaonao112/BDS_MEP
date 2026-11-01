QT += core gui widgets printsupport concurrent testlib

TEMPLATE = app
TARGET = tst_ui
CONFIG += c++17 console testcase
CONFIG -= app_bundle

# 端到端 UI 冒烟测试：真实 MainWindow + 真实数据 + 后台评估
INCLUDEPATH += $$PWD/../../src \
               $$PWD/../../src/core \
               $$PWD/../../src/core/gnss \
               $$PWD/../../src/core/evaluation \
               $$PWD/../../src/core/project \
               $$PWD/../../src/core/track \
               $$PWD/../../src/services \
               $$PWD/../../src/ui \
               $$PWD/../../src/widgets \
               $$PWD/../../src/widgets/plot

SOURCES += \
    tst_ui.cpp \
    ../../src/ui/mainwindow.cpp \
    ../../src/ui/newprojectdialog.cpp \
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
    ../../src/core/project/projectmanager.cpp \
    ../../src/core/track/trajectorydata.cpp \
    ../../src/core/track/fileparser.cpp \
    ../../src/services/evaluationservice.cpp \
    ../../src/services/importservice.cpp \
    ../../src/services/projectsession.cpp \
    ../../src/services/appcontroller.cpp \
    ../../src/core/gnss/textcodechelper.cpp \
    ../../src/core/gnss/textfilereader.cpp \
    ../../src/widgets/debugoutput.cpp \
    ../../src/widgets/busyindicator.cpp \
    ../../src/widgets/projecttreewidget.cpp \
    ../../src/widgets/filetableview.cpp \
    ../../src/widgets/reportpreview.cpp \
    ../../src/widgets/plot/camera2d.cpp \
    ../../src/widgets/plot/trackseries.cpp \
    ../../src/widgets/plot/trackindex.cpp \
    ../../src/widgets/plot/tracklod.cpp \
    ../../src/widgets/plot/trackrendermodel.cpp \
    ../../src/widgets/plot/plotrenderer.cpp \
    ../../src/widgets/plot/rtkplotwidget.cpp

HEADERS += \
    ../../src/ui/mainwindow.h \
    ../../src/ui/newprojectdialog.h \
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
    ../../src/core/evaluation/evaluationpipeline.h \
    ../../src/core/project/projectmanager.h \
    ../../src/core/track/trackmeta.h \
    ../../src/core/track/trajectorydata.h \
    ../../src/core/track/fileparser.h \
    ../../src/services/evaluationservice.h \
    ../../src/services/importservice.h \
    ../../src/services/projectsession.h \
    ../../src/services/appcontroller.h \
    ../../src/services/appstate.h \
    ../../src/core/gnss/textcodechelper.h \
    ../../src/core/gnss/textfilereader.h \
    ../../src/widgets/debugoutput.h \
    ../../src/widgets/busyindicator.h \
    ../../src/widgets/projecttreewidget.h \
    ../../src/widgets/filetableview.h \
    ../../src/widgets/reportpreview.h \
    ../../src/widgets/rtkplottypes.h \
    ../../src/widgets/plot/camera2d.h \
    ../../src/widgets/plot/trackseries.h \
    ../../src/widgets/plot/trackindex.h \
    ../../src/widgets/plot/tracklod.h \
    ../../src/widgets/plot/renderframe.h \
    ../../src/widgets/plot/plotstyle.h \
    ../../src/widgets/plot/trackrendermodel.h \
    ../../src/widgets/plot/plotrenderer.h \
    ../../src/widgets/plot/rtkplotwidget.h

FORMS += ../../src/ui/mainwindow.ui
RESOURCES += ../../resources.qrc

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
