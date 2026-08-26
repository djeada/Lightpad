

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstdio>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

class Channel {
public:
  virtual ~Channel() = default;
  virtual qint64 readBytes(char *data, qint64 maxSize) = 0;
  virtual bool writeBytes(const char *data, qint64 size) = 0;
};

class FdChannel : public Channel {
public:
  qint64 readBytes(char *data, qint64 maxSize) override {
#ifdef Q_OS_WIN
    return static_cast<qint64>(
        ::_read(0, data, static_cast<unsigned>(maxSize)));
#else
    return ::read(0, data, static_cast<size_t>(maxSize));
#endif
  }

  bool writeBytes(const char *data, qint64 size) override {
    qint64 written = 0;
    while (written < size) {
#ifdef Q_OS_WIN
      const qint64 n =
          ::_write(1, data + written, static_cast<unsigned>(size - written));
#else
      const qint64 n =
          ::write(1, data + written, static_cast<size_t>(size - written));
#endif
      if (n <= 0) {
        return false;
      }
      written += n;
    }
    return true;
  }
};

class TcpChannel : public Channel {
public:
  explicit TcpChannel(QTcpSocket *socket) : m_socket(socket) {}

  qint64 readBytes(char *data, qint64 maxSize) override {
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (m_socket->bytesAvailable() == 0) {
      if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return -1;
      }
      if (waitTimer.elapsed() > 60000) {
        return -1;
      }
      m_socket->waitForReadyRead(100);
    }
    return m_socket->read(data, maxSize);
  }

  bool writeBytes(const char *data, qint64 size) override {
    const qint64 written = m_socket->write(data, size);
    m_socket->flush();
    return written == size;
  }

private:
  QTcpSocket *m_socket;
};

QFile *g_traceFile = nullptr;
QJsonObject g_caps = []() {
  QJsonObject caps;
  caps["supportsConfigurationDoneRequest"] = true;
  caps["supportsRestartRequest"] = false;
  caps["supportsTerminateRequest"] = false;
  caps["supportsSetVariable"] = true;
  caps["supportsExceptionInfoRequest"] = true;
  caps["supportsDataBreakpoints"] = false;
  return caps;
}();
int g_nextSeq = 1;
bool g_stopOnLaunch = false;

void trace(const QJsonObject &message) {
  if (!g_traceFile) {
    return;
  }
  g_traceFile->write(QJsonDocument(message).toJson(QJsonDocument::Compact) +
                     "\n");
  g_traceFile->flush();
}

void sendMessage(Channel *channel, const QJsonObject &message) {
  const QByteArray content =
      QJsonDocument(message).toJson(QJsonDocument::Compact);
  QByteArray header;
  header += "Content-Length: ";
  header += QByteArray::number(content.size());
  header += "\r\n\r\n";
  channel->writeBytes(header.constData(), header.size());
  channel->writeBytes(content.constData(), content.size());
}

void sendResponse(Channel *channel, int requestSeq, const QString &command,
                  bool success, const QJsonObject &body = {},
                  const QString &errorMessage = {}) {
  QJsonObject response;
  response["seq"] = g_nextSeq++;
  response["type"] = "response";
  response["request_seq"] = requestSeq;
  response["success"] = success;
  response["command"] = command;
  if (!body.isEmpty()) {
    response["body"] = body;
  }
  if (!success && !errorMessage.isEmpty()) {
    response["message"] = errorMessage;
  }
  sendMessage(channel, response);
}

void sendEvent(Channel *channel, const QString &event,
               const QJsonObject &body = {}) {
  QJsonObject message;
  message["seq"] = g_nextSeq++;
  message["type"] = "event";
  message["event"] = event;
  if (!body.isEmpty()) {
    message["body"] = body;
  }
  sendMessage(channel, message);
}

void handleRequest(Channel *channel, const QJsonObject &request) {
  trace(request);

  const int seq = request.value("seq").toInt();
  const QString command = request.value("command").toString();

  if (command == "initialize") {
    sendResponse(channel, seq, command, true, g_caps);
    sendEvent(channel, "initialized");
  } else if (command == "launch" || command == "attach") {
    sendResponse(channel, seq, command, true);
    if (g_stopOnLaunch && command == "launch") {
      QJsonObject stoppedBody;
      stoppedBody["reason"] = "breakpoint";
      stoppedBody["threadId"] = 1;
      stoppedBody["allThreadsStopped"] = true;
      sendEvent(channel, "stopped", stoppedBody);
    }
  } else if (command == "configurationDone") {
    sendResponse(channel, seq, command, true);
  } else if (command == "setBreakpoints") {
    QJsonObject body;
    body["breakpoints"] = QJsonArray();
    sendResponse(channel, seq, command, true, body);
  } else if (command == "setFunctionBreakpoints") {
    QJsonObject body;
    body["breakpoints"] = QJsonArray();
    sendResponse(channel, seq, command, true, body);
  } else if (command == "setExceptionBreakpoints") {
    sendResponse(channel, seq, command, true);
  } else if (command == "threads") {
    QJsonObject thread;
    thread["id"] = 1;
    thread["name"] = QStringLiteral("main");
    QJsonObject body;
    body["threads"] = QJsonArray{thread};
    sendResponse(channel, seq, command, true, body);
  } else if (command == "stackTrace") {
    QJsonObject source;
    source["name"] = QStringLiteral("main.py");
    source["path"] = QStringLiteral("/tmp/main.py");
    QJsonObject frame;
    frame["id"] = 100;
    frame["name"] = QStringLiteral("<module>");
    frame["source"] = source;
    frame["line"] = 5;
    frame["column"] = 1;
    QJsonObject body;
    body["stackFrames"] = QJsonArray{frame};
    body["totalFrames"] = 1;
    sendResponse(channel, seq, command, true, body);
  } else if (command == "scopes") {
    QJsonObject scope;
    scope["name"] = QStringLiteral("Locals");
    scope["variablesReference"] = 1000;
    QJsonObject body;
    body["scopes"] = QJsonArray{scope};
    sendResponse(channel, seq, command, true, body);
  } else if (command == "variables") {
    QJsonObject first;
    first["name"] = QStringLiteral("x");
    first["value"] = QStringLiteral("41");
    first["type"] = QStringLiteral("int");
    QJsonObject second;
    second["name"] = QStringLiteral("\u6f22\u5b57_\u00e9\u00e8");
    second["value"] = QStringLiteral("v\u00e0lue \U0001F680");
    QJsonObject body;
    body["variables"] = QJsonArray{first, second};
    sendResponse(channel, seq, command, true, body);
  } else if (command == "evaluate") {
    const QString expression =
        request.value("arguments").toObject().value("expression").toString();
    QJsonObject body;
    body["result"] = expression;
    body["variablesReference"] = 0;
    sendResponse(channel, seq, command, true, body);
  } else if (command == "setVariable") {
    const QJsonObject args = request.value("arguments").toObject();
    QJsonObject body;
    body["value"] = args.value("value").toString();
    body["name"] = args.value("name").toString();
    sendResponse(channel, seq, command, true, body);
  } else if (command == "exceptionInfo") {
    QJsonObject details;
    details["typeName"] = QStringLiteral("ZeroDivisionError");
    details["message"] = QStringLiteral("division by zero");
    QJsonObject body;
    body["exceptionId"] = QStringLiteral("ZeroDivisionError");
    body["description"] = QStringLiteral("Unhandled exception");
    body["breakMode"] = QStringLiteral("unhandled");
    body["details"] = details;
    sendResponse(channel, seq, command, true, body);
  } else if (command == "disconnect" || command == "terminate") {
    sendResponse(channel, seq, command, true);
  } else {
    sendResponse(channel, seq, command, false, {},
                 QStringLiteral("unknown command"));
  }
}

bool parseHeader(const QByteArray &buffer, int *contentLength,
                 int *headerEndOut) {
  const int headerEnd = buffer.indexOf("\r\n\r\n");
  if (headerEnd < 0) {
    return false;
  }
  int length = 0;
  const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
  for (const QByteArray &line : lines) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.toLower().startsWith("content-length:")) {
      length = trimmed.mid(15).trimmed().toInt();
      break;
    }
  }
  *contentLength = length;
  *headerEndOut = headerEnd;
  return true;
}

int runSession(Channel *channel) {
  QByteArray buffer;
  char chunk[4096];

  while (true) {
    int contentLength = 0;
    int headerEnd = -1;
    if (!parseHeader(buffer, &contentLength, &headerEnd)) {
      const qint64 n = channel->readBytes(chunk, sizeof(chunk));
      if (n <= 0) {
        return 0;
      }
      buffer.append(chunk, static_cast<int>(n));
      continue;
    }

    if (contentLength <= 0) {
      buffer.remove(0, headerEnd + 4);
      continue;
    }

    const int messageStart = headerEnd + 4;
    if (buffer.size() < messageStart + contentLength) {
      const qint64 n = channel->readBytes(chunk, sizeof(chunk));
      if (n <= 0) {
        return 0;
      }
      buffer.append(chunk, static_cast<int>(n));
      continue;
    }

    const QJsonDocument doc =
        QJsonDocument::fromJson(buffer.mid(messageStart, contentLength));
    buffer.remove(0, messageStart + contentLength);

    if (!doc.isObject()) {
      continue;
    }

    const QJsonObject message = doc.object();
    const QString type = message.value("type").toString();
    if (type == "request") {
      handleRequest(channel, message);
      if (message.value("command").toString() == "disconnect" ||
          message.value("command").toString() == "terminate") {
        return 0;
      }
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);

  QString tracePath;
  quint16 listenPort = 0;
  const QStringList args = app.arguments().mid(1);
  for (int i = 0; i < args.size(); ++i) {
    if (args[i] == "--trace" && i + 1 < args.size()) {
      tracePath = args[++i];
    } else if (args[i] == "--caps" && i + 1 < args.size()) {
      g_caps = QJsonDocument::fromJson(args[++i].toUtf8()).object();
    } else if (args[i] == "--stop-on-launch") {
      g_stopOnLaunch = true;
    } else if (args[i] == "--listen" && i + 1 < args.size()) {
      listenPort = static_cast<quint16>(args[++i].toUInt());
    }
  }

  if (!tracePath.isEmpty()) {
    g_traceFile = new QFile(tracePath);
    g_traceFile->open(QIODevice::WriteOnly | QIODevice::Truncate |
                      QIODevice::Text);
  }

  if (listenPort > 0) {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, listenPort)) {
      std::fprintf(stderr, "listen failed on port %u\n", listenPort);
      std::fflush(stderr);
      return 2;
    }
    std::fprintf(stderr, "listening %u\n", listenPort);
    std::fflush(stderr);
    if (!server.waitForNewConnection(30000)) {
      return 3;
    }
    QTcpSocket *socket = server.nextPendingConnection();
    if (!socket) {
      return 4;
    }
    socket->waitForConnected(5000);
    TcpChannel channel(socket);
    const int result = runSession(&channel);
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
    return result;
  }

  FdChannel channel;
  return runSession(&channel);
}
