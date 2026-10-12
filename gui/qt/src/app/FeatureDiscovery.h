#pragma once

#include "ChessPosition.h"
#include "EngineEvaluation.h"
#include "PolyglotBook.h"

#include <QList>
#include <QString>

#include <optional>

/// Points a feature out to someone trying the program, on a sign that it
/// would help now — never at random, which would nag. For Explain the sign
/// is a move that looks like a trial, the moves of someone moving pieces to
/// see what the program does: one the engine judges an error, as the tutor
/// would, or one off the moves the book offers, out of the openings it
/// knows. A feature is presented at most once per run — closed and opened
/// again, the program may present it again — and "Don't show this again"
/// ends it for good: the client keeps that and gives it back here.
/// Pure: the window tells it what happened and keeps what it decides.
class FeatureDiscovery {
public:
    enum class Feature { Explain };

    /// Every feature there is to discover.
    static const QList<Feature> &all();
    /// The feature's name in settings and in the development API ("explain").
    static QString name(Feature feature);
    static std::optional<Feature> fromName(const QString &name);

    /// `dismissed`: the features never to present again.
    explicit FeatureDiscovery(const QList<Feature> &dismissed = {});

    bool isDismissed(Feature feature) const { return m_dismissed.contains(feature); }
    /// "Don't show this again".
    void dismiss(Feature feature);
    void undismiss(Feature feature);
    /// Presented in this run.
    bool wasPresented(Feature feature) const { return m_presented.contains(feature); }
    void markPresented(Feature feature);
    void clearPresented(Feature feature);
    /// Whether to present the feature now: not dismissed, not presented in this run.
    bool shouldPresent(Feature feature) const { return !isDismissed(feature) && !wasPresented(feature); }

    /// A move off the moves the book offers for the position it was played
    /// in: the game leaves the openings the book knows. False where the book
    /// offers nothing for that position: there was nothing to leave.
    static bool leavesBook(const QList<PolyglotBook::Move> &bookMoves, const ChessMove &played);
    /// A move the engine judges an error, as the tutor does: `before` the
    /// evaluation of `start`, the position it was played in, `after` the one
    /// of the position it made. Both deep enough (kMinDepthBefore,
    /// kMinDepthAfter), or there is no judgement.
    static bool looksWrong(const EngineEvaluation &before, const EngineEvaluation &after, const ChessPosition &start,
                           const ChessMove &played);
    /// The depths a judgement needs: of the position before (the tutor's) and of the one after.
    static constexpr int kMinDepthBefore = 8;
    static constexpr int kMinDepthAfter = 12;

private:
    QList<Feature> m_dismissed;
    QList<Feature> m_presented;
};
