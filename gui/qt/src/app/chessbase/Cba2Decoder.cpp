#include "Cba2Decoder.h"

#include "app/MoveAnnotation.h"
#include "app/MoveComment.h"

#include <QCoreApplication>
#include <QStringDecoder>
#include <QtEndian>

#include <optional>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Cba2Decoder)
};

constexpr qint32 kEnd = 0x7FFFFFFF;

/// The types of annotation, as stored.
enum Type : quint16 {
    kTextAfter = 0x02,
    kSymbols = 0x03,
    kSquares = 0x04,
    kArrows = 0x05,
    kTimeSpent = 0x07,
    kUnknown08 = 0x08,
    kTraining = 0x09,
    kQuotation = 0x13,
    kPawnStructure = 0x14,
    kPiecePath = 0x15,
    kWhiteClock = 0x16,
    kBlackClock = 0x17,
    kCritical = 0x18,
    kWebLink = 0x1C,
    kEvaluation = 0x21,
    kMedals = 0x22,
    kVariationColour = 0x23,
    kTimeControl = 0x24,
    kVideoTime = 0x25,
    kEvaluations = 0x26,
    kTextBefore = 0x82,
};

constexpr int kAnyLanguage = 7;

/// Windows-1252's characters 0x80 to 0x9F; the rest is Latin-1. Qt has no
/// codec for it everywhere (only through ICU).
constexpr char16_t kWindows1252[32] = {
    u'€', u'\x81', u'‚', u'ƒ', u'„', u'…', u'†', u'‡', u'ˆ', u'‰', u'Š', u'‹', u'Œ', u'\x8D', u'Ž', u'\x8F',
    u'\x90', u'‘', u'’', u'“', u'”', u'•', u'–', u'—', u'˜', u'™', u'š', u'›', u'œ', u'\x9D', u'ž', u'Ÿ',
};

/// Reads a record's content, little-endian, never past its end.
struct Reader {
    const QByteArray &content;
    qsizetype at = 0;
    bool overrun = false;

    bool has(qsizetype count) const { return count >= 0 && at + count <= content.size(); }
    template <typename T>
    T number()
    {
        if (!has(sizeof(T))) {
            overrun = true;
            at = content.size();
            return T{};
        }
        const T value = qFromLittleEndian<T>(content.constData() + at);
        at += sizeof(T);
        return value;
    }
    QByteArray bytes(qsizetype count)
    {
        if (!has(count)) {
            overrun = true;
            at = content.size();
            return {};
        }
        const QByteArray value = content.mid(at, count);
        at += count;
        return value;
    }
    void skip(qsizetype count) { bytes(count); }
    /// Bytes preceded by their 32-bit length.
    QByteArray sized() { return bytes(number<qint32>()); }
};

/// A square as the annotations number it: from 1, file-major (a1 1, a2 2,
/// b1 9), as "e4"; empty for a number off the board.
QString squareName(int number)
{
    if (number < 1 || number > 64)
        return {};
    const int square = number - 1;
    return QString(QChar(u'a' + square / 8)) + QChar(u'1' + square % 8);
}

/// Lichess's letter for a ChessBase colour of squares and arrows (2 green,
/// 3 yellow, 4 red); the others, not known, are drawn blue.
QChar colourLetter(int colour)
{
    switch (colour) {
    case 2: return u'G';
    case 3: return u'Y';
    case 4: return u'R';
    default: return u'B';
    }
}

/// "[%csl Ge5,Rh7]" for the squares, "[%cal Gd2d4]" for the arrows.
QString marksCommand(const QByteArray &data, bool arrows)
{
    const int step = arrows ? 3 : 2;
    QStringList marks;
    for (qsizetype i = 0; i + step <= data.size(); i += step) {
        const QString from = squareName(quint8(data.at(i + 1)));
        const QString to = arrows ? squareName(quint8(data.at(i + 2))) : QString();
        if (from.isEmpty() || (arrows && to.isEmpty()))
            continue;
        marks << colourLetter(quint8(data.at(i))) + from + to;
    }
    if (marks.isEmpty())
        return {};
    return QStringLiteral("[%%1 %2]").arg(arrows ? QStringLiteral("cal") : QStringLiteral("csl"), marks.join(u','));
}

/// "[%eval 0.18]", "[%eval #-3]", with the depth when there is one:
/// "[%eval 0.18,20]". Empty for the kinds not known.
QString evaluationCommand(qint16 value, qint16 kind, qint16 depth)
{
    QString text;
    if (kind == 0)
        text = QString::number(value / 100.0, 'f', 2);
    else if (kind == 1)
        text = QStringLiteral("#%1").arg(value);
    else
        return {};
    if (depth > 0 && depth < 100)
        text += QStringLiteral(",%1").arg(depth);
    return QStringLiteral("[%eval %1]").arg(text);
}

/// The texts of one position, by language, as they come.
struct Texts {
    QList<std::pair<int, QString>> before;
    QList<std::pair<int, QString>> after;
};

/// The texts kept of those in several languages: those in any language, and
/// those of the language chosen.
QString chosenText(const QList<std::pair<int, QString>> &texts, const QString &language)
{
    std::optional<int> chosen;
    const auto has = [&texts](int code) {
        for (const auto &[language, text] : texts)
            if (language == code)
                return true;
        return false;
    };
    for (int code = 0; code < 32 && !chosen; ++code) {
        if (code != kAnyLanguage && Cba2Decoder::languageOf(code) == language && has(code))
            chosen = code;
    }
    if (!chosen && has(0))
        chosen = 0;
    for (const auto &[code, text] : texts) {
        if (!chosen && code != kAnyLanguage)
            chosen = code;
    }
    QString result;
    for (const auto &[code, text] : texts) {
        if (code == kAnyLanguage || code == chosen)
            result = MoveComment::joined(result, text);
    }
    return result;
}

/// Steps over a training question (its lists of texts, arrows and squares).
void skipTraining(Reader &reader)
{
    reader.skip(6 + 4 + 2);
    const auto list = [&reader] {
        const quint16 count = reader.number<quint16>();
        for (int i = 0; i < count && !reader.overrun; ++i) {
            reader.skip(2);
            reader.sized();
        }
    };
    for (int i = 0; i < 4; ++i)
        list();
    const quint8 solutions = reader.number<quint8>();
    for (int i = 0; i < solutions && !reader.overrun; ++i) {
        reader.skip(4);
        list();
    }
}

/// Steps over a game quoted in a comment: its header, names, ratings, moves.
void skipQuotation(Reader &reader)
{
    reader.skip(1 + 2 + 2 + 4 + 1);
    for (int i = 0; i < 6; ++i) // Players' names, site and event, each with its length in a byte.
        reader.skip(reader.number<quint8>());
    reader.skip(79);
    for (int i = 0; i < 2; ++i) { // The rating lists: 01 00 01 00 00, then a name.
        reader.skip(5);
        reader.sized();
    }
    reader.skip(29);
    const qint32 moves = reader.number<qint32>();
    reader.skip(qint64(qMax(0, moves)) * 5 + 4);
}

} // namespace

namespace Cba2Decoder {

QString languageOf(int code)
{
    switch (code) {
    case 0: return QStringLiteral("en");
    case 1: return QStringLiteral("de");
    case 2: return QStringLiteral("fr");
    case 3: return QStringLiteral("es");
    case 4: return QStringLiteral("it");
    case 5: return QStringLiteral("nl");
    case 6: return QStringLiteral("pt");
    case 12: return QStringLiteral("pl");
    case 18: return QStringLiteral("el");
    default: return {};
    }
}

QString text(const QByteArray &bytes)
{
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString result = utf8.decode(bytes);
    if (utf8.hasError()) {
        result.clear();
        for (const char byte : bytes) {
            const quint8 code = quint8(byte);
            result += code >= 0x80 && code < 0xA0 ? QChar(kWindows1252[code - 0x80]) : QChar(code);
        }
    }
    // Figurines, U+E024 to U+E029: king, queen, rook, bishop, knight, pawn.
    // Other characters of ChessBase's font have no letter: they go.
    static const QString pieces = QStringLiteral("KQRBNP");
    QString shown;
    shown.reserve(result.size());
    for (const QChar c : std::as_const(result)) {
        if (c.unicode() >= 0xE024 && c.unicode() <= 0xE029)
            shown += pieces.at(c.unicode() - 0xE024);
        else if (c.unicode() < 0xE000 || c.unicode() > 0xF8FF)
            shown += c;
    }
    shown.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    shown.replace(u'\r', u'\n');
    shown.remove(QLatin1String("[#]"));
    return shown.trimmed();
}

Decoded decode(const QByteArray &content, const QString &language)
{
    Decoded decoded;
    Reader reader{content};
    const auto fail = [&decoded](const QString &why) {
        decoded.error = why;
        return decoded;
    };
    while (true) {
        const qint32 position = reader.number<qint32>();
        if (reader.overrun)
            return fail(Text::tr("The annotations run past their record."));
        if (position == kEnd)
            return decoded;
        const qint32 count = reader.number<qint32>();
        Notes notes = decoded.positions.value(position);
        Texts texts;
        QStringList commands;
        for (qint32 i = 0; i < count && !reader.overrun; ++i) {
            const quint16 type = reader.number<quint16>();
            switch (type) {
            case kTextAfter:
            case kTextBefore: {
                reader.skip(2);
                const int code = reader.number<quint16>();
                const QString words = text(reader.sized());
                if (!words.isEmpty())
                    (type == kTextAfter ? texts.after : texts.before) << std::pair{code, words};
                break;
            }
            case kSymbols:
                for (const char nag : reader.bytes(3))
                    if (nag)
                        notes.nags << quint8(nag);
                break;
            case kSquares:
            case kArrows:
                if (const QString command = marksCommand(reader.sized(), type == kArrows); !command.isEmpty())
                    commands << command;
                break;
            case kTimeSpent: {
                const QByteArray time = reader.bytes(4); // Unknown, seconds, minutes, hours.
                if (time.size() == 4)
                    commands << QStringLiteral("[%emt %1:%2:%3]")
                                    .arg(quint8(time.at(3)))
                                    .arg(quint8(time.at(2)), 2, 10, QLatin1Char('0'))
                                    .arg(quint8(time.at(1)), 2, 10, QLatin1Char('0'));
                break;
            }
            case kEvaluation: {
                const qint16 value = reader.number<qint16>();
                const qint16 kind = reader.number<qint16>();
                const qint16 depth = reader.number<qint16>();
                if (const QString command = evaluationCommand(value, kind, depth); !command.isEmpty())
                    commands << command;
                break;
            }
            case kUnknown08:
            case kWhiteClock:
            case kBlackClock:
            case kMedals:
            case kVariationColour:
            case kVideoTime:
                reader.skip(4);
                break;
            case kPawnStructure:
            case kCritical:
                reader.skip(1);
                break;
            case kPiecePath:
                reader.sized();
                break;
            case kTimeControl:
                reader.skip(38);
                break;
            case kEvaluations: // The main line's, from an older analysis: the moves' own are read.
                reader.skip(1);
                reader.sized();
                break;
            case kWebLink:
                reader.skip(1);
                reader.sized(); // The address,
                reader.sized(); // then its caption.
                break;
            case kTraining:
                skipTraining(reader);
                break;
            case kQuotation:
                skipQuotation(reader);
                break;
            default:
                // No length to step over it: the rest of the record is lost.
                decoded.positions.insert(position, notes);
                return fail(Text::tr("Unknown annotation %1.").arg(type, 2, 16, QLatin1Char('0')));
            }
        }
        notes.before = MoveComment::joined(notes.before, chosenText(texts.before, language));
        notes.after = MoveComment::joined(MoveComment::joined(notes.after, chosenText(texts.after, language)),
                                          commands.join(u' '));
        notes.nags = MoveAnnotation::normalized(notes.nags);
        if (reader.overrun)
            return fail(Text::tr("The annotations run past their record."));
        decoded.positions.insert(position, notes);
    }
}

namespace {

void applyLine(QList<MoveRecord> &moves, QList<Variation> &variations, QString &startComment, const Decoded &notes,
               int &position)
{
    for (qsizetype i = 0; i < moves.size(); ++i) {
        if (const auto found = notes.positions.constFind(position); found != notes.positions.cend()) {
            QString &before = i == 0 ? startComment : moves[i - 1].comment;
            before = MoveComment::joined(before, found->before);
            moves[i].comment = MoveComment::joined(moves[i].comment, found->after);
            moves[i].nags = MoveAnnotation::normalized(moves[i].nags + found->nags);
        }
        ++position;
        for (Variation &variation : variations) {
            if (variation.atPly == i + 1)
                applyLine(variation.moves, variation.variations, variation.startComment, notes, position);
        }
    }
}

} // namespace

void apply(QList<MoveRecord> &moves, QList<Variation> &variations, QString &startComment, const Decoded &notes)
{
    if (const auto game = notes.positions.constFind(-1); game != notes.positions.cend())
        startComment = MoveComment::joined(MoveComment::joined(startComment, game->before), game->after);
    int position = 0;
    applyLine(moves, variations, startComment, notes, position);
}

} // namespace Cba2Decoder
