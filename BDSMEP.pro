#-------------------------------------------------
# GNSS 数据评估系统 (BDS_MEP)
#
# 目录分层：
#   src/version.h      版本等全局定义（单文件，不放目录）
#   src/core           gnss(解析/坐标/时间/编码) / evaluation(统计/报告) / project / track
#   src/services       控制器、会话与后台服务（无 UI 依赖）
#   src/ui             窗口、对话框
#   src/widgets        可复用控件；plot/ 为绘图管线
#   src/widgets/debugoutput  日志面板（控件，不是工具类）
#
# 构建产物请放到独立构建目录（见 scripts/build.ps1），不要污染源码目录。
#-------------------------------------------------

QT += core gui widgets printsupport concurrent

greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += QT6
    DEFINES += QT_ENABLE_HIGHDPI_SCALING
}

CONFIG += c++17
CONFIG -= console
CONFIG -= app_bundle
CONFIG += no_batch
QMAKE_PROJECT_DEPTH = 0
CONFIG += absolute_library_paths

TARGET = BDSMEP
TEMPLATE = app

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_CFLAGS += /utf-8
}

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

win32:LIBS += -lkernel32 -luser32

INCLUDEPATH += $$PWD \
               $$PWD/src \
               $$PWD/src/core \
               $$PWD/src/core/gnss \
               $$PWD/src/core/evaluation \
               $$PWD/src/core/project \
               $$PWD/src/core/track \
               $$PWD/src/services \
               $$PWD/src/ui \
               $$PWD/src/widgets \
               $$PWD/src/widgets/plot

SOURCES += \
    main.cpp \
    src/ui/mainwindow.cpp \
    src/ui/newprojectdialog.cpp \
    src/core/gnss/gnssdefs.cpp \
    src/core/gnss/coordinates.cpp \
    src/core/gnss/timeutil.cpp \
    src/core/gnss/epochdata.cpp \
    src/core/gnss/ggaparser.cpp \
    src/core/gnss/posparser.cpp \
    src/core/gnss/tabularparser.cpp \
    src/core/gnss/parsercommon.cpp \
    src/core/evaluation/evaluationresult.cpp \
    src/core/evaluation/errorstats.cpp \
    src/core/evaluation/evaluationengine.cpp \
    src/core/evaluation/reportbuilder.cpp \
    src/core/evaluation/comparisonwriter.cpp \
    src/core/evaluation/evaluationpipeline.cpp \
    src/core/project/projectmanager.cpp \
    src/core/track/trajectorydata.cpp \
    src/core/track/fileparser.cpp \
    src/services/evaluationservice.cpp \
    src/services/importservice.cpp \
    src/services/projectsession.cpp \
    src/services/appcontroller.cpp \
    src/core/gnss/textcodechelper.cpp \
    src/core/gnss/textfilereader.cpp \
    src/widgets/debugoutput.cpp \
    src/widgets/busyindicator.cpp \
    src/widgets/projecttreewidget.cpp \
    src/widgets/filetableview.cpp \
    src/widgets/reportpreview.cpp \
    src/widgets/plot/camera2d.cpp \
    src/widgets/plot/trackseries.cpp \
    src/widgets/plot/trackindex.cpp \
    src/widgets/plot/tracklod.cpp \
    src/widgets/plot/trackrendermodel.cpp \
    src/widgets/plot/plotrenderer.cpp \
    src/widgets/plot/rtkplotwidget.cpp

HEADERS += \
    src/version.h \
    src/ui/mainwindow.h \
    src/ui/newprojectdialog.h \
    src/core/gnss/gnssdefs.h \
    src/core/gnss/coordinates.h \
    src/core/gnss/timeutil.h \
    src/core/gnss/epochdata.h \
    src/core/gnss/gnssparser.h \
    src/core/evaluation/evaluationresult.h \
    src/core/evaluation/errorstats.h \
    src/core/evaluation/evaluationengine.h \
    src/core/evaluation/reportbuilder.h \
    src/core/evaluation/comparisonwriter.h \
    src/core/evaluation/evaluationpipeline.h \
    src/core/project/projectmanager.h \
    src/core/track/trackmeta.h \
    src/core/track/trajectorydata.h \
    src/core/track/fileparser.h \
    src/services/evaluationservice.h \
    src/services/importservice.h \
    src/services/projectsession.h \
    src/services/appcontroller.h \
    src/services/appstate.h \
    src/core/gnss/textcodechelper.h \
    src/core/gnss/textfilereader.h \
    src/widgets/debugoutput.h \
    src/widgets/busyindicator.h \
    src/widgets/projecttreewidget.h \
    src/widgets/filetableview.h \
    src/widgets/reportpreview.h \
    src/widgets/rtkplottypes.h \
    src/widgets/plot/camera2d.h \
    src/widgets/plot/trackseries.h \
    src/widgets/plot/trackindex.h \
    src/widgets/plot/tracklod.h \
    src/widgets/plot/renderframe.h \
    src/widgets/plot/plotstyle.h \
    src/widgets/plot/trackrendermodel.h \
    src/widgets/plot/plotrenderer.h \
    src/widgets/plot/rtkplotwidget.h

FORMS += \
    src/ui/mainwindow.ui

RESOURCES += \
    resources.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 部署（拷贝 Qt DLL）交给 scripts/deploy.ps1，不挂在每次 make 的 post-link 上：
#   * 构建快，源码目录干净
#   * 旧写法 $$TARGET.exe 被 qmake 当成名为 "TARGET.exe" 的变量，实际展开为空，
#     windeployqt 收到的是目录而不是 exe —— 一直在静默失败。
