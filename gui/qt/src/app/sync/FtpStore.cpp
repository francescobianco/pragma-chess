#include "FtpStore.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTimer>
#include <QUuid>

namespace {

/// Without any sign of life from the server for this long, an operation fails.
constexpr int kIdleTimeoutMs = 45000;

} // namespace

FtpStore::FtpStore(const QString &host, quint16 port, bool tls, const QString &user, const QString &password,
                   const QString &folder, QObject *parent)
    : RemoteStore(parent)
    , m_host(host)
    , m_port(port)
    , m_tls(tls)
    , m_user(user.isEmpty() ? QStringLiteral("anonymous") : user)
    , m_password(password)
    , m_folder(folder)
    , m_control(new QSslSocket(this))
    , m_watchdog(new QTimer(this))
{
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(kIdleTimeoutMs);
    connect(m_watchdog, &QTimer::timeout, this, [this] {
        failAll(tr("The FTP server stopped answering."));
    });
    connect(m_control, &QSslSocket::readyRead, this, &FtpStore::readControl);
    connect(m_control, &QSslSocket::disconnected, this, [this] {
        m_loggedIn = false;
        if (!m_waiting.isEmpty())
            failAll(tr("The FTP server closed the connection."));
    });
    connect(m_control, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (error == QAbstractSocket::RemoteHostClosedError && m_waiting.isEmpty())
            return;
        failAll(tr("Could not talk to the FTP server: %1").arg(m_control->errorString()));
    });
}

FtpStore::~FtpStore()
{
    ++m_generation;
    m_control->abort();
}

QString FtpStore::identity() const
{
    return QStringLiteral("%1://%2@%3:%4/%5").arg(m_tls ? QStringLiteral("ftps") : QStringLiteral("ftp"), m_user, m_host)
        .arg(m_port).arg(m_folder);
}

QString FtpStore::remotePath(const QString &path) const
{
    QString folder = m_folder;
    while (folder.endsWith(QLatin1Char('/')))
        folder.chop(1);
    if (folder.isEmpty())
        return path;
    return folder + QLatin1Char('/') + path;
}

QByteArray FtpStore::encodePath(const QString &path) const
{
    return remotePath(path).toUtf8();
}

// Queue ------------------------------------------------------------------------

void FtpStore::enqueue(Operation operation, Callback done)
{
    m_queue.append({operation, done});
    runNext();
}

void FtpStore::runNext()
{
    if (m_running || m_queue.isEmpty())
        return;
    m_running = true;
    const auto [operation, done] = m_queue.takeFirst();
    m_current = done;
    const int generation = m_generation;
    auto finish = [this, done, generation](const Result &result) {
        if (generation != m_generation)
            return;
        m_running = false;
        m_current = nullptr;
        m_watchdog->stop();
        done(result);
        QTimer::singleShot(0, this, &FtpStore::runNext);
    };
    m_watchdog->start();
    if (m_loggedIn && m_control->state() == QAbstractSocket::ConnectedState) {
        operation(finish);
        return;
    }
    connectAndLogIn([operation, finish](const QString &error) {
        if (!error.isEmpty())
            finish(failure(error));
        else
            operation(finish);
    });
}

void FtpStore::failAll(const QString &error)
{
    // Everything in flight is dropped; the next operation logs in again.
    ++m_generation;
    m_waiting.clear();
    m_loggedIn = false;
    m_running = false;
    m_watchdog->stop();
    m_control->abort();
    const Callback current = m_current;
    m_current = nullptr;
    const auto queue = m_queue;
    m_queue.clear();
    if (current)
        current(failure(error));
    for (const auto &[operation, done] : queue)
        done(failure(error));
}

// Control connection -----------------------------------------------------------

void FtpStore::command(const QByteArray &line, Reply reply)
{
    m_waiting.append(reply);
    m_control->write(line + "\r\n");
    m_watchdog->start();
}

void FtpStore::expectReply(Reply reply)
{
    m_waiting.append(reply);
}

void FtpStore::readControl()
{
    m_watchdog->start();
    m_buffer += m_control->readAll();
    // A reply is "123 text", or "123-text" … "123 text" over several lines.
    while (true) {
        qsizetype end = m_buffer.indexOf("\r\n");
        if (end < 0)
            return;
        const QByteArray first = m_buffer.left(end);
        if (first.size() < 3) {
            m_buffer.remove(0, end + 2);
            continue;
        }
        const QByteArray code = first.left(3);
        qsizetype replyEnd = end;
        if (first.size() > 3 && first.at(3) == '-') {
            qsizetype search = end + 2;
            replyEnd = -1;
            while (true) {
                const qsizetype lineEnd = m_buffer.indexOf("\r\n", search);
                if (lineEnd < 0)
                    return; // Wait for the rest.
                const QByteArray line = m_buffer.mid(search, lineEnd - search);
                if (line.startsWith(code + ' ')) {
                    replyEnd = lineEnd;
                    break;
                }
                search = lineEnd + 2;
            }
        }
        const QString text = QString::fromUtf8(m_buffer.mid(4, replyEnd - 4)).trimmed();
        m_buffer.remove(0, replyEnd + 2);
        if (m_waiting.isEmpty())
            continue; // Unsolicited, e.g. a timeout notice before closing.
        const Reply reply = m_waiting.takeFirst();
        reply(code.toInt(), text);
    }
}

void FtpStore::connectAndLogIn(std::function<void(const QString &error)> done)
{
    m_buffer.clear();
    m_waiting.clear();
    m_loggedIn = false;
    if (m_control->state() != QAbstractSocket::UnconnectedState)
        m_control->abort();

    const auto fail = [done](int code, const QString &text) {
        done(QObject::tr("The FTP server refused (%1 %2).").arg(code).arg(text));
    };
    const auto login = [this, done, fail] {
        command("USER " + m_user.toUtf8(), [this, done, fail](int code, const QString &text) {
            const auto afterPassword = [this, done, fail](int passCode, const QString &passText) {
                if (passCode != 230 && passCode != 202) {
                    if (passCode == 530)
                        done(tr("The FTP server refused the user name or password."));
                    else
                        fail(passCode, passText);
                    return;
                }
                const auto binary = [this, done, fail] {
                    command("TYPE I", [this, done, fail](int typeCode, const QString &typeText) {
                        if (typeCode != 200) {
                            fail(typeCode, typeText);
                            return;
                        }
                        m_loggedIn = true;
                        done(QString());
                    });
                };
                if (!m_tls) {
                    binary();
                    return;
                }
                command("PBSZ 0", [this, binary](int, const QString &) {
                    command("PROT P", [binary](int, const QString &) { binary(); });
                });
            };
            if (code == 230) {
                afterPassword(230, text);
            } else if (code == 331) {
                command("PASS " + m_password.toUtf8(), afterPassword);
            } else {
                fail(code, text);
            }
        });
    };

    expectReply([this, done, fail, login](int code, const QString &text) {
        if (code != 220) {
            fail(code, text);
            return;
        }
        if (!m_tls) {
            login();
            return;
        }
        command("AUTH TLS", [this, done, fail, login](int authCode, const QString &authText) {
            if (authCode != 234) {
                fail(authCode, authText);
                return;
            }
            connect(m_control, &QSslSocket::encrypted, this, login, Qt::SingleShotConnection);
            m_control->setPeerVerifyName(m_host);
            m_control->startClientEncryption();
        });
    });
    m_control->connectToHost(m_host, m_port);
}

// Data connections -------------------------------------------------------------

void FtpStore::openData(std::function<void(QSslSocket *data, const QString &error)> done)
{
    command("EPSV", [this, done](int code, const QString &text) {
        const auto connectTo = [this, done](quint16 port) {
            auto *data = new QSslSocket(this);
            if (m_tls) {
                // Resume the control session: servers may require it for data connections.
                data->setSslConfiguration(m_control->sslConfiguration());
                data->setPeerVerifyName(m_host);
            }
            connect(data, &QAbstractSocket::connected, this, [data, done] { done(data, QString()); },
                    Qt::SingleShotConnection);
            connect(data, &QAbstractSocket::errorOccurred, this, [this, data, done](QAbstractSocket::SocketError error) {
                if (error == QAbstractSocket::RemoteHostClosedError)
                    return;
                if (data->state() != QAbstractSocket::ConnectedState && !data->property("reported").toBool()) {
                    data->setProperty("reported", true);
                    done(nullptr, tr("Could not open a data connection: %1").arg(data->errorString()));
                }
            });
            data->connectToHost(m_control->peerAddress(), port);
        };

        if (code == 229) {
            // "Entering Extended Passive Mode (|||6446|)"
            const QRegularExpressionMatch match = QRegularExpression(QStringLiteral(R"re(\(\|\|\|(\d+)\|\))re")).match(text);
            if (match.hasMatch()) {
                connectTo(quint16(match.captured(1).toUInt()));
                return;
            }
        }
        command("PASV", [this, done, connectTo](int pasvCode, const QString &pasvText) {
            // "Entering Passive Mode (h1,h2,h3,h4,p1,p2)"; the address is ignored (NAT).
            const QRegularExpressionMatch match =
                QRegularExpression(QStringLiteral(R"re((\d+),(\d+),(\d+),(\d+),(\d+),(\d+))re")).match(pasvText);
            if (pasvCode != 227 || !match.hasMatch()) {
                done(nullptr, tr("The FTP server does not support passive mode (%1 %2).").arg(pasvCode).arg(pasvText));
                return;
            }
            connectTo(quint16(match.captured(5).toUInt() * 256 + match.captured(6).toUInt()));
        });
    });
}

void FtpStore::retrieve(const QString &path, std::function<void(const QByteArray &)> sink, Callback done)
{
    const int generation = m_generation;
    openData([this, path, sink, done, generation](QSslSocket *data, const QString &error) {
        if (generation != m_generation)
            return;
        if (!data) {
            done(failure(error));
            return;
        }
        struct State {
            bool dataClosed = false;
            bool replied = false;
            int code = 0;
            QString text;
            bool reported = false;
        };
        auto state = std::make_shared<State>();
        const auto complete = [this, data, state, done, generation] {
            if (generation != m_generation || state->reported || !state->dataClosed || !state->replied)
                return;
            state->reported = true;
            data->deleteLater();
            if (state->code == 226 || state->code == 250)
                done(success());
            else if (state->code == 550) {
                Result missing;
                missing.notFound = true;
                missing.error = tr("The file does not exist on the FTP server.");
                done(missing);
            }
            else
                done(failure(tr("The FTP server could not send the file (%1 %2).").arg(state->code).arg(state->text)));
        };
        connect(data, &QAbstractSocket::readyRead, this, [this, data, sink] {
            m_watchdog->start();
            sink(data->readAll());
        });
        connect(data, &QAbstractSocket::disconnected, this, [data, sink, state, complete] {
            if (data->bytesAvailable() > 0)
                sink(data->readAll());
            state->dataClosed = true;
            complete();
        });
        if (m_tls)
            data->startClientEncryption();
        command("RETR " + encodePath(path), [this, data, state, complete](int code, const QString &text) {
            if (code == 150 || code == 125) {
                expectReply([state, complete](int finalCode, const QString &finalText) {
                    state->replied = true;
                    state->code = finalCode;
                    state->text = finalText;
                    complete();
                });
                return;
            }
            // Refused before any transfer: there will be no data.
            state->replied = true;
            state->code = code;
            state->text = text;
            state->dataClosed = true;
            data->abort();
            complete();
        });
    });
}

void FtpStore::store(const QString &path, std::function<QByteArray(qint64)> source, Callback done)
{
    const int generation = m_generation;
    openData([this, path, source, done, generation](QSslSocket *data, const QString &error) {
        if (generation != m_generation)
            return;
        if (!data) {
            done(failure(error));
            return;
        }
        struct State {
            bool accepted = false;
            bool ready = false;
            bool finished = false;
        };
        auto state = std::make_shared<State>();
        // Writes in chunks, keeping memory flat for large databases.
        auto pump = std::make_shared<std::function<void()>>();
        *pump = [this, data, source, state] {
            if (!state->accepted || !state->ready || state->finished)
                return;
            m_watchdog->start();
            while (data->bytesToWrite() < 256 * 1024) {
                const QByteArray chunk = source(64 * 1024);
                if (chunk.isEmpty()) {
                    state->finished = true;
                    data->disconnectFromHost(); // Flushes, then closes: the end of the file.
                    return;
                }
                data->write(chunk);
            }
        };
        connect(data, &QAbstractSocket::bytesWritten, this, [pump] { (*pump)(); });
        connect(data, &QSslSocket::encryptedBytesWritten, this, [pump] { (*pump)(); });
        if (m_tls) {
            connect(data, &QSslSocket::encrypted, this, [state, pump] {
                state->ready = true;
                (*pump)();
            });
            data->startClientEncryption();
        } else {
            state->ready = true;
        }
        command("STOR " + encodePath(path), [this, data, state, pump, done, generation](int code, const QString &text) {
            if (code != 150 && code != 125) {
                data->abort();
                data->deleteLater();
                done(failure(tr("The FTP server did not accept the file (%1 %2).").arg(code).arg(text)));
                return;
            }
            state->accepted = true;
            expectReply([this, data, done, generation](int finalCode, const QString &finalText) {
                data->deleteLater();
                if (generation != m_generation)
                    return;
                done(finalCode == 226 || finalCode == 250
                         ? success()
                         : failure(tr("The FTP server could not store the file (%1 %2).").arg(finalCode).arg(finalText)));
            });
            (*pump)();
        });
    });
}

void FtpStore::makeFolders(const QString &path, std::function<void()> done)
{
    QStringList folders;
    QString prefix;
    const QString full = remotePath(path);
    const bool absolute = full.startsWith(QLatin1Char('/'));
    const QStringList segments = full.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i + 1 < segments.size(); ++i) {
        prefix += (i > 0 || absolute ? QStringLiteral("/") : QString()) + segments.at(i);
        folders << prefix;
    }
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, folders, done, next]() mutable {
        if (folders.isEmpty()) {
            done();
            return;
        }
        // 257 created; 550 usually "already exists": the upload tells if it is not.
        command("MKD " + folders.takeFirst().toUtf8(), [next](int, const QString &) { (*next)(); });
    };
    (*next)();
}

void FtpStore::storeAndRename(const QString &path, std::function<QByteArray(qint64)> source, Callback done)
{
    const int generation = m_generation;
    makeFolders(path, [this, path, source, done, generation] {
        const QString partial = QStringLiteral("%1.%2.part").arg(path, QUuid::createUuid().toString(QUuid::Id128).left(8));
        store(partial, source, [this, path, partial, done, generation](const Result &stored) {
            if (generation != m_generation)
                return;
            if (!stored.ok) {
                done(stored);
                return;
            }
            const auto rename = std::make_shared<std::function<void(bool)>>();
            *rename = [this, path, partial, done, rename](bool retried) {
                command("RNFR " + encodePath(partial), [this, path, done, rename, retried](int code, const QString &text) {
                    if (code != 350) {
                        done(failure(tr("The FTP server could not rename the file (%1 %2).").arg(code).arg(text)));
                        return;
                    }
                    command("RNTO " + encodePath(path), [this, path, done, rename, retried](int toCode, const QString &toText) {
                        if (toCode == 250) {
                            done(success());
                        } else if (!retried) {
                            // Some servers do not rename over an existing file.
                            command("DELE " + encodePath(path), [rename](int, const QString &) { (*rename)(true); });
                        } else {
                            done(failure(tr("The FTP server could not rename the file (%1 %2).").arg(toCode).arg(toText)));
                        }
                    });
                });
            };
            (*rename)(false);
        });
    });
}

// Operations -------------------------------------------------------------------

void FtpStore::read(const QString &path, Callback done)
{
    enqueue([this, path](Callback finish) {
        auto content = std::make_shared<QByteArray>();
        retrieve(path, [content](const QByteArray &chunk) { *content += chunk; },
                 [content, finish](const Result &result) {
                     if (result.notFound) {
                         Result missing;
                         missing.ok = true;
                         missing.notFound = true;
                         finish(missing);
                         return;
                     }
                     finish(result.ok ? success(*content) : result);
                 });
    }, done);
}

void FtpStore::write(const QString &path, const QByteArray &data, Callback done)
{
    enqueue([this, path, data](Callback finish) {
        auto offset = std::make_shared<qint64>(0);
        storeAndRename(path, [data, offset](qint64 maxSize) {
            const QByteArray chunk = data.mid(*offset, maxSize);
            *offset += chunk.size();
            return chunk;
        }, finish);
    }, done);
}

void FtpStore::download(const QString &path, const QString &localFile, Callback done)
{
    enqueue([this, path, localFile](Callback finish) {
        auto file = std::make_shared<QSaveFile>(localFile);
        if (!file->open(QIODevice::WriteOnly)) {
            finish(failure(file->errorString()));
            return;
        }
        retrieve(path, [file](const QByteArray &chunk) { file->write(chunk); },
                 [file, finish](const Result &result) {
                     if (!result.ok) {
                         file->cancelWriting();
                         finish(result);
                         return;
                     }
                     finish(file->commit() ? success() : failure(file->errorString()));
                 });
    }, done);
}

void FtpStore::upload(const QString &localFile, const QString &path, Callback done)
{
    enqueue([this, localFile, path](Callback finish) {
        auto file = std::make_shared<QFile>(localFile);
        if (!file->open(QIODevice::ReadOnly)) {
            finish(failure(file->errorString()));
            return;
        }
        storeAndRename(path, [file](qint64 maxSize) { return file->read(maxSize); }, finish);
    }, done);
}

void FtpStore::remove(const QString &path, Callback done)
{
    enqueue([this, path](Callback finish) {
        command("DELE " + encodePath(path), [finish](int code, const QString &text) {
            if (code == 250) {
                finish(success());
            } else if (code == 550) {
                Result missing = success();
                missing.notFound = true;
                finish(missing);
            } else {
                finish(failure(tr("The FTP server could not delete the file (%1 %2).").arg(code).arg(text)));
            }
        });
    }, done);
}

void FtpStore::abort()
{
    ++m_generation;
    m_current = nullptr;
    m_queue.clear();
    m_waiting.clear();
    m_running = false;
    m_loggedIn = false;
    m_watchdog->stop();
    m_control->abort();
}
