#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "mainwindow.h"

// 注意：这里刻意不设置全局字体、不打开 Qt 的高 DPI 属性。
// 原版就是用 QApplication 的默认字体和默认缩放，改了会让整个界面字形和
// 清晰度跟以前不一样（高分屏下尤其明显）。
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GNSS 数据评估系统"));
    parser.addHelpOption();
    const QCommandLineOption openOption(
        {QStringLiteral("o"), QStringLiteral("open")},
        QStringLiteral("启动时打开指定项目目录"), QStringLiteral("dir"));
    parser.addOption(openOption);
    parser.process(app);

    MainWindow window;
    if (parser.isSet(openOption)) {
        window.openProjectPath(parser.value(openOption));
    }
    window.show();

    return app.exec();
}
