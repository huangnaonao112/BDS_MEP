#ifndef COMPARISONWRITER_H
#define COMPARISONWRITER_H

#include <QFile>
#include <QStringList>
#include <QTextStream>

#include "evaluationresult.h"

// 比较明细输出：Fix_Fix.txt / Fix_Float.txt / Fix_Single.txt
//
// 逐条写出，不在内存里囤几十万行；三个文件共用同一份样本流。
class ComparisonWriter
{
public:
    ComparisonWriter();
    ~ComparisonWriter();

    bool open(const QString &outputDirectory, QString *error = nullptr);
    void writeSample(const ev::ComparisonSample &sample);
    void finalize(const ev::EvaluationResult &result);

    QStringList writtenFiles() const { return m_writtenFiles; }

private:
    void writeHeader(QTextStream &stream) const;

    QFile m_fixFixFile;
    QFile m_fixFloatFile;
    QFile m_fixSingleFile;

    QTextStream m_fixFix;
    QTextStream m_fixFloat;
    QTextStream m_fixSingle;

    int m_fixFixCount = 0;
    int m_fixFloatCount = 0;
    int m_fixSingleCount = 0;

    QStringList m_writtenFiles;
};

#endif // COMPARISONWRITER_H
