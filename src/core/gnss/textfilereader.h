#ifndef TEXTFILEREADER_H
#define TEXTFILEREADER_H

#include <QByteArray>
#include <QString>

class QFile;
class QTextStream;

// 统一的文本文件读取入口：先嗅探编码（UTF-8 / GBK/GB18030），再交给 QTextStream。
//
// 旧版 FileParser 直接 new QTextStream，遇到国产设备常见的 GBK 输出会整行乱码；
// TextCodecHelper 虽然写了却没人调用。这里把它接进解析链路。
class TextFileReader
{
public:
    // 打开文件并按探测到的编码配置 stream。file/stream 由调用方持有。
    // 成功时 encoding 返回实际使用的编码名。
    static bool open(const QString &path, QFile &file, QTextStream &stream,
                     QString *encoding = nullptr, QString *error = nullptr);

    // 只嗅探编码，不保持文件打开
    static QString detectEncoding(const QString &path);

private:
    static QString normalizeEncodingName(const QString &detected);
};

#endif // TEXTFILEREADER_H
