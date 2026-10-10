#pragma once

#include "Extension.h"

#include <QObject>

class QNetworkAccessManager;

/// Where Help ▸ Manage Extensions finds extensions: a provider with its
/// catalog. En Croissant's registry is one; others can follow (a publisher
/// whose engines and databases could be installed the same way).
class ExtensionProvider : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString homepage() const = 0;
    /// Reads the catalog; ready() or failed() follows.
    virtual void fetch() = 0;
    const QList<Extension> &extensions() const { return m_extensions; }

Q_SIGNALS:
    void ready();
    void failed(const QString &message);

protected:
    QList<Extension> m_extensions;
};

/// En Croissant's registry (EnCroissantCatalog): its engines for this system
/// and processor, its databases and puzzles listed.
class EnCroissantProvider : public ExtensionProvider {
    Q_OBJECT

public:
    explicit EnCroissantProvider(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("encroissant"); }
    QString name() const override { return QStringLiteral("En Croissant"); }
    QString description() const override;
    QString homepage() const override { return QStringLiteral("https://encroissant.org"); }
    void fetch() override;

private:
    QNetworkAccessManager *m_network;
    int m_pending = 0;
    QList<Extension> m_engines;
    QList<Extension> m_databases;
    QList<Extension> m_puzzles;
    QString m_error;
};
