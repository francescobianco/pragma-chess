// pragma-phone: Phone Link on the command line (docs/phone-link.md).
//
// As the phone, with the link shown by Options ▸ Connect Mobile App…:
//
//     pragma-phone --pair '<link>' --list
//     pragma-phone --pair '<link>' --get "Classic Games.pdb" -o ~/Downloads
//     pragma-phone --pair '<link>' --put "Le mie partite.pdb" games.json
//     pragma-phone --pair '<link>' --put "Le mie partite.pdb" --db <uuid> games.json
//
// As the computer, serving a folder of databases (for testing without the
// desktop client; it prints the pairing link):
//
//     pragma-phone --serve --databases /tmp/pragma/Databases --state /tmp/pragma/phone-link.json
//
// The phone's key is kept in --key (by default in the user's app data).

#include "app/phone/DatabaseFolderStore.h"
#include "app/phone/NostrKey.h"
#include "app/phone/PairingLink.h"
#include "app/phone/PhoneLink.h"
#include "app/phone/PhoneLinkClient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>

#include <rtc/global.hpp>

#include <csignal>

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

void quitOnSignal(int)
{
    QCoreApplication::quit();
}

std::optional<NostrKey> loadOrCreateKey(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
        return NostrKey::fromSecret(QByteArray::fromHex(file.readAll().trimmed()));
    const NostrKey key = NostrKey::generate();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile save(path);
    if (!save.open(QIODevice::WriteOnly))
        return std::nullopt;
    save.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    save.write(key.secret().toHex() + '\n');
    save.commit();
    return key;
}

int serve(const QCommandLineParser &parser)
{
    const QString databases = parser.value(QStringLiteral("databases"));
    const QString state = parser.value(QStringLiteral("state"));
    if (databases.isEmpty() || state.isEmpty()) {
        err() << "--serve needs --databases and --state\n";
        return 2;
    }
    auto *link = new PhoneLink(state, databases, qApp);
    auto *store = new DatabaseFolderStore;
    link->setGameStore(store);
    if (!parser.values(QStringLiteral("relay")).isEmpty())
        link->setRelays(parser.values(QStringLiteral("relay")));
    if (parser.isSet(QStringLiteral("name")))
        link->setComputerName(parser.value(QStringLiteral("name")));

    const auto printLink = [link] { out() << "link: " << link->pairingLink().toString() << Qt::endl; };
    link->beginPairing();
    printLink();
    QObject::connect(link, &PhoneLink::pairingLinkChanged, link, printLink);
    QObject::connect(link, &PhoneLink::devicePaired, link,
                     [](const QString &name) { out() << "paired: " << name << Qt::endl; });
    QObject::connect(link, &PhoneLink::gamesStored, link, [](const QString &path, int count) {
        out() << "stored: " << count << " game(s) in " << path << Qt::endl;
    });
    QObject::connect(link, &PhoneLink::statusChanged, link, [link] {
        out() << "status: relays " << link->connectedRelays() << ", sessions " << link->activeSessions()
              << (link->activity().isEmpty() ? QString() : QStringLiteral(", ") + link->activity()) << Qt::endl;
    });
    const int code = QCoreApplication::exec();
    delete link;
    delete store;
    return code;
}

int actAsPhone(const QCommandLineParser &parser)
{
    const std::optional<PairingLink> pairing = PairingLink::parse(parser.value(QStringLiteral("pair")));
    if (!pairing) {
        err() << "not a pairing link\n";
        return 2;
    }
    const QString keyPath = parser.isSet(QStringLiteral("key"))
                                ? parser.value(QStringLiteral("key"))
                                : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                      + QStringLiteral("/pragma-phone.key");
    const std::optional<NostrKey> key = loadOrCreateKey(keyPath);
    if (!key) {
        err() << "cannot read or write the key " << keyPath << "\n";
        return 2;
    }

    QJsonObject request;
    QString outputDir;
    if (parser.isSet(QStringLiteral("list"))) {
        request = {{QStringLiteral("op"), QStringLiteral("list")}};
    } else if (parser.isSet(QStringLiteral("get"))) {
        request = {{QStringLiteral("op"), QStringLiteral("get")},
                   {QStringLiteral("name"), parser.value(QStringLiteral("get"))}};
        outputDir = parser.value(QStringLiteral("output"));
    } else if (parser.isSet(QStringLiteral("put"))) {
        const QStringList arguments = parser.positionalArguments();
        QFile file(arguments.value(0));
        if (arguments.size() != 1 || !file.open(QIODevice::ReadOnly)) {
            err() << "--put NAME needs one JSON file of games\n";
            return 2;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonArray games =
            document.isArray() ? document.array() : document.object().value(QStringLiteral("games")).toArray();
        request = {{QStringLiteral("op"), QStringLiteral("put")},
                   {QStringLiteral("name"), parser.value(QStringLiteral("put"))},
                   {QStringLiteral("games"), games}};
        if (parser.isSet(QStringLiteral("db")))
            request.insert(QStringLiteral("db"), parser.value(QStringLiteral("db")));
        if (document.isObject() && document.object().contains(QStringLiteral("properties")))
            request.insert(QStringLiteral("properties"), document.object().value(QStringLiteral("properties")));
    } else {
        err() << "say what to do: --list, --get NAME or --put NAME FILE\n";
        return 2;
    }

    const QString phoneName = parser.isSet(QStringLiteral("name")) ? parser.value(QStringLiteral("name"))
                                                                  : QStringLiteral("pragma-phone on %1")
                                                                        .arg(QSysInfo::machineHostName());
    PhoneLinkClient client(*key, pairing->computerKey, pairing->relays, PhoneLink::defaultIceServers(), phoneName);
    client.setPairingSecret(pairing->secret);

    int code = 1;
    QByteArray received;
    QJsonObject header;
    QObject::connect(&client, &PhoneLinkClient::failed, &client, [&code](const QString &reason) {
        err() << "error: " << reason << Qt::endl;
        code = 1;
        QCoreApplication::quit();
    });
    QObject::connect(&client, &PhoneLinkClient::connected, &client, [&client, request](const QString &computer) {
        err() << "connected to " << computer << Qt::endl;
        client.sendJson(request);
    });
    QObject::connect(&client, &PhoneLinkClient::binaryReceived, &client,
                     [&received](const QByteArray &data) { received += data; });
    QObject::connect(&client, &PhoneLinkClient::messageReceived, &client, [&](const QJsonObject &message) {
        const QString op = message.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("list")) {
            for (const QJsonValue &value : message.value(QStringLiteral("files")).toArray()) {
                const QJsonObject file = value.toObject();
                out() << file.value(QStringLiteral("sha256")).toString() << "  "
                      << file.value(QStringLiteral("id")).toString() << "  "
                      << file.value(QStringLiteral("games")).toInteger() << " games  "
                      << file.value(QStringLiteral("size")).toInteger() << "  "
                      << file.value(QStringLiteral("modified")).toString() << "  "
                      << file.value(QStringLiteral("name")).toString() << Qt::endl;
            }
            code = 0;
        } else if (op == QLatin1String("file")) {
            header = message;
            received.clear();
            return;
        } else if (op == QLatin1String("done")) {
            const QString sha = QString::fromLatin1(QCryptographicHash::hash(received, QCryptographicHash::Sha256).toHex());
            if (received.size() != header.value(QStringLiteral("size")).toInteger()
                || sha != header.value(QStringLiteral("sha256")).toString()) {
                err() << "error: the file arrived damaged\n";
                code = 1;
            } else {
                const QString name = QFileInfo(header.value(QStringLiteral("name")).toString()).fileName();
                const QString path = QDir(outputDir.isEmpty() ? QDir::currentPath() : outputDir).filePath(name);
                QSaveFile file(path);
                if (file.open(QIODevice::WriteOnly) && file.write(received) == received.size() && file.commit()) {
                    out() << "saved " << path << " (" << received.size() << " bytes, sha256 " << sha << ")"
                          << Qt::endl;
                    code = 0;
                } else {
                    err() << "error: cannot write " << path << Qt::endl;
                }
            }
        } else if (op == QLatin1String("put")) {
            out() << message.value(QStringLiteral("name")).toString() << ": stored "
                  << message.value(QStringLiteral("stored")).toInt() << ", updated "
                  << message.value(QStringLiteral("updated")).toInt() << ", known "
                  << message.value(QStringLiteral("known")).toInt() << ", conflicts "
                  << message.value(QStringLiteral("conflicts")).toArray().size() << Qt::endl;
            code = 0;
        } else if (op == QLatin1String("error")) {
            err() << "error: " << message.value(QStringLiteral("message")).toString() << Qt::endl;
            code = 1;
        }
        QCoreApplication::quit();
    });
    client.start();
    QCoreApplication::exec();
    return code;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Pragma"));
    QCoreApplication::setApplicationName(QStringLiteral("pragma-phone"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    rtc::InitLogger(rtc::LogLevel::None);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Phone Link from the command line: the phone's side, or the "
                                                    "computer's with --serve."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {QStringLiteral("pair"), QStringLiteral("The computer's pairing link."), QStringLiteral("link")},
        {QStringLiteral("key"), QStringLiteral("File with the phone's secret key (created if missing)."),
         QStringLiteral("file")},
        {QStringLiteral("list"), QStringLiteral("List the computer's databases.")},
        {QStringLiteral("get"), QStringLiteral("Download a database."), QStringLiteral("name")},
        {{QStringLiteral("o"), QStringLiteral("output")}, QStringLiteral("Folder for --get."), QStringLiteral("dir")},
        {QStringLiteral("put"), QStringLiteral("Send the games of a JSON file to a database."), QStringLiteral("name")},
        {QStringLiteral("db"), QStringLiteral("Universal id of the database for --put (a new one is made with it)."),
         QStringLiteral("id")},
        {QStringLiteral("name"), QStringLiteral("This phone's (or, with --serve, computer's) name."),
         QStringLiteral("name")},
        {QStringLiteral("serve"), QStringLiteral("Be the computer: serve --databases.")},
        {QStringLiteral("databases"), QStringLiteral("Folder of databases for --serve."), QStringLiteral("dir")},
        {QStringLiteral("state"), QStringLiteral("Key and paired phones for --serve."), QStringLiteral("file")},
        {QStringLiteral("relay"), QStringLiteral("Relay for --serve (repeatable)."), QStringLiteral("url")},
        {QStringLiteral("timeout"), QStringLiteral("Give up after this many seconds (default 90)."),
         QStringLiteral("seconds")},
    });
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("JSON games for --put."));
    parser.process(app);

    std::signal(SIGINT, quitOnSignal);
    std::signal(SIGTERM, quitOnSignal);
    if (parser.isSet(QStringLiteral("serve"))) {
        if (parser.isSet(QStringLiteral("timeout")))
            QTimer::singleShot(parser.value(QStringLiteral("timeout")).toInt() * 1000, &app, &QCoreApplication::quit);
        return serve(parser);
    }
    if (!parser.isSet(QStringLiteral("pair"))) {
        parser.showHelp(2);
    }
    const int timeout = parser.isSet(QStringLiteral("timeout")) ? parser.value(QStringLiteral("timeout")).toInt() : 90;
    QTimer::singleShot(timeout * 1000, &app, [] {
        err() << "error: timed out\n";
        QCoreApplication::exit(1);
    });
    return actAsPhone(parser);
}
