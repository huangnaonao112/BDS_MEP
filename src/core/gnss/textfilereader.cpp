#include "textfilereader.h"
#include "textcodechelper.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#if !defined(QT6)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif

QString TextFileReader::normalizeEncodingName(const QString &detected)
{
    const QString upper = detected.trimmed().toUpper();
    if (upper.contains(QStringLiteral("UTF"))) {
        return QStringLiteral("UTF-8");
    }
    if (upper.contains(QStringLiteral("GB")) || upper.contains(QStringLiteral("GBK"))
        || upper.contains(QStringLiteral("GB2312")) || upper.contains(QStringLiteral("GB18030"))) {
        return QStringLiteral("GB18030");  // GBK/GB2312 的超集
    }
    if (upper.contains(QStringLiteral("LATIN")) || upper.contains(QStringLiteral("ISO-8859"))) {
        return QStringLiteral("ISO-8859-1");
    }
    return QStringLiteral("UTF-8");
}

QString TextFileReader::detectEncoding(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("UTF-8");
    }
    const QByteArray probe = file.read(64 * 1024);
    file.close();
    return normalizeEncodingName(TextCodecHelper::detectEncoding(probe));
}

bool TextFileReader::open(const QString &path, QFile &file, QTextStream &stream,
                          QString *encoding, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (error) *error = QStringLiteral("文件不存在: %1").arg(path);
        return false;
    }

    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开文件: %1 (%2)").arg(path, file.errorString());
        return false;
    }

    const QByteArray probe = file.read(64 * 1024);
    file.seek(0);
    const QString enc = normalizeEncodingName(TextCodecHelper::detectEncoding(probe));

    // 关键：QTextStream 默认没有设备，必须显式绑定，否则 readLine() 直接返回空
    stream.setDevice(&file);

#if defined(QT6)
    // Qt6 下由 QTextStream::setEncoding 处理；GB18030 需要 ICU 支持，
    // 不支持时退回系统编码（QStringConverter::System）。
    const auto target = QStringConverter::encodingForName(enc.toUtf8());
    if (target.has_value()) {
        stream.setEncoding(target.value());
    }
#else
    QTextCodec *codec = QTextCodec::codecForName(enc.toUtf8());
    if (!codec) {
        codec = QTextCodec::codecForLocale();
    }
    stream.setCodec(codec);
#endif

    if (encoding) *encoding = enc;
    return true;
}
