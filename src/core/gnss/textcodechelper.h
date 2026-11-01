#ifndef TEXTCODECHELPER_H
#define TEXTCODECHELPER_H

#include <QString>
#include <QByteArray>

#if defined(QT6)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

class TextCodecHelper
{
public:
    static QString detectEncoding(const QByteArray &data);
    static QString convertToUtf8(const QByteArray &data, const QString &encoding = "");
    static QString getSystemEncoding();
    static void setGlobalTextCodec(const QString &encoding);

private:
    static bool isUtf8(const QByteArray &data);
    static bool isGbk(const QByteArray &data);
    static bool isGb2312(const QByteArray &data);
};

#endif // TEXTCODECHELPER_H
