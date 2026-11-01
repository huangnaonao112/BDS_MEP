#include "textcodechelper.h"
#include <QDebug>

#if defined(QT6)
#include <QStringDecoder>
#include <optional>
#endif

QString TextCodecHelper::detectEncoding(const QByteArray &data)
{
    if (isUtf8(data)) {
        return "UTF-8";
    } else if (isGbk(data)) {
        return "GBK";
    } else if (isGb2312(data)) {
        return "GB2312";
    } else {
        return getSystemEncoding();
    }
}

QString TextCodecHelper::convertToUtf8(const QByteArray &data, const QString &encoding)
{
    QString detectedEncoding = encoding.isEmpty() ? detectEncoding(data) : encoding;

#if defined(QT6)
    // Qt6: 使用 QStringDecoder
    QStringDecoder::Encoding enc = QStringDecoder::Utf8;

    if (detectedEncoding.toUpper() == "UTF-8" || detectedEncoding.toUpper() == "UTF8") {
        enc = QStringDecoder::Utf8;
    } else if (detectedEncoding.toUpper() == "GBK" ||
               detectedEncoding.toUpper() == "GB2312" ||
               detectedEncoding.toUpper() == "GB18030") {
        // 使用 encodingForName 获取 GBK 编码
        auto gbEnc = QStringDecoder::encodingForName("GB18030");
        if (gbEnc.has_value()) {
            enc = gbEnc.value();
        } else {
            // 如果 GB18030 不支持，尝试 System
            enc = QStringDecoder::System;
        }
    } else if (detectedEncoding.toUpper() == "LATIN1" || detectedEncoding.toUpper() == "ISO-8859-1") {
        enc = QStringDecoder::Latin1;
    } else {
        enc = QStringDecoder::System;
    }

    QStringDecoder decoder(enc);
    if (!decoder.isValid()) {
        decoder = QStringDecoder(QStringDecoder::System);
    }

    return decoder.decode(data);
#else
    // Qt5: 使用 QTextCodec
    QTextCodec *codec = QTextCodec::codecForName(detectedEncoding.toUtf8());
    if (!codec) {
        codec = QTextCodec::codecForLocale();
    }
    return codec->toUnicode(data);
#endif
}

QString TextCodecHelper::getSystemEncoding()
{
#if defined(QT6)
    // Qt6: 返回系统默认编码名称
    return QString("System");
#else
    // Qt5: 使用 QTextCodec
    QTextCodec *codec = QTextCodec::codecForLocale();
    return QString::fromUtf8(codec->name());
#endif
}

void TextCodecHelper::setGlobalTextCodec(const QString &encoding)
{
#if defined(QT6)
    // Qt6: QTextCodec::setCodecForLocale 已移除
    // 在 Qt6 中，使用 QStringConverter 处理编码转换
    // 此函数保留为空实现以保持兼容性
    Q_UNUSED(encoding)
    qDebug() << "Qt6: 编码设置已通过 QStringConverter 自动处理";
#else
    // Qt5: 设置全局编码
    QTextCodec *codec = QTextCodec::codecForName(encoding.toUtf8());
    if (codec) {
        QTextCodec::setCodecForLocale(codec);
        qDebug() << "设置全局文本编码为:" << encoding;
    }
#endif
}

bool TextCodecHelper::isUtf8(const QByteArray &data)
{
    int nBytes = 0;
    const char *p = data.constData();

    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = p[i];

        if (nBytes == 0) {
            if ((c & 0x80) != 0) {
                if ((c & 0xE0) == 0xC0) {
                    nBytes = 1;
                } else if ((c & 0xF0) == 0xE0) {
                    nBytes = 2;
                } else if ((c & 0xF8) == 0xF0) {
                    nBytes = 3;
                } else {
                    return false;
                }
            }
        } else {
            if ((c & 0xC0) != 0x80) {
                return false;
            }
            nBytes--;
        }
    }

    return nBytes == 0;
}

bool TextCodecHelper::isGbk(const QByteArray &data)
{
    const char *p = data.constData();

    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = p[i];

        if (c > 0x80) {
            if (i + 1 < data.size()) {
                unsigned char c2 = p[i + 1];
                if (c2 >= 0x40 && c2 <= 0xFE) {
                    i++;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    return true;
}

bool TextCodecHelper::isGb2312(const QByteArray &data)
{
    const char *p = data.constData();

    for (int i = 0; i < data.size(); ++i) {
        unsigned char c = p[i];

        if (c > 0x80) {
            if (i + 1 < data.size()) {
                unsigned char c2 = p[i + 1];
                if (c2 >= 0xA1 && c2 <= 0xFE) {
                    i++;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    return true;
}
