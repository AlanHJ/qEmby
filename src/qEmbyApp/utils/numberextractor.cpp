#include "numberextractor.h"

#include <QDebug>
#include <QTextDocumentFragment>

#include <algorithm>
#include <functional>

NumberExtractor::NumberExtractor(bool extraSuffix)
    : m_extraSuffix(extraSuffix)
{
    m_uncensoredPrefixes = {
        "BT-",          "CT-",          "EMP-",         "CCDV-",
        "CWP-",         "CWPBD-",       "DSAM-",        "DRC-",
        "DRG-",         "GACHI-",       "HEYDOUGA",     "JAV-",
        "LAF-",         "LAFBD-",       "HEYZO-",       "KTG-",
        "KP-",          "KG-",          "LLDV-",        "MCDV-",
        "MKD-",         "MKBD-",        "MMDV-",        "NIP-",
        "PB-",          "PT-",          "QE-",          "RED-",
        "RHJ-",         "S2M-",         "SKY-",         "SKYHD-",
        "SMD-",         "SSDV-",        "SSKP-",        "TRG-",
        "TS-",          "XXX-AV-",      "YKB-",         "BIRD",
        "BOUGA",        "1PONDO",       "10MUSUME",     "PACOPACOMAMA",
        "CARIB",        "CARIBBEANCOM", "TOKYOHOT"
    };

    m_blockedPrefixes = {
        "WEB",     "WEBDL",   "WEBDLRIP", "WEBRIP",  "BLURAY",
        "BD",      "BDRIP",   "BRRIP",    "DVDRIP",  "HDRIP",
        "REMUX",   "HEVC",    "AVC",      "AAC",     "DTS",
        "TRUEHD",  "ATMOS",   "FLAC",     "MP3",     "WAV",
        "UHD",     "FHD",     "HD",       "HDR",     "SDR",
        "XVID",    "DIVX",    "H264",     "H265",    "X264",
        "X265",    "HEVC10",  "AV1",      "VP9",     "COMPLETE",
        "PROPER",  "REPACK",  "UNCUT",    "UNRATED", "SUB",
        "CHS",     "CHT",     "GB",       "BIG5",    "JPN",
        "JAP",     "ENG",     "MULTI",    "DUBBED",  "DUAL"
    };

    for (const QString &prefix : m_uncensoredPrefixes) {
        m_uncensoredPrefixKeys.insert(canonicalizePrefix(prefix));
    }

    compilePatterns();
}

void NumberExtractor::compilePatterns()
{
    QStringList prefixPatterns;
    prefixPatterns.reserve(m_uncensoredPrefixes.size());

    QStringList sortedPrefixes = m_uncensoredPrefixes;
    std::sort(sortedPrefixes.begin(), sortedPrefixes.end(),
              [](const QString &lhs, const QString &rhs) {
                  return lhs.length() > rhs.length();
              });

    for (QString prefix : sortedPrefixes) {
        prefix = prefix.trimmed();
        while (prefix.endsWith('-')) {
            prefix.chop(1);
        }
        prefix = QRegularExpression::escape(prefix);
        prefix.replace("\\-", "[-_\\s]?");
        prefixPatterns.append(prefix);
    }

    const QString knownPrefixAlternation = prefixPatterns.join('|');

    m_cleanHtmlRe.setPattern(R"(<[^>]+>|&nbsp;|\u3000)");
    m_removeSpecialCharsRe.setPattern(
        R"([^A-Z0-9\-\._\s\u4E00-\u9FFF\u3040-\u30FF])");
    m_removeSpecialCharsRe.setPatternOptions(
        QRegularExpression::CaseInsensitiveOption);
    m_normalizeSpaceRe.setPattern(R"(\s{2,})");

    m_fileExtensionRe.setPattern(
        R"(\.(?:GIF|JPG|JPEG|PNG|BMP|WEBP|MP4|AVI|MOV|WMV|MKV|FLV|TS|M2TS|SRT|ASS|SSA|ISO)$)");
    m_fileExtensionRe.setPatternOptions(
        QRegularExpression::CaseInsensitiveOption);

    m_fc2Re.setPattern(
        R"((?<![A-Z0-9])(FC2(?:[-_\s]?PPV)?[-_\s]?\d{5,7})(?![A-Z0-9]))");

    m_knownPrefixRe.setPattern(QString(
        R"((?<![A-Z0-9])((?:%1))[-_\s]?(\d{2,6})(?:[-_](\d{2,4}))?([A-Z]{0,3})(?![A-Z0-9]))")
                                   .arg(knownPrefixAlternation));
    m_knownPrefixRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    m_numericDateRe.setPattern(R"((?<!\d)(\d{6})[-_](\d{2,4})(?!\d))");

    m_standardRe.setPattern(
        R"((?<![A-Z0-9])([A-Z][A-Z0-9]{1,9})[-_\s](\d{2,6})([A-Z]{0,3})(?![A-Z0-9]))");
    m_standardRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    m_dottedRe.setPattern(
        R"((?<![A-Z0-9])([A-Z][A-Z0-9]{1,12})[\._-](\d{2})[\._-](\d{2})[\._-](\d{2})(?![A-Z0-9]))");
    m_dottedRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    m_compactTokenRe.setPattern(R"((?<![A-Z0-9])([A-Z][A-Z0-9]{4,15})(?![A-Z0-9]))");
    m_compactTokenRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    m_hasLetterRe.setPattern(R"([A-Z])");
    m_hasDigitRe.setPattern(R"(\d)");
    m_datePatternRe.setPattern(R"(^\d{4}[\.\-]\d{2}[\.\-]\d{2}$)");
    m_pureNumericCodeRe.setPattern(R"(^\d{6}-\d{2,4}$)");
}

QString NumberExtractor::preprocessText(const QString &text) const
{
    QString processed = QTextDocumentFragment::fromHtml(text).toPlainText();
    processed.replace(m_cleanHtmlRe, " ");
    processed = processed.toUpper();
    processed.replace(m_removeSpecialCharsRe, " ");
    processed.replace(m_normalizeSpaceRe, " ");
    processed = processed.trimmed();

    if (m_extraSuffix) {
        QStringList tokens =
            processed.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
        for (QString &token : tokens) {
            token.remove(m_fileExtensionRe);
            while (!token.isEmpty() &&
                   QStringLiteral("-_.").contains(token.back())) {
                token.chop(1);
            }
        }
        processed = tokens.join(' ');
    }

    return processed;
}

QString NumberExtractor::normalizePrefix(const QString &prefix) const
{
    QString normalized = prefix.trimmed().toUpper();
    normalized.replace(QRegularExpression(R"([_\s]+)"), "-");
    normalized.replace(QRegularExpression(R"(-{2,})"), "-");
    return normalized;
}

QString NumberExtractor::canonicalizePrefix(const QString &prefix) const
{
    QString canonical = normalizePrefix(prefix);
    canonical.remove('-');
    canonical.remove('.');
    return canonical;
}

QString NumberExtractor::normalizeFc2(const QString &raw) const
{
    const QString compact = raw.toUpper().remove(QRegularExpression(R"([_\s-]+)"));
    const int digitsIndex = compact.lastIndexOf(QRegularExpression(R"(\d{5,7}$)"));
    if (digitsIndex < 0) {
        return {};
    }

    const QString digits = compact.mid(digitsIndex);
    const bool hasPpv = compact.contains("PPV");
    return hasPpv ? QString("FC2-PPV-%1").arg(digits)
                  : QString("FC2-%1").arg(digits);
}

QString NumberExtractor::normalizeKnownPrefixMatch(
    const QRegularExpressionMatch &match) const
{
    const QString prefix = normalizePrefix(match.captured(1));
    const QString digits = match.captured(2);
    const QString extraDigits = match.captured(3);
    const QString suffix = match.captured(4).toUpper();

    if (prefix.isEmpty() || digits.isEmpty()) {
        return {};
    }

    QString normalized = QString("%1-%2").arg(prefix, digits);
    if (!extraDigits.isEmpty()) {
        normalized += QString("-%1").arg(extraDigits);
    }
    normalized += suffix;
    return normalized;
}

QString NumberExtractor::normalizeStandardMatch(
    const QRegularExpressionMatch &match) const
{
    const QString prefix = normalizePrefix(match.captured(1));
    const QString digits = match.captured(2);
    const QString suffix = match.captured(3).toUpper();

    if (prefix.isEmpty() || digits.isEmpty()) {
        return {};
    }

    return QString("%1-%2%3").arg(prefix, digits, suffix);
}

QString NumberExtractor::normalizeDottedMatch(
    const QRegularExpressionMatch &match) const
{
    const QString prefix = normalizePrefix(match.captured(1));
    if (prefix.isEmpty()) {
        return {};
    }

    return QString("%1-%2-%3-%4")
        .arg(prefix, match.captured(2), match.captured(3), match.captured(4));
}

NumberExtractor::CompactCandidate
NumberExtractor::extractCompactNumber(const QString &token) const
{
    if (token.contains('-') || token.contains('_') || token.contains('.')) {
        return {};
    }

    const QString upperToken = token.toUpper();
    if (upperToken.startsWith("FC2")) {
        return {};
    }

    CompactCandidate best;
    const int maxPrefixLength = qMin(10, upperToken.length() - 2);

    for (int prefixLength = 2; prefixLength <= maxPrefixLength; ++prefixLength) {
        for (int suffixLength = 0; suffixLength <= 3; ++suffixLength) {
            const int digitLength =
                upperToken.length() - prefixLength - suffixLength;
            if (digitLength < 2 || digitLength > 6) {
                continue;
            }

            const QString prefix = upperToken.left(prefixLength);
            const QString digits = upperToken.mid(prefixLength, digitLength);
            const QString suffix =
                suffixLength > 0 ? upperToken.right(suffixLength) : QString();

            int letterCount = 0;
            int digitCountInPrefix = 0;
            for (const QChar ch : prefix) {
                if (ch.isLetter()) {
                    ++letterCount;
                } else if (ch.isDigit()) {
                    ++digitCountInPrefix;
                }
            }

            if (letterCount < 2 || digitCountInPrefix > 3) {
                continue;
            }
            if (!m_hasDigitRe.match(digits).hasMatch() ||
                digits.contains(QRegularExpression(R"(\D)"))) {
                continue;
            }
            if (!suffix.isEmpty() &&
                suffix.contains(QRegularExpression(R"([^A-Z])"))) {
                continue;
            }
            if (isBlockedPrefix(prefix)) {
                continue;
            }

            int score = 0;
            score += (prefix.back().isLetter() ? 20 : 12);
            score += (digitLength >= 3 && digitLength <= 5) ? 18 : 8;
            score += qMax(0, 12 - qAbs(prefixLength - 4) * 3);
            score += letterCount * 2;
            if (digitCountInPrefix == 0) {
                score += 8;
            }
            if (suffixLength == 0) {
                score += 4;
            }

            const QString normalized =
                QString("%1-%2%3").arg(prefix, digits, suffix);
            if (score > best.score && validateNumber(normalized)) {
                best.number = normalized;
                best.score = score;
            }
        }
    }

    return best;
}

bool NumberExtractor::isBlockedPrefix(const QString &prefix) const
{
    const QString canonical = canonicalizePrefix(prefix);
    if (canonical.isEmpty()) {
        return true;
    }

    if (m_uncensoredPrefixKeys.contains(canonical)) {
        return false;
    }

    if (m_blockedPrefixes.contains(canonical)) {
        return true;
    }

    static const QStringList blockedStarts = {
        "WEB",   "BD",    "BR",    "DVD",   "HDR",   "UHD",
        "FHD",   "HEVC",  "AAC",   "DTS",   "TRUEHD","FLAC",
        "AVC",   "X264",  "X265",  "H264",  "H265",  "REMUX"
    };

    for (const QString &blocked : blockedStarts) {
        if (canonical.startsWith(blocked) &&
            !m_uncensoredPrefixKeys.contains(canonical)) {
            return true;
        }
    }

    return false;
}

bool NumberExtractor::validateNumber(const QString &number) const
{
    const QString normalized = number.trimmed().toUpper();
    if (normalized.length() < 5 || normalized.length() > 24) {
        return false;
    }

    if (m_datePatternRe.match(normalized).hasMatch()) {
        return false;
    }

    if (m_pureNumericCodeRe.match(normalized).hasMatch()) {
        return true;
    }

    if (!m_hasLetterRe.match(normalized).hasMatch() ||
        !m_hasDigitRe.match(normalized).hasMatch()) {
        return false;
    }

    const int dashIndex = normalized.indexOf('-');
    const QString prefix =
        dashIndex > 0 ? normalized.left(dashIndex) : normalized;
    if (isBlockedPrefix(prefix)) {
        return false;
    }

    return true;
}

void NumberExtractor::appendMatch(QStringList &matches, QSet<QString> &seen,
                                  const QString &number) const
{
    const QString normalized = number.trimmed().toUpper();
    if (normalized.isEmpty() || !validateNumber(normalized) ||
        seen.contains(normalized)) {
        return;
    }

    matches.append(normalized);
    seen.insert(normalized);
}

QStringList NumberExtractor::extractNumbersFromHtml(const QString &htmlContent) const
{
    return extractNumbers(htmlContent);
}

QStringList NumberExtractor::extractNumbers(const QString &text) const
{
    const QString processed = preprocessText(text);
    QStringList matches;
    QSet<QString> seen;

    auto collectByRegex =
        [this, &processed, &matches, &seen](
            const QRegularExpression &regex,
            const std::function<QString(const QRegularExpressionMatch &)> &builder) {
            QRegularExpressionMatchIterator iterator = regex.globalMatch(processed);
            while (iterator.hasNext()) {
                const QRegularExpressionMatch match = iterator.next();
                appendMatch(matches, seen, builder(match));
            }
        };

    collectByRegex(m_fc2Re, [this](const QRegularExpressionMatch &match) {
        return normalizeFc2(match.captured(1));
    });
    collectByRegex(m_knownPrefixRe,
                   [this](const QRegularExpressionMatch &match) {
                       return normalizeKnownPrefixMatch(match);
                   });
    collectByRegex(m_numericDateRe, [](const QRegularExpressionMatch &match) {
        return QString("%1-%2").arg(match.captured(1), match.captured(2));
    });
    collectByRegex(m_standardRe, [this](const QRegularExpressionMatch &match) {
        return normalizeStandardMatch(match);
    });
    collectByRegex(m_dottedRe, [this](const QRegularExpressionMatch &match) {
        return normalizeDottedMatch(match);
    });

    QRegularExpressionMatchIterator compactIterator =
        m_compactTokenRe.globalMatch(processed);
    while (compactIterator.hasNext()) {
        const QString token = compactIterator.next().captured(1);
        const CompactCandidate compactCandidate = extractCompactNumber(token);
        if (!compactCandidate.number.isEmpty()) {
            appendMatch(matches, seen, compactCandidate.number);
        }
    }

    if (!matches.isEmpty()) {
        qDebug() << "[NumberExtractor] extracted" << matches
                 << "from text length" << processed.length();
    }

    return matches;
}

QString NumberExtractor::extractBestNumber(const QString &text) const
{
    const QStringList numbers = extractNumbers(text);
    return numbers.isEmpty() ? QString() : numbers.first();
}

QString NumberExtractor::extractBestNumber(const QStringList &texts) const
{
    for (int index = 0; index < texts.size(); ++index) {
        const QStringList numbers = extractNumbers(texts.at(index));
        if (!numbers.isEmpty()) {
            qDebug() << "[NumberExtractor] best number selected from source"
                     << index << ":" << numbers.first();
            return numbers.first();
        }
    }

    return {};
}
