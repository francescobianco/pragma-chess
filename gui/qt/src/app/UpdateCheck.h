#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

/// Whether a newer Pragma Chess is out. Each release carries a small
/// `version.json` ({"version": "0.4.0", "url": "…/releases/tag/v0.4.0"}),
/// read at releases/latest/download/version.json: the check reads only that,
/// sends nothing about the user, and the file's download count tells the
/// project roughly how many copies are in use. Pure, unit-tested.
namespace UpdateCheck {

/// Where the latest release's version.json is.
QString latestUrl();

struct Release {
    QString version;
    QString url;
};

/// The release a version.json describes; none for anything else.
std::optional<Release> parse(const QByteArray &json);

/// Whether `candidate` ("0.4.0", "v0.4.0") is newer than `current`: by
/// numbers, and a pre-release ("0.4.0-beta.1") older than its release.
bool isNewer(const QString &candidate, const QString &current);

} // namespace UpdateCheck
