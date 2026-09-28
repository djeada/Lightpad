#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

QString g_logPath;
int g_initDelayMs = 0;

void logLine(const QString &line) {
  if (g_logPath.isEmpty()) {
    return;
  }
  QFile file(g_logPath);
  if (file.open(QIODevice::Append | QIODevice::Text)) {
    file.write(line.toUtf8() + "\n");
  }
}

void writeMessage(const QJsonObject &message, bool flush = true) {
  const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
  const std::string header =
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  std::fwrite(header.data(), 1, header.size(), stdout);
  std::fwrite(body.constData(), 1, body.size(), stdout);
  if (flush) {
    std::fflush(stdout);
  }
}

bool readMessage(QJsonObject &out) {
  int contentLength = -1;
  while (true) {
    std::string line;
    int c = 0;
    while ((c = std::fgetc(stdin)) != EOF && c != '\n') {
      line.push_back(static_cast<char>(c));
    }
    if (c == EOF) {
      return false;
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    if (line.rfind("Content-Length:", 0) == 0) {
      contentLength = std::atoi(line.substr(15).c_str());
    }
  }
  if (contentLength < 0) {
    return false;
  }
  QByteArray body(contentLength, '\0');
  if (std::fread(body.data(), 1, contentLength, stdin) !=
      static_cast<size_t>(contentLength)) {
    return false;
  }
  out = QJsonDocument::fromJson(body).object();
  return true;
}

void respond(const QJsonValue &id, const QJsonValue &result) {
  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["result"] = result;
  writeMessage(message);
}

void publishDiagnostics(const QString &uri, const QString &text,
                        const QJsonValue &version, bool flush = true) {
  QJsonObject diagnostic;
  diagnostic["range"] =
      QJsonObject{{"start", QJsonObject{{"line", 0}, {"character", 0}}},
                  {"end", QJsonObject{{"line", 0}, {"character", 1}}}};
  diagnostic["severity"] = 1;
  diagnostic["message"] = text;

  QJsonObject params;
  params["uri"] = QUrl(uri).toString(QUrl::FullyEncoded);
  params["diagnostics"] = QJsonArray{diagnostic};
  if (!version.isUndefined()) {
    params["version"] = version;
  }

  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["method"] = "textDocument/publishDiagnostics";
  message["params"] = params;
  writeMessage(message, flush);
}

void sendServerRequest(const QJsonValue &id, const QString &method,
                       const QJsonObject &params) {
  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["method"] = method;
  message["params"] = params;
  writeMessage(message);
}

QJsonObject locationFor(const QString &uri, int line) {
  return QJsonObject{
      {"uri", uri},
      {"range",
       QJsonObject{{"start", QJsonObject{{"line", line}, {"character", 2}}},
                   {"end", QJsonObject{{"line", line}, {"character", 3}}}}}};
}

void handleDocument(const QString &method, const QString &uri, int version,
                    const QString &text) {
  logLine(QString("%1 %2 %3 %4").arg(method, uri).arg(version).arg(text));

  if (method == "textDocument/didChange" && text.contains("CRASH")) {
    std::exit(3);
  }
  if (text.contains("BURST")) {
    for (int i = 0; i < 150; ++i) {
      publishDiagnostics(uri, QString("burst %1").arg(i), version, false);
    }
    std::fflush(stdout);
    return;
  }
  if (text.contains("STALE")) {
    publishDiagnostics(uri, "stale", version - 1);
    publishDiagnostics("file:///marker", "marker", QJsonValue::Undefined);
    return;
  }
  if (text.contains("NOVERSION")) {
    publishDiagnostics(uri, text, QJsonValue::Undefined);
    return;
  }
  publishDiagnostics(uri, text, version);
}

} // namespace

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg == "--log" && i + 1 < argc) {
      g_logPath = QString::fromLocal8Bit(argv[++i]);
    } else if (arg == "--init-delay" && i + 1 < argc) {
      g_initDelayMs = QString::fromLocal8Bit(argv[++i]).toInt();
    }
  }

  QJsonObject message;
  while (readMessage(message)) {
    const QString method = message["method"].toString();
    const QJsonValue id = message["id"];
    const QJsonObject params = message["params"].toObject();

    if (method.isEmpty()) {
      const QJsonValue payload =
          message.contains("error") ? message["error"] : message["result"];
      const QString idText =
          id.isString() ? id.toString() : QString::number(id.toInt());
      QString payloadText;
      if (payload.isArray()) {
        payloadText = QString::fromUtf8(
            QJsonDocument(payload.toArray()).toJson(QJsonDocument::Compact));
      } else if (payload.isObject()) {
        payloadText = QString::fromUtf8(
            QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact));
      } else if (payload.isNull()) {
        payloadText = "null";
      } else {
        payloadText = "undefined";
      }
      logLine(QString("response %1 %2").arg(idText, payloadText));
      continue;
    }

    if (method == "initialize") {
      logLine("initialize");
      if (g_initDelayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(g_initDelayMs));
      }
      sendServerRequest(
          "cfg", "workspace/configuration",
          QJsonObject{{"items", QJsonArray{QJsonObject{}, QJsonObject{}}}});
      sendServerRequest(77, "client/registerCapability",
                        QJsonObject{{"registrations", QJsonArray{}}});
      sendServerRequest(78, "custom/unknownRequest", QJsonObject{});
      QJsonObject capabilities;
      capabilities["textDocumentSync"] = 1;
      capabilities["definitionProvider"] = true;
      capabilities["completionProvider"] = QJsonObject{};
      respond(id, QJsonObject{{"capabilities", capabilities}});
    } else if (method == "shutdown") {
      respond(id, QJsonValue::Null);
    } else if (method == "exit") {
      return 0;
    } else if (method == "textDocument/didOpen") {
      const QJsonObject doc = params["textDocument"].toObject();
      handleDocument(method, doc["uri"].toString(), doc["version"].toInt(),
                     doc["text"].toString());
    } else if (method == "textDocument/didChange") {
      const QJsonObject doc = params["textDocument"].toObject();
      const QJsonArray changes = params["contentChanges"].toArray();
      const QString text = changes.isEmpty()
                               ? QString()
                               : changes.last().toObject()["text"].toString();
      handleDocument(method, doc["uri"].toString(), doc["version"].toInt(),
                     text);
    } else if (method == "textDocument/didClose" ||
               method == "textDocument/didSave") {
      logLine(QString("%1 %2").arg(
          method, params["textDocument"].toObject()["uri"].toString()));
    } else if (method == "textDocument/definition") {
      const QString uri = params["textDocument"].toObject()["uri"].toString();
      const int line = params["position"].toObject()["line"].toInt();
      logLine(QString("%1 %2 %3").arg(method, uri).arg(line));
      if (line == 0) {
        respond(id, QJsonValue::Null);
      } else if (line == 1) {
        respond(id, locationFor(uri, line + 10));
      } else if (line == 2) {
        QJsonObject error;
        error["code"] = -32603;
        error["message"] = "definition failed";
        QJsonObject reply;
        reply["jsonrpc"] = "2.0";
        reply["id"] = id;
        reply["error"] = error;
        writeMessage(reply);
      } else {
        respond(id, QJsonArray{locationFor(uri, line + 10)});
      }
    } else if (method == "textDocument/hover") {
      logLine(method);
    } else if (!id.isUndefined()) {
      respond(id, QJsonValue::Null);
    }
  }
  return 0;
}
