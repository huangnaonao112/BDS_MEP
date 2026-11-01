#include "comparisonwriter.h"

#include "reportbuilder.h"

#include <QDir>

#if defined(QT6)
#include <QStringConverter>
#endif

namespace {

const char *kHeader =
    "% GPSTSec             BaseE        BaseN        BaseU  Q"
    "              EvalE        EvalN        EvalU  Q"
    "            dx            dy            dz     plane    height\n";

void configureUtf8(QTextStream &stream)
{
#if defined(QT6)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
}

} // namespace

ComparisonWriter::ComparisonWriter()
{
    m_fixFix.setDevice(&m_fixFixFile);
    m_fixFloat.setDevice(&m_fixFloatFile);
    m_fixSingle.setDevice(&m_fixSingleFile);
}

ComparisonWriter::~ComparisonWriter()
{
    if (m_fixFixFile.isOpen()) m_fixFixFile.close();
    if (m_fixFloatFile.isOpen()) m_fixFloatFile.close();
    if (m_fixSingleFile.isOpen()) m_fixSingleFile.close();
}

void ComparisonWriter::writeHeader(QTextStream &stream) const
{
    stream << QString::fromUtf8(kHeader);
}

bool ComparisonWriter::open(const QString &outputDirectory, QString *error)
{
    QDir dir(outputDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("无法创建输出目录: %1").arg(outputDirectory);
        return false;
    }

    struct Target {
        QFile *file;
        QTextStream *stream;
        const char *name;
    };
    const Target targets[] = {
        {&m_fixFixFile, &m_fixFix, "Fix_Fix.txt"},
        {&m_fixFloatFile, &m_fixFloat, "Fix_Float.txt"},
        {&m_fixSingleFile, &m_fixSingle, "Fix_Single.txt"},
    };

    for (const Target &target : targets) {
        const QString path = dir.filePath(QString::fromLatin1(target.name));
        // 每次评估覆盖旧结果，避免残留上一次的数据
        QFile::remove(path);

        target.file->setFileName(path);
        if (!target.file->open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (error) {
                *error = QStringLiteral("无法写入结果文件: %1 (%2)")
                             .arg(path, target.file->errorString());
            }
            return false;
        }
        configureUtf8(*target.stream);
        writeHeader(*target.stream);
        m_writtenFiles << path;
    }

    return true;
}

void ComparisonWriter::writeSample(const ev::ComparisonSample &sample)
{
    QTextStream *stream = nullptr;
    if (sample.baselineQuality == 1.0) {
        if (sample.evalQuality == 1.0) {
            stream = &m_fixFix;
            ++m_fixFixCount;
        } else if (sample.evalQuality == 2.0) {
            stream = &m_fixFloat;
            ++m_fixFloatCount;
        } else {
            stream = &m_fixSingle;
            ++m_fixSingleCount;
        }
    }

    if (!stream) {
        return;
    }

    *stream << QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14\n")
                   .arg(sample.time, 14, 'f', 3)
                   .arg(sample.baselineE, 13, 'f', 3)
                   .arg(sample.baselineN, 13, 'f', 3)
                   .arg(sample.baselineU, 13, 'f', 3)
                   .arg(static_cast<int>(sample.baselineQuality), 3)
                   .arg(sample.evalE, 13, 'f', 3)
                   .arg(sample.evalN, 13, 'f', 3)
                   .arg(sample.evalU, 13, 'f', 3)
                   .arg(static_cast<int>(sample.evalQuality), 3)
                   .arg(sample.dx, 13, 'f', 4)
                   .arg(sample.dy, 13, 'f', 4)
                   .arg(sample.dz, 13, 'f', 4)
                   .arg(sample.planeError, 9, 'f', 4)
                   .arg(sample.heightError, 9, 'f', 4);
}

void ComparisonWriter::finalize(const ev::EvaluationResult &result)
{
    m_fixFix << QStringLiteral("\n% 统计信息（分母=本文件样本数）\n");
    m_fixFix << QStringLiteral("% 固定-固定样本: %1\n").arg(m_fixFixCount);
    m_fixFix << QStringLiteral("% %1\n").arg(ReportBuilder::metricsLine(result.fixFix));

    m_fixFloat << QStringLiteral("\n% 统计信息（分母=本文件样本数）\n");
    m_fixFloat << QStringLiteral("% 固定-浮点样本: %1\n").arg(m_fixFloatCount);
    m_fixFloat << QStringLiteral("% %1\n").arg(ReportBuilder::metricsLine(result.fixFloat));

    m_fixSingle << QStringLiteral("\n% 统计信息（分母=本文件样本数）\n");
    m_fixSingle << QStringLiteral("% 固定-其它解状态样本: %1\n").arg(m_fixSingleCount);
    m_fixSingle << QStringLiteral("% 这些样本未参与固定解精度统计，仅作排查用\n");

    m_fixFix.flush();
    m_fixFloat.flush();
    m_fixSingle.flush();

    m_fixFixFile.close();
    m_fixFloatFile.close();
    m_fixSingleFile.close();
}
