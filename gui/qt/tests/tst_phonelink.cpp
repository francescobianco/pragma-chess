#include "app/SqliteGameDatabase.h"
#include "app/phone/DatabaseFolderStore.h"
#include "app/phone/Nip44.h"
#include "app/phone/NostrEvent.h"
#include "app/phone/NostrKey.h"
#include "app/phone/PairingLink.h"
#include "app/phone/PhoneFiles.h"
#include "app/phone/PhoneGames.h"
#include "app/phone/PhoneLink.h"
#include "app/phone/PhoneLinkClient.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <rtc/rtc.hpp>
#include <rtc/websocketserver.hpp>

#include <mutex>

namespace {

QByteArray hex(const QJsonValue &value)
{
    return QByteArray::fromHex(value.toString().toLatin1());
}

QJsonObject vectors()
{
    QFile file(QStringLiteral(PHONE_TEST_DATA "/nip44-vectors.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject section(const char *validity, const char *name)
{
    return {{QStringLiteral("x"), vectors().value(QLatin1String(validity)).toObject().value(QLatin1String(name))}};
}

QJsonArray cases(const char *validity, const char *name)
{
    return section(validity, name).value(QStringLiteral("x")).toArray();
}

/// A minimal Nostr relay on localhost (REQ with kinds and #p, EVENT, CLOSE),
/// so Phone Link can be tested end to end without the network.
class LocalRelay {
public:
    LocalRelay()
    {
        rtc::WebSocketServerConfiguration config;
        config.port = 0;
        config.bindAddress = "127.0.0.1";
        m_server = std::make_unique<rtc::WebSocketServer>(config);
        m_server->onClient([this](std::shared_ptr<rtc::WebSocket> socket) {
            std::lock_guard lock(m_mutex);
            auto client = std::make_shared<Client>();
            client->socket = socket;
            m_clients.push_back(client);
            socket->onMessage([this, weak = std::weak_ptr<Client>(client)](rtc::message_variant data) {
                if (const auto *text = std::get_if<std::string>(&data))
                    if (const auto locked = weak.lock())
                        onMessage(*locked, QByteArray::fromStdString(*text));
            });
        });
    }

    ~LocalRelay()
    {
        std::lock_guard lock(m_mutex);
        for (const auto &client : m_clients) {
            client->socket->resetCallbacks();
            client->socket->close();
        }
        m_server->stop();
    }

    QString url() const { return QStringLiteral("ws://127.0.0.1:%1").arg(m_server->port()); }

private:
    struct Client {
        std::shared_ptr<rtc::WebSocket> socket;
        QHash<QString, QJsonObject> subscriptions;
    };

    static bool matches(const QJsonObject &filter, const QJsonObject &event)
    {
        if (!filter.value(QStringLiteral("kinds")).toArray().contains(event.value(QStringLiteral("kind"))))
            return false;
        const QJsonArray wanted = filter.value(QStringLiteral("#p")).toArray();
        for (const QJsonValue &tag : event.value(QStringLiteral("tags")).toArray()) {
            const QJsonArray values = tag.toArray();
            if (values.size() >= 2 && values.at(0).toString() == QLatin1String("p") && wanted.contains(values.at(1)))
                return true;
        }
        return false;
    }

    void onMessage(Client &from, const QByteArray &text)
    {
        const QJsonArray message = QJsonDocument::fromJson(text).array();
        const QString type = message.at(0).toString();
        std::lock_guard lock(m_mutex);
        if (type == QLatin1String("REQ")) {
            from.subscriptions.insert(message.at(1).toString(), message.at(2).toObject());
            send(from, {QStringLiteral("EOSE"), message.at(1)});
        } else if (type == QLatin1String("CLOSE")) {
            from.subscriptions.remove(message.at(1).toString());
        } else if (type == QLatin1String("EVENT")) {
            const QJsonObject event = message.at(1).toObject();
            send(from, {QStringLiteral("OK"), event.value(QStringLiteral("id")), true, QString()});
            for (const auto &client : m_clients) {
                for (auto it = client->subscriptions.cbegin(); it != client->subscriptions.cend(); ++it) {
                    if (matches(it.value(), event))
                        send(*client, {QStringLiteral("EVENT"), it.key(), event});
                }
            }
        }
    }

    static void send(Client &client, const QJsonArray &message)
    {
        if (client.socket->isOpen())
            client.socket->send(QJsonDocument(message).toJson(QJsonDocument::Compact).toStdString());
    }

    std::mutex m_mutex;
    std::unique_ptr<rtc::WebSocketServer> m_server;
    std::vector<std::shared_ptr<Client>> m_clients;
};

GameRecord scholarsMate()
{
    GameRecord game;
    game.white = QStringLiteral("White");
    game.black = QStringLiteral("Black");
    game.result = QStringLiteral("1-0");
    const QStringList uci{QStringLiteral("e2e4"), QStringLiteral("e7e5"), QStringLiteral("f1c4"),
                          QStringLiteral("b8c6"), QStringLiteral("d1h5"), QStringLiteral("g8f6"),
                          QStringLiteral("h5f7")};
    const QStringList san{QStringLiteral("e4"), QStringLiteral("e5"), QStringLiteral("Bc4"), QStringLiteral("Nc6"),
                          QStringLiteral("Qh5"), QStringLiteral("Nf6"), QStringLiteral("Qxf7#")};
    for (qsizetype i = 0; i < uci.size(); ++i)
        game.moves.append({san.at(i), uci.at(i)});
    game.plyCount = int(game.moves.size());
    return game;
}

QJsonObject phoneGame(const QString &id)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("white"), QStringLiteral("Me")},
            {QStringLiteral("black"), QStringLiteral("Friend")},
            {QStringLiteral("date"), QStringLiteral("2026.09.29")},
            {QStringLiteral("result"), QStringLiteral("0-1")},
            // SAN deliberately sloppy: the computer writes it again from UCI.
            {QStringLiteral("moves_san"), QStringLiteral("f3 e5 g4 Qh4")},
            {QStringLiteral("moves_uci"), QStringLiteral("f2f3 e7e5 g2g4 d8h4")}};
}

} // namespace

class PhoneLinkTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void nip44ConversationKeys();
    void nip44InvalidConversationKeys();
    void nip44MessageKeys();
    void nip44PaddedLength();
    void nip44EncryptDecrypt();
    void nip44RejectsInvalidPayloads();
    void verifiesBip340Signatures();
    void serializesAndSignsEvents();
    void parsesPairingLinks();
    void resolvesOnlyDatabasesInsideTheFolder();
    void parsesPhoneGames();
    void storesPhoneGamesOnce();
    void pairsListsGetsAndPutsEndToEnd();
};

void PhoneLinkTest::nip44ConversationKeys()
{
    const QJsonArray all = cases("valid", "get_conversation_key");
    QVERIFY(!all.isEmpty());
    for (const QJsonValue &value : all) {
        const QJsonObject vector = value.toObject();
        const std::optional<NostrKey> key = NostrKey::fromSecret(hex(vector.value(QStringLiteral("sec1"))));
        QVERIFY(key);
        const std::optional<QByteArray> shared = key->sharedX(hex(vector.value(QStringLiteral("pub2"))));
        QVERIFY(shared);
        QCOMPARE(Nip44::conversationKey(*shared).toHex(), vector.value(QStringLiteral("conversation_key")).toString());
    }
}

void PhoneLinkTest::nip44InvalidConversationKeys()
{
    const QJsonArray all = cases("invalid", "get_conversation_key");
    QVERIFY(!all.isEmpty());
    for (const QJsonValue &value : all) {
        const QJsonObject vector = value.toObject();
        const std::optional<NostrKey> key = NostrKey::fromSecret(hex(vector.value(QStringLiteral("sec1"))));
        const bool valid = key && key->sharedX(hex(vector.value(QStringLiteral("pub2"))));
        QVERIFY2(!valid, qPrintable(vector.value(QStringLiteral("note")).toString()));
    }
}

void PhoneLinkTest::nip44MessageKeys()
{
    const QJsonObject vectors = section("valid", "get_message_keys").value(QStringLiteral("x")).toObject();
    const QByteArray conversation = hex(vectors.value(QStringLiteral("conversation_key")));
    const QJsonArray keys = vectors.value(QStringLiteral("keys")).toArray();
    QVERIFY(!keys.isEmpty());
    for (const QJsonValue &value : keys) {
        const QJsonObject vector = value.toObject();
        const Nip44::MessageKeys derived = Nip44::messageKeys(conversation, hex(vector.value(QStringLiteral("nonce"))));
        QCOMPARE(derived.chachaKey.toHex(), vector.value(QStringLiteral("chacha_key")).toString());
        QCOMPARE(derived.chachaNonce.toHex(), vector.value(QStringLiteral("chacha_nonce")).toString());
        QCOMPARE(derived.hmacKey.toHex(), vector.value(QStringLiteral("hmac_key")).toString());
    }
}

void PhoneLinkTest::nip44PaddedLength()
{
    const QJsonArray all = cases("valid", "calc_padded_len");
    QVERIFY(!all.isEmpty());
    for (const QJsonValue &value : all) {
        const QJsonArray pair = value.toArray();
        QCOMPARE(Nip44::paddedLength(pair.at(0).toInt()), pair.at(1).toInt());
    }
}

void PhoneLinkTest::nip44EncryptDecrypt()
{
    const QJsonArray all = cases("valid", "encrypt_decrypt");
    QVERIFY(!all.isEmpty());
    for (const QJsonValue &value : all) {
        const QJsonObject vector = value.toObject();
        const std::optional<NostrKey> first = NostrKey::fromSecret(hex(vector.value(QStringLiteral("sec1"))));
        const std::optional<NostrKey> second = NostrKey::fromSecret(hex(vector.value(QStringLiteral("sec2"))));
        QVERIFY(first && second);
        const std::optional<QByteArray> shared = first->sharedX(second->publicKey());
        QVERIFY(shared);
        const QByteArray conversation = Nip44::conversationKey(*shared);
        QCOMPARE(conversation.toHex(), vector.value(QStringLiteral("conversation_key")).toString());
        // Both sides agree on the key.
        QCOMPARE(Nip44::conversationKey(*second->sharedX(first->publicKey())), conversation);

        const QByteArray plaintext = vector.value(QStringLiteral("plaintext")).toString().toUtf8();
        const QString payload = vector.value(QStringLiteral("payload")).toString();
        QCOMPARE(Nip44::encrypt(plaintext, conversation, hex(vector.value(QStringLiteral("nonce")))), payload);
        QCOMPARE(Nip44::decrypt(payload, conversation), plaintext);
        // A random nonce round-trips too.
        QCOMPARE(Nip44::decrypt(*Nip44::encrypt(plaintext, conversation), conversation), plaintext);
    }
    QVERIFY(!Nip44::encrypt(QByteArray(), QByteArray(32, 'k')));
    QVERIFY(!Nip44::encrypt(QByteArray(65536, 'a'), QByteArray(32, 'k')));
}

void PhoneLinkTest::nip44RejectsInvalidPayloads()
{
    const QJsonArray all = cases("invalid", "decrypt");
    QVERIFY(!all.isEmpty());
    for (const QJsonValue &value : all) {
        const QJsonObject vector = value.toObject();
        QVERIFY2(!Nip44::decrypt(vector.value(QStringLiteral("payload")).toString(),
                                 hex(vector.value(QStringLiteral("conversation_key")))),
                 qPrintable(vector.value(QStringLiteral("note")).toString()));
    }
}

void PhoneLinkTest::verifiesBip340Signatures()
{
    // BIP-340 test vector 0 (secret key 3).
    const QByteArray publicKey =
        QByteArray::fromHex("F9308A019258C31049344F85F89D5229B531C845836F99B08601F113BCE036F9");
    const QByteArray message(32, '\0');
    QByteArray signature = QByteArray::fromHex(
        "E907831F80848D1069A5371B402410364BDF1C5F8307B0084C55F1CE2DCA8215"
        "25F66A4A85EA8B71E482A74F382D2CE5EBEEE8FDB2172F477DF4900D310536C0");
    QVERIFY(NostrKey::verify(publicKey, message, signature));
    signature[10] = char(signature.at(10) ^ 1);
    QVERIFY(!NostrKey::verify(publicKey, message, signature));

    const std::optional<NostrKey> three = NostrKey::fromSecret(QByteArray(31, '\0') + QByteArray(1, '\3'));
    QVERIFY(three);
    QCOMPARE(three->publicKey(), publicKey);
    QVERIFY(NostrKey::verify(publicKey, message, three->sign(message)));
    QVERIFY(!NostrKey::fromSecret(QByteArray(32, '\0')));
}

void PhoneLinkTest::serializesAndSignsEvents()
{
    NostrEvent event;
    event.pubkey = QStringLiteral("79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798");
    event.createdAt = 1700000000;
    event.kind = 25050;
    event.tags = {{QStringLiteral("p"), QStringLiteral("ab")}};
    event.content = QStringLiteral("a\n\"b\\ é");
    QCOMPARE(event.serializeForId(),
             QByteArray("[0,\"79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\",1700000000,25050,"
                        "[[\"p\",\"ab\"]],\"a\\n\\\"b\\\\ \xc3\xa9\"]"));
    QCOMPARE(event.computeId(), QStringLiteral("527312b762316ed85d256220a91257c29eb12004fbbdc71cf6543cb862e9a0d6"));

    const NostrKey key = NostrKey::generate();
    NostrEvent signedEvent = NostrEvent::create(key, 25050, {{QStringLiteral("p"), QStringLiteral("cd")}},
                                                QStringLiteral("hello"));
    QVERIFY(signedEvent.verify());
    QCOMPARE(signedEvent.tagValue(QStringLiteral("p")), QStringLiteral("cd"));
    const std::optional<NostrEvent> parsed = NostrEvent::fromJson(signedEvent.toJson());
    QVERIFY(parsed && parsed->verify());
    signedEvent.content = QStringLiteral("hello!");
    QVERIFY(!signedEvent.verify());
}

void PhoneLinkTest::parsesPairingLinks()
{
    PairingLink link;
    link.computerKey = QByteArray(32, '\x11');
    link.secret = QByteArray(32, '\x22');
    link.computerName = QStringLiteral("Studio di Francesco & co.");
    link.relays = {QStringLiteral("wss://relay.damus.io"), QStringLiteral("wss://nos.lol")};
    const QString text = link.toString();
    QVERIFY(text.startsWith(QLatin1String("pragma-chess://pair?k=1111")));
    const std::optional<PairingLink> parsed = PairingLink::parse(text);
    QVERIFY(parsed);
    QCOMPARE(parsed->computerKey, link.computerKey);
    QCOMPARE(parsed->secret, link.secret);
    QCOMPARE(parsed->computerName, link.computerName);
    QCOMPARE(parsed->relays, link.relays);

    QVERIFY(!PairingLink::parse(QStringLiteral("https://example.com/pair?k=11")));
    QVERIFY(!PairingLink::parse(QString(text).replace(QLatin1String("k=1111"), QLatin1String("k=11"))));
    QVERIFY(!PairingLink::parse(QStringLiteral("pragma-chess://pair?k=%1&s=%1").arg(QString(64, QLatin1Char('a')))));
}

void PhoneLinkTest::resolvesOnlyDatabasesInsideTheFolder()
{
    const QString root = QStringLiteral("/home/me/Chess/Pragma/Databases");
    QCOMPARE(PhoneFiles::resolve(root, QStringLiteral("Classic Games.pdb")),
             QStringLiteral("/home/me/Chess/Pragma/Databases/Classic Games.pdb"));
    QCOMPARE(PhoneFiles::resolve(root, QStringLiteral("Club/2026.pdb")),
             QStringLiteral("/home/me/Chess/Pragma/Databases/Club/2026.pdb"));
    for (const char *bad : {"../secret.pdb", "Club/../../x.pdb", "/etc/passwd.pdb", "games.sqlite", ".hidden.pdb",
                            "a/.git/x.pdb", "C:/x.pdb", "a\\b.pdb", "", "a//b.pdb", "./a.pdb"})
        QVERIFY2(!PhoneFiles::resolve(root, QString::fromLatin1(bad)), bad);
}

void PhoneLinkTest::parsesPhoneGames()
{
    QString error;
    const std::optional<QList<ImportedGame>> games = PhoneGames::parse({phoneGame(QStringLiteral("g1"))}, &error);
    QVERIFY2(games, qPrintable(error));
    QCOMPARE(games->size(), 1);
    const GameRecord &game = games->first().game;
    QCOMPARE(games->first().externalId, QStringLiteral("g1"));
    QCOMPARE(game.moves.size(), 4);
    QCOMPARE(game.moves.last().san, QStringLiteral("Qh4#"));
    QCOMPARE(game.result, QStringLiteral("0-1"));

    QJsonObject illegal = phoneGame(QStringLiteral("g2"));
    illegal.insert(QStringLiteral("moves_uci"), QStringLiteral("e2e5"));
    QVERIFY(!PhoneGames::parse({illegal}, &error));
    QVERIFY(error.contains(QLatin1String("illegal")));
    QJsonObject anonymous = phoneGame(QString());
    QVERIFY(!PhoneGames::parse({anonymous}, &error));
}

void PhoneLinkTest::storesPhoneGamesOnce()
{
    QTemporaryDir dir;
    QString error;
    const QString path = dir.filePath(QStringLiteral("Le mie partite.pdb"));
    DatabaseFolderStore store;
    const QList<ImportedGame> games =
        *PhoneGames::parse({phoneGame(QStringLiteral("a")), phoneGame(QStringLiteral("b"))}, &error);

    std::optional<PhoneGames::PutResult> result = store.storeGames(path, QStringLiteral("key"), QStringLiteral("Pixel"),
                                                                    games, &error);
    QVERIFY2(result, qPrintable(error));
    QCOMPARE(result->stored, 2);
    QCOMPARE(result->known, 0);
    result = store.storeGames(path, QStringLiteral("key"), QStringLiteral("Pixel"), games, &error);
    QVERIFY(result);
    QCOMPARE(result->stored, 0);
    QCOMPARE(result->known, 2);
    // Another phone's games with the same ids are its own.
    result = store.storeGames(path, QStringLiteral("other"), QStringLiteral("Tablet"), games.mid(0, 1), &error);
    QCOMPARE(result->stored, 1);

    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    QVERIFY(database);
    QCOMPARE(database->gameCount(), 3);
    QCOMPARE(database->sources().size(), 2);
    QCOMPARE(database->sources().first().kind, QStringLiteral("phone"));
    QCOMPARE(database->loadGame(0)->moves.size(), 4);
}

void PhoneLinkTest::pairsListsGetsAndPutsEndToEnd()
{
    rtc::InitLogger(rtc::LogLevel::None);
    LocalRelay relay;
    QTemporaryDir dir;
    const QString databases = dir.filePath(QStringLiteral("Databases"));
    QVERIFY(QDir().mkpath(databases));
    QString error;
    QVERIFY(SqliteGameDatabase::create(databases + QStringLiteral("/Classic.pdb"), {scholarsMate()}, &error));

    PhoneLink link(dir.filePath(QStringLiteral("phone-link.json")), databases);
    DatabaseFolderStore store;
    link.setGameStore(&store);
    link.setRelays({relay.url()});
    link.setIceServers({});
    const PairingLink pairing = link.beginPairing();
    QVERIFY(link.isListening());
    QTRY_COMPARE_WITH_TIMEOUT(link.connectedRelays(), 1, 10000);

    // A phone that never saw the code is refused.
    {
        PhoneLinkClient stranger(NostrKey::generate(), pairing.computerKey, pairing.relays, {}, QStringLiteral("?"));
        QSignalSpy failed(&stranger, &PhoneLinkClient::failed);
        stranger.start();
        QVERIFY(failed.wait(20000));
        QVERIFY(failed.first().first().toString().contains(QLatin1String("refused")));
    }

    const NostrKey phoneKey = NostrKey::generate();
    PhoneLinkClient phone(phoneKey, pairing.computerKey, pairing.relays, {}, QStringLiteral("Test phone"));
    phone.setPairingSecret(pairing.secret);
    QSignalSpy connected(&phone, &PhoneLinkClient::connected);
    QSignalSpy messages(&phone, &PhoneLinkClient::messageReceived);
    QByteArray received;
    connect(&phone, &PhoneLinkClient::binaryReceived, this, [&received](const QByteArray &data) { received += data; });
    phone.start();
    QVERIFY(connected.wait(20000));
    QCOMPARE(link.devices().size(), 1);
    QCOMPARE(link.devices().first().name, QStringLiteral("Test phone"));
    QVERIFY(link.pairingLink().secret != pairing.secret); // One phone per code.

    const auto next = [&messages]() -> QJsonObject {
        if (messages.isEmpty() && !messages.wait(20000))
            return {};
        return messages.takeFirst().first().toJsonObject();
    };

    phone.sendJson({{QStringLiteral("op"), QStringLiteral("list")}});
    const QJsonObject list = next();
    QCOMPARE(list.value(QStringLiteral("op")).toString(), QStringLiteral("list"));
    const QJsonArray files = list.value(QStringLiteral("files")).toArray();
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.first().toObject().value(QStringLiteral("name")).toString(), QStringLiteral("Classic.pdb"));

    phone.sendJson({{QStringLiteral("op"), QStringLiteral("get")}, {QStringLiteral("name"), QStringLiteral("Classic.pdb")}});
    const QJsonObject header = next();
    QCOMPARE(header.value(QStringLiteral("op")).toString(), QStringLiteral("file"));
    QCOMPARE(next().value(QStringLiteral("op")).toString(), QStringLiteral("done"));
    QCOMPARE(qint64(received.size()), header.value(QStringLiteral("size")).toInteger());
    QCOMPARE(QCryptographicHash::hash(received, QCryptographicHash::Sha256).toHex(),
             header.value(QStringLiteral("sha256")).toString().toLatin1());

    phone.sendJson({{QStringLiteral("op"), QStringLiteral("get")}, {QStringLiteral("name"), QStringLiteral("../x.pdb")}});
    QCOMPARE(next().value(QStringLiteral("op")).toString(), QStringLiteral("error"));

    const QJsonObject put{{QStringLiteral("op"), QStringLiteral("put")},
                          {QStringLiteral("name"), QStringLiteral("Le mie partite.pdb")},
                          {QStringLiteral("games"), QJsonArray{phoneGame(QStringLiteral("p1"))}}};
    phone.sendJson(put);
    QJsonObject answer = next();
    QCOMPARE(answer.value(QStringLiteral("stored")).toInt(), 1);
    phone.sendJson(put);
    answer = next();
    QCOMPARE(answer.value(QStringLiteral("stored")).toInt(), 0);
    QCOMPARE(answer.value(QStringLiteral("known")).toInt(), 1);
    QVERIFY(QFile::exists(databases + QStringLiteral("/Le mie partite.pdb")));
    QVERIFY(link.devices().first().lastSyncAt.isValid());

    // Unpairing the last phone stops listening once the dialog is closed.
    link.endPairing();
    QVERIFY(link.isListening());
    link.removeDevice(link.devices().first().key);
    QVERIFY(!link.isListening());
}

QTEST_GUILESS_MAIN(PhoneLinkTest)
#include "tst_phonelink.moc"
