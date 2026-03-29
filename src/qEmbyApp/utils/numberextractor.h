#ifndef NUMBEREXTRACTOR_H
#define NUMBEREXTRACTOR_H

#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

class NumberExtractor
{
public:
    explicit NumberExtractor(bool extraSuffix = false);

    QStringList extractNumbersFromHtml(const QString &htmlContent) const;
    QStringList extractNumbers(const QString &text) const;
    QString extractBestNumber(const QString &text) const;
    QString extractBestNumber(const QStringList &texts) const;

private:
    struct CompactCandidate {
        QString number;
        int score = -1;
    };

    bool m_extraSuffix;
    QStringList m_uncensoredPrefixes;
    QSet<QString> m_uncensoredPrefixKeys;
    QSet<QString> m_blockedPrefixes;

    QRegularExpression m_cleanHtmlRe;
    QRegularExpression m_removeSpecialCharsRe;
    QRegularExpression m_normalizeSpaceRe;
    QRegularExpression m_fileExtensionRe;
    QRegularExpression m_fc2Re;
    QRegularExpression m_knownPrefixRe;
    QRegularExpression m_numericDateRe;
    QRegularExpression m_standardRe;
    QRegularExpression m_dottedRe;
    QRegularExpression m_compactTokenRe;
    QRegularExpression m_hasLetterRe;
    QRegularExpression m_hasDigitRe;
    QRegularExpression m_datePatternRe;
    QRegularExpression m_pureNumericCodeRe;

    void compilePatterns();
    QString preprocessText(const QString &text) const;
    QString normalizePrefix(const QString &prefix) const;
    QString canonicalizePrefix(const QString &prefix) const;
    QString normalizeFc2(const QString &raw) const;
    QString normalizeKnownPrefixMatch(const QRegularExpressionMatch &match) const;
    QString normalizeStandardMatch(const QRegularExpressionMatch &match) const;
    QString normalizeDottedMatch(const QRegularExpressionMatch &match) const;
    CompactCandidate extractCompactNumber(const QString &token) const;
    bool isBlockedPrefix(const QString &prefix) const;
    bool validateNumber(const QString &number) const;
    void appendMatch(QStringList &matches, QSet<QString> &seen,
                     const QString &number) const;
};

#endif 
