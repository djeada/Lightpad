#include "lspclient.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>
#include <functional>

LspClient::LspClient(QObject *parent)
    : QObject(parent), m_process(nullptr), m_state(State::Disconnected),
      m_nextRequestId(1) {}

LspClient::~LspClient() { stop(); }

bool LspClient::start(const QString &program, const QStringList &arguments) {
  if (m_process) {
    LOG_WARNING("LSP client already started");
    return false;
  }

  m_process = new QProcess(this);
  m_process->setProgram(program);
  m_process->setArguments(arguments);

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &LspClient::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &LspClient::onReadyReadStandardError);
  connect(m_process, &QProcess::errorOccurred, this,
          &LspClient::onProcessError);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &LspClient::onProcessFinished);

  connect(m_process, &QProcess::started, this, [this]() {
    LOG_INFO(QString("LSP server started"));
    doInitialize();
  });

  setState(State::Connecting);
  m_process->start();

  LOG_INFO(QString("Starting LSP server: %1").arg(program));
  return true;
}

void LspClient::stop() {
  if (!m_process) {
    return;
  }

  setState(State::ShuttingDown);

  QJsonObject params;
  sendRequest("shutdown", params, m_nextRequestId++);

  sendNotification("exit", {});

  QTimer::singleShot(3000, this, [this]() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
      LOG_WARNING("LSP server did not exit gracefully, terminating");
      m_process->terminate();

      QTimer::singleShot(2000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
          LOG_WARNING("LSP server did not terminate, killing");
          m_process->kill();
        }
      });
    }
  });

  LOG_INFO("LSP server shutdown initiated (async)");
}

LspClient::State LspClient::state() const { return m_state; }

bool LspClient::isReady() const { return m_state == State::Ready; }

void LspClient::didOpen(const QString &uri, const QString &languageId,
                        int version, const QString &text) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;
  textDocument["languageId"] = languageId;
  textDocument["version"] = version;
  textDocument["text"] = text;

  QJsonObject params;
  params["textDocument"] = textDocument;

  sendNotification("textDocument/didOpen", params);
}

void LspClient::didChange(const QString &uri, int version,
                          const QString &text) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;
  textDocument["version"] = version;

  QJsonObject contentChange;
  contentChange["text"] = text;

  QJsonArray contentChanges;
  contentChanges.append(contentChange);

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["contentChanges"] = contentChanges;

  sendNotification("textDocument/didChange", params);
}

void LspClient::didChangeDebounced(const QString &uri, int version,
                                   const QString &text) {

  m_pendingChangeUri = uri;
  m_pendingChangeVersion = version;
  m_pendingChangeText = text;

  if (!m_changeDebounceTimer) {
    m_changeDebounceTimer = new QTimer(this);
    m_changeDebounceTimer->setSingleShot(true);
    connect(m_changeDebounceTimer, &QTimer::timeout, this, [this]() {
      if (!m_pendingChangeUri.isEmpty()) {
        didChange(m_pendingChangeUri, m_pendingChangeVersion,
                  m_pendingChangeText);
        m_pendingChangeUri.clear();
        m_pendingChangeText.clear();
      }
    });
  }

  m_changeDebounceTimer->start(100);
}

void LspClient::didChangeIncremental(const QString &uri, int version,
                                     LspRange range, const QString &text) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;
  textDocument["version"] = version;

  QJsonObject contentChange;
  contentChange["range"] = range.toJson();
  contentChange["text"] = text;

  QJsonArray contentChanges;
  contentChanges.append(contentChange);

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["contentChanges"] = contentChanges;

  sendNotification("textDocument/didChange", params);
}

void LspClient::didSave(const QString &uri) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;

  sendNotification("textDocument/didSave", params);
}

void LspClient::didClose(const QString &uri) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;

  sendNotification("textDocument/didClose", params);
}

void LspClient::requestCompletion(const QString &uri, LspPosition position) {

  cancelPendingCompletionRequest();

  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/completion";
  m_pendingCompletionRequestId = id;
  sendRequest("textDocument/completion", params, id);
}

void LspClient::cancelPendingCompletionRequest() {
  if (m_pendingCompletionRequestId > 0) {

    QJsonObject params;
    params["id"] = m_pendingCompletionRequestId;
    sendNotification("$/cancelRequest", params);

    m_pendingRequests.remove(m_pendingCompletionRequestId);
    m_pendingCompletionRequestId = -1;
  }
}

void LspClient::requestHover(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/hover";
  sendRequest("textDocument/hover", params, id);
}

int LspClient::requestDefinition(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/definition";
  sendRequest("textDocument/definition", params, id);
  return id;
}

void LspClient::requestReferences(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject context;
  context["includeDeclaration"] = true;

  QJsonObject params;
  params["textDocument"] = QJsonObject{{"uri", uri}};
  params["position"] = position.toJson();
  params["context"] = context;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/references";
  sendRequest("textDocument/references", params, id);
}

void LspClient::requestSignatureHelp(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/signatureHelp";
  sendRequest("textDocument/signatureHelp", params, id);
}

void LspClient::requestDocumentSymbols(const QString &uri) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/documentSymbol";
  sendRequest("textDocument/documentSymbol", params, id);
}

void LspClient::requestRename(const QString &uri, LspPosition position,
                              const QString &newName) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();
  params["newName"] = newName;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/rename";
  sendRequest("textDocument/rename", params, id);
}

void LspClient::requestCodeAction(const QString &uri, LspRange range,
                                  const QList<LspDiagnostic> &diagnostics) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject context;
  QJsonArray diagArray;
  for (const LspDiagnostic &diag : diagnostics) {
    QJsonObject diagObj;
    diagObj["range"] = diag.range.toJson();
    diagObj["severity"] = static_cast<int>(diag.severity);
    diagObj["message"] = diag.message;
    if (!diag.code.isEmpty())
      diagObj["code"] = diag.code;
    if (!diag.source.isEmpty())
      diagObj["source"] = diag.source;
    diagArray.append(diagObj);
  }
  context["diagnostics"] = diagArray;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["range"] = range.toJson();
  params["context"] = context;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/codeAction";
  sendRequest("textDocument/codeAction", params, id);
}

void LspClient::requestFormatting(const QString &uri, int tabSize,
                                  bool insertSpaces) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject options;
  options["tabSize"] = tabSize;
  options["insertSpaces"] = insertSpaces;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["options"] = options;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/formatting";
  sendRequest("textDocument/formatting", params, id);
}

int LspClient::requestDeclaration(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/declaration";
  sendRequest("textDocument/declaration", params, id);
  return id;
}

int LspClient::requestTypeDefinition(const QString &uri, LspPosition position) {
  QJsonObject textDocument;
  textDocument["uri"] = uri;

  QJsonObject params;
  params["textDocument"] = textDocument;
  params["position"] = position.toJson();

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "textDocument/typeDefinition";
  sendRequest("textDocument/typeDefinition", params, id);
  return id;
}

void LspClient::requestWorkspaceSymbols(const QString &query) {
  QJsonObject params;
  params["query"] = query;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "workspace/symbol";
  sendRequest("workspace/symbol", params, id);
}

void LspClient::setRootUri(const QString &rootUri) { m_rootUri = rootUri; }

const LspServerCapabilities &LspClient::serverCapabilities() const {
  return m_capabilities;
}

bool LspClient::supportsCapability(const QString &capability) const {
  static const QMap<QString, bool LspServerCapabilities::*> capMap = {
      {"hover", &LspServerCapabilities::hoverProvider},
      {"completion", &LspServerCapabilities::completionProvider},
      {"definition", &LspServerCapabilities::definitionProvider},
      {"declaration", &LspServerCapabilities::declarationProvider},
      {"typeDefinition", &LspServerCapabilities::typeDefinitionProvider},
      {"references", &LspServerCapabilities::referencesProvider},
      {"rename", &LspServerCapabilities::renameProvider},
      {"documentSymbol", &LspServerCapabilities::documentSymbolProvider},
      {"workspaceSymbol", &LspServerCapabilities::workspaceSymbolProvider},
      {"signatureHelp", &LspServerCapabilities::signatureHelpProvider},
      {"formatting", &LspServerCapabilities::documentFormattingProvider},
      {"codeAction", &LspServerCapabilities::codeActionProvider},
      {"semanticTokens", &LspServerCapabilities::semanticTokensProvider},
  };

  auto it = capMap.find(capability);
  if (it != capMap.end()) {
    return m_capabilities.*it.value();
  }
  return false;
}

void LspClient::sendRequest(const QString &method, const QJsonObject &params,
                            int id) {
  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["method"] = method;
  if (!params.isEmpty()) {
    message["params"] = params;
  }

  if (!m_process) {
    LOG_WARNING("LSP: Cannot send request, process not started");
    return;
  }

  writeMessage(message);

  LOG_DEBUG(QString("LSP request: %1 (id=%2)").arg(method).arg(id));
}

void LspClient::sendNotification(const QString &method,
                                 const QJsonObject &params) {
  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["method"] = method;
  if (!params.isEmpty()) {
    message["params"] = params;
  }

  if (!m_process) {
    LOG_WARNING("LSP: Cannot send notification, process not started");
    return;
  }

  writeMessage(message);

  LOG_DEBUG(QString("LSP notification: %1").arg(method));
}

void LspClient::sendResponse(const QJsonValue &id, const QJsonValue &result) {
  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["result"] = result;
  writeMessage(message);
}

void LspClient::sendErrorResponse(const QJsonValue &id, int code,
                                  const QString &errorMessage) {
  QJsonObject errorObj;
  errorObj["code"] = code;
  errorObj["message"] = errorMessage;

  QJsonObject message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["error"] = errorObj;
  writeMessage(message);
}

void LspClient::writeMessage(const QJsonObject &message) {
  if (!m_process) {
    return;
  }

  const QByteArray content =
      QJsonDocument(message).toJson(QJsonDocument::Compact);
  const QByteArray header = QByteArray("Content-Length: ") +
                            QByteArray::number(content.size()) + "\r\n\r\n";
  m_process->write(header);
  m_process->write(content);
}

QList<QByteArray> LspClient::extractMessages(QByteArray &buffer,
                                             int maxMessages) {
  QList<QByteArray> messages;

  while (messages.size() < maxMessages) {
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
      break;
    }

    const QByteArray header = buffer.left(headerEnd);
    int contentLength = -1;

    const QList<QByteArray> lines = header.split('\n');
    for (const QByteArray &line : lines) {
      const QByteArray trimmed = line.trimmed();
      if (trimmed.toLower().startsWith("content-length:")) {
        bool ok = false;
        const int parsed = trimmed.mid(15).trimmed().toInt(&ok);
        if (ok) {
          contentLength = parsed;
        }
        break;
      }
    }

    if (contentLength < 0) {
      buffer = buffer.mid(headerEnd + 4);
      messages.append(QByteArray());
      continue;
    }

    const int messageStart = headerEnd + 4;
    const int messageEnd = messageStart + contentLength;

    if (buffer.size() < messageEnd) {
      break;
    }

    messages.append(buffer.mid(messageStart, contentLength));
    buffer = buffer.mid(messageEnd);
  }

  return messages;
}

void LspClient::onReadyReadStandardOutput() {
  if (!m_process) {
    return;
  }
  m_buffer += m_process->readAllStandardOutput();

  constexpr int batchSize = 100;
  QList<QByteArray> messages;
  do {
    messages = extractMessages(m_buffer, batchSize);
    for (const QByteArray &content : messages) {
      if (content.isEmpty()) {
        LOG_WARNING("LSP message without Content-Length, skipping header");
        continue;
      }

      QJsonParseError parseError;
      QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);

      if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse LSP message: %1")
                      .arg(parseError.errorString()));
        continue;
      }

      handleMessage(doc.object());
    }
  } while (messages.size() == batchSize);
}

void LspClient::onReadyReadStandardError() {
  QString stderrText = QString::fromUtf8(m_process->readAllStandardError());
  LOG_DEBUG(QString("LSP stderr: %1").arg(stderrText.trimmed()));
}

void LspClient::onProcessError(QProcess::ProcessError processError) {
  QString errorMsg = m_process ? m_process->errorString() : "Unknown error";
  if (m_process && processError == QProcess::FailedToStart) {
    errorMsg = QString("Could not start '%1' (%2). Install the language server "
                       "or change its command in the language server settings.")
                   .arg(m_process->program(), errorMsg);
  }
  LOG_ERROR(QString("LSP process error: %1").arg(errorMsg));
  setState(State::Error);
  emit error(errorMsg);
}

void LspClient::onProcessFinished(int exitCode,
                                  QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitStatus);
  LOG_INFO(QString("LSP server exited with code: %1").arg(exitCode));

  if (m_process) {
    m_process->deleteLater();
    m_process = nullptr;
  }

  failPendingRequests(
      QString("Language server exited with code %1").arg(exitCode));
  setState(State::Disconnected);
}

void LspClient::failPendingRequests(const QString &message) {
  const QMap<int, QString> pending = m_pendingRequests;
  m_pendingRequests.clear();
  m_pendingCompletionRequestId = -1;
  for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
    emit requestFailed(it.key(), it.value(), message);
  }
}

void LspClient::handleMessage(const QJsonObject &message) {
  if (message.contains("id")) {

    if (message.contains("method")) {
      handleServerRequest(message["id"], message["method"].toString(),
                          message["params"]);
    } else {

      handleResponse(message["id"].toInt(), message["result"],
                     message["error"]);
    }
  } else if (message.contains("method")) {

    handleNotification(message["method"].toString(),
                       message["params"].toObject());
  }
}

void LspClient::handleServerRequest(const QJsonValue &id, const QString &method,
                                    const QJsonValue &params) {
  LOG_DEBUG(QString("LSP server request: %1").arg(method));

  if (method == "workspace/configuration") {
    QJsonArray results;
    const QJsonArray items = params.toObject()["items"].toArray();
    for (int i = 0; i < items.size(); ++i) {
      results.append(QJsonValue::Null);
    }
    sendResponse(id, results);
  } else if (method == "client/registerCapability" ||
             method == "client/unregisterCapability" ||
             method == "window/workDoneProgress/create" ||
             method == "window/showMessageRequest" ||
             method == "workspace/codeLens/refresh" ||
             method == "workspace/semanticTokens/refresh" ||
             method == "workspace/inlayHint/refresh" ||
             method == "workspace/diagnostic/refresh") {
    sendResponse(id, QJsonValue::Null);
  } else if (method == "workspace/workspaceFolders") {
    QJsonArray folders;
    if (!m_rootUri.isEmpty()) {
      QJsonObject folder;
      folder["uri"] = m_rootUri;
      folder["name"] = QUrl(m_rootUri).fileName();
      folders.append(folder);
    }
    sendResponse(id, folders);
  } else if (method == "workspace/applyEdit") {
    QJsonObject result;
    result["applied"] = false;
    sendResponse(id, result);
  } else {
    sendErrorResponse(id, -32601, QString("Method not found: %1").arg(method));
  }
}

void LspClient::handleResponse(int id, const QJsonValue &result,
                               const QJsonValue &errorVal) {
  QString method = m_pendingRequests.take(id);

  if (!errorVal.isNull() && !errorVal.isUndefined()) {
    QJsonObject errorObj = errorVal.toObject();
    const QString errorMessage = errorObj["message"].toString();
    LOG_ERROR(QString("LSP error for %1: %2").arg(method).arg(errorMessage));
    emit requestFailed(id, method, errorMessage);
    return;
  }

  if (method == "initialize") {
    QJsonObject resultObj = result.toObject();
    QJsonObject caps = resultObj["capabilities"].toObject();
    parseServerCapabilities(caps);
    setState(State::Ready);
    sendNotification("initialized", {});
    emit initialized();
    LOG_INFO("LSP client initialized");
  } else if (method == "textDocument/completion") {
    QList<LspCompletionItem> items;
    QJsonArray itemsArray = result.isArray()
                                ? result.toArray()
                                : result.toObject()["items"].toArray();
    for (const QJsonValue &val : itemsArray) {
      items.append(parseCompletionItem(val.toObject()));
    }
    emit completionReceived(id, items);
  } else if (method == "textDocument/hover") {
    QString contents;
    QJsonObject obj = result.toObject();
    QJsonValue contentsVal = obj["contents"];
    if (contentsVal.isString()) {
      contents = contentsVal.toString();
    } else if (contentsVal.isObject()) {
      contents = contentsVal.toObject()["value"].toString();
    }
    emit hoverReceived(id, contents);
  } else if (method == "textDocument/definition") {
    const QList<LspLocation> locations = parseLocations(result);
    emit definitionReceived(id, locations);
  } else if (method == "textDocument/references") {
    const QList<LspLocation> locations = parseLocations(result);
    emit referencesReceived(id, locations);
  } else if (method == "textDocument/signatureHelp") {
    LspSignatureHelp signatureHelp;
    QJsonObject obj = result.toObject();
    signatureHelp.activeSignature = obj["activeSignature"].toInt(0);
    signatureHelp.activeParameter = obj["activeParameter"].toInt(0);

    QJsonArray signaturesArray = obj["signatures"].toArray();
    for (const QJsonValue &sigVal : signaturesArray) {
      QJsonObject sigObj = sigVal.toObject();
      LspSignatureInfo sigInfo;
      sigInfo.label = sigObj["label"].toString();
      sigInfo.activeParameter = sigObj["activeParameter"].toInt(-1);

      QJsonValue docVal = sigObj["documentation"];
      if (docVal.isString()) {
        sigInfo.documentation = docVal.toString();
      } else if (docVal.isObject()) {
        sigInfo.documentation = docVal.toObject()["value"].toString();
      }

      QJsonArray paramsArray = sigObj["parameters"].toArray();
      for (const QJsonValue &paramVal : paramsArray) {
        QJsonObject paramObj = paramVal.toObject();
        LspParameterInfo paramInfo;

        QJsonValue labelVal = paramObj["label"];
        if (labelVal.isString()) {
          paramInfo.label = labelVal.toString();
        } else if (labelVal.isArray()) {
          QJsonArray labelArray = labelVal.toArray();

          paramInfo.label =
              sigInfo.label.mid(labelArray[0].toInt(),
                                labelArray[1].toInt() - labelArray[0].toInt());
        }

        QJsonValue paramDocVal = paramObj["documentation"];
        if (paramDocVal.isString()) {
          paramInfo.documentation = paramDocVal.toString();
        } else if (paramDocVal.isObject()) {
          paramInfo.documentation = paramDocVal.toObject()["value"].toString();
        }

        sigInfo.parameters.append(paramInfo);
      }
      signatureHelp.signatures.append(sigInfo);
    }
    emit signatureHelpReceived(id, signatureHelp);
  } else if (method == "textDocument/documentSymbol") {
    QList<LspDocumentSymbol> symbols;
    QJsonArray symbolsArray = result.toArray();

    std::function<LspDocumentSymbol(const QJsonObject &)> parseSymbol;
    parseSymbol = [&parseSymbol](const QJsonObject &obj) -> LspDocumentSymbol {
      LspDocumentSymbol symbol;
      symbol.name = obj["name"].toString();
      symbol.detail = obj["detail"].toString();
      symbol.kind = static_cast<LspSymbolKind>(obj["kind"].toInt());
      symbol.range = LspRange::fromJson(obj["range"].toObject());
      symbol.selectionRange =
          LspRange::fromJson(obj["selectionRange"].toObject());

      QJsonArray childrenArray = obj["children"].toArray();
      for (const QJsonValue &childVal : childrenArray) {
        symbol.children.append(parseSymbol(childVal.toObject()));
      }
      return symbol;
    };

    for (const QJsonValue &val : symbolsArray) {
      QJsonObject obj = val.toObject();

      if (obj.contains("range")) {

        symbols.append(parseSymbol(obj));
      } else if (obj.contains("location")) {

        LspDocumentSymbol symbol;
        symbol.name = obj["name"].toString();
        symbol.kind = static_cast<LspSymbolKind>(obj["kind"].toInt());
        QJsonObject location = obj["location"].toObject();
        symbol.range = LspRange::fromJson(location["range"].toObject());
        symbol.selectionRange = symbol.range;
        symbols.append(symbol);
      }
    }
    emit documentSymbolsReceived(id, symbols);
  } else if (method == "textDocument/rename") {
    LspWorkspaceEdit workspaceEdit;
    QJsonObject obj = result.toObject();

    QJsonObject changesObj = obj["changes"].toObject();
    for (const QString &uri : changesObj.keys()) {
      QList<LspTextEdit> edits;
      QJsonArray editsArray = changesObj[uri].toArray();
      for (const QJsonValue &editVal : editsArray) {
        QJsonObject editObj = editVal.toObject();
        LspTextEdit edit;
        edit.range = LspRange::fromJson(editObj["range"].toObject());
        edit.newText = editObj["newText"].toString();
        edits.append(edit);
      }
      workspaceEdit.changes[uri] = edits;
    }

    QJsonArray docChangesArray = obj["documentChanges"].toArray();
    for (const QJsonValue &docChangeVal : docChangesArray) {
      QJsonObject docChangeObj = docChangeVal.toObject();
      if (docChangeObj.contains("textDocument") &&
          docChangeObj.contains("edits")) {
        QString uri = docChangeObj["textDocument"].toObject()["uri"].toString();
        QList<LspTextEdit> edits;
        QJsonArray editsArray = docChangeObj["edits"].toArray();
        for (const QJsonValue &editVal : editsArray) {
          QJsonObject editObj = editVal.toObject();
          LspTextEdit edit;
          edit.range = LspRange::fromJson(editObj["range"].toObject());
          edit.newText = editObj["newText"].toString();
          edits.append(edit);
        }
        if (workspaceEdit.changes.contains(uri)) {
          workspaceEdit.changes[uri].append(edits);
        } else {
          workspaceEdit.changes[uri] = edits;
        }
      }
    }
    emit renameReceived(id, workspaceEdit);
  } else if (method == "textDocument/codeAction") {
    QList<LspCodeAction> actions;
    QJsonArray actionsArray = result.toArray();
    for (const QJsonValue &val : actionsArray) {
      QJsonObject obj = val.toObject();
      LspCodeAction action;
      action.title = obj["title"].toString();
      action.kind = obj["kind"].toString();
      action.isPreferred = obj["isPreferred"].toBool(false);

      QJsonArray diagArray = obj["diagnostics"].toArray();
      for (const QJsonValue &diagVal : diagArray) {
        QJsonObject diagObj = diagVal.toObject();
        LspDiagnostic diag;
        diag.range = LspRange::fromJson(diagObj["range"].toObject());
        diag.severity =
            static_cast<LspDiagnosticSeverity>(diagObj["severity"].toInt(1));
        diag.code = diagObj["code"].toString();
        diag.source = diagObj["source"].toString();
        diag.message = diagObj["message"].toString();
        action.diagnostics.append(diag);
      }

      QJsonObject editObj = obj["edit"].toObject();
      if (!editObj.isEmpty()) {
        QJsonObject changesObj = editObj["changes"].toObject();
        for (const QString &uri : changesObj.keys()) {
          QList<LspTextEdit> edits;
          QJsonArray editsArray = changesObj[uri].toArray();
          for (const QJsonValue &editVal : editsArray) {
            QJsonObject editItem = editVal.toObject();
            LspTextEdit edit;
            edit.range = LspRange::fromJson(editItem["range"].toObject());
            edit.newText = editItem["newText"].toString();
            edits.append(edit);
          }
          action.edit.changes[uri] = edits;
        }
      }

      actions.append(action);
    }
    emit codeActionReceived(id, actions);
  } else if (method == "textDocument/formatting") {
    QList<LspTextEdit> edits;
    QJsonArray editsArray = result.toArray();
    for (const QJsonValue &val : editsArray) {
      QJsonObject obj = val.toObject();
      LspTextEdit edit;
      edit.range = LspRange::fromJson(obj["range"].toObject());
      edit.newText = obj["newText"].toString();
      edits.append(edit);
    }
    emit formattingReceived(id, edits);
  } else if (method == "textDocument/declaration") {
    const QList<LspLocation> locations = parseLocations(result);
    emit declarationReceived(id, locations);
  } else if (method == "textDocument/typeDefinition") {
    const QList<LspLocation> locations = parseLocations(result);
    emit typeDefinitionReceived(id, locations);
  } else if (method == "workspace/symbol") {
    QList<LspDocumentSymbol> symbols;
    QJsonArray symbolsArray = result.toArray();
    for (const QJsonValue &val : symbolsArray) {
      QJsonObject obj = val.toObject();
      LspDocumentSymbol symbol;
      symbol.name = obj["name"].toString();
      symbol.kind = static_cast<LspSymbolKind>(obj["kind"].toInt());
      if (obj.contains("location")) {
        QJsonObject location = obj["location"].toObject();
        symbol.range = LspRange::fromJson(location["range"].toObject());
        symbol.selectionRange = symbol.range;
      }
      symbols.append(symbol);
    }
    emit workspaceSymbolsReceived(id, symbols);
  }
}

void LspClient::handleNotification(const QString &method,
                                   const QJsonObject &params) {
  if (method == "textDocument/publishDiagnostics") {
    QString uri = params["uri"].toString();
    QList<LspDiagnostic> diagnostics;

    QJsonArray diagArray = params["diagnostics"].toArray();
    for (const QJsonValue &val : diagArray) {
      QJsonObject obj = val.toObject();
      LspDiagnostic diag;
      diag.range = LspRange::fromJson(obj["range"].toObject());
      diag.severity =
          static_cast<LspDiagnosticSeverity>(obj["severity"].toInt(1));
      diag.code = obj["code"].toString();
      diag.source = obj["source"].toString();
      diag.message = obj["message"].toString();
      diagnostics.append(diag);
    }

    const QJsonValue versionVal = params["version"];
    const int version = versionVal.isDouble() ? versionVal.toInt(-1) : -1;
    emit diagnosticsReceived(uri, diagnostics, version);
  }
}

QList<LspLocation> LspClient::parseLocations(const QJsonValue &result) {
  QJsonArray locArray;
  if (result.isArray()) {
    locArray = result.toArray();
  } else if (result.isObject()) {
    locArray.append(result);
  }

  QList<LspLocation> locations;
  for (const QJsonValue &val : locArray) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    LspLocation loc;
    if (obj.contains("targetUri")) {
      loc.uri = obj["targetUri"].toString();
      const QJsonValue range = obj.contains("targetSelectionRange")
                                   ? obj["targetSelectionRange"]
                                   : obj["targetRange"];
      loc.range = LspRange::fromJson(range.toObject());
    } else {
      loc.uri = obj["uri"].toString();
      loc.range = LspRange::fromJson(obj["range"].toObject());
    }
    if (loc.uri.isEmpty()) {
      continue;
    }
    locations.append(loc);
  }
  return locations;
}

LspCompletionItem LspClient::parseCompletionItem(const QJsonObject &obj) {
  LspCompletionItem item;
  item.label = obj["label"].toString();
  item.kind = obj["kind"].toInt();
  item.detail = obj["detail"].toString();

  const QJsonValue docVal = obj["documentation"];
  if (docVal.isString()) {
    item.documentation = docVal.toString();
  } else if (docVal.isObject()) {
    item.documentation = docVal.toObject()["value"].toString();
  }

  const QJsonObject textEdit = obj["textEdit"].toObject();
  if (textEdit.contains("newText")) {
    item.insertText = textEdit["newText"].toString();
  } else {
    item.insertText = obj["insertText"].toString(item.label);
  }
  item.insertTextFormat = obj["insertTextFormat"].toInt(1);
  return item;
}

void LspClient::doInitialize() {
  setState(State::Initializing);

  QJsonObject capabilities;

  QJsonObject textDocumentSync;
  textDocumentSync["openClose"] = true;
  textDocumentSync["change"] = 1;
  textDocumentSync["save"] = true;

  QJsonObject textDocumentCaps;
  textDocumentCaps["synchronization"] = textDocumentSync;
  textDocumentCaps["completion"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["hover"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["definition"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["declaration"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["typeDefinition"] =
      QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["references"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["signatureHelp"] =
      QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["documentSymbol"] =
      QJsonObject{{"dynamicRegistration", false},
                  {"hierarchicalDocumentSymbolSupport", true}};
  textDocumentCaps["rename"] =
      QJsonObject{{"dynamicRegistration", false}, {"prepareSupport", false}};
  textDocumentCaps["formatting"] = QJsonObject{{"dynamicRegistration", false}};
  textDocumentCaps["codeAction"] = QJsonObject{{"dynamicRegistration", false}};

  capabilities["textDocument"] = textDocumentCaps;

  QJsonObject workspaceCaps;
  workspaceCaps["symbol"] = QJsonObject{{"dynamicRegistration", false}};
  capabilities["workspace"] = workspaceCaps;

  QJsonObject params;
  params["processId"] = QJsonValue::Null;
  params["rootUri"] =
      m_rootUri.isEmpty() ? QJsonValue::Null : QJsonValue(m_rootUri);
  params["capabilities"] = capabilities;

  int id = m_nextRequestId++;
  m_pendingRequests[id] = "initialize";
  sendRequest("initialize", params, id);
}

void LspClient::parseServerCapabilities(const QJsonObject &caps) {
  auto isTruthy = [](const QJsonValue &v) {
    return v.isBool() ? v.toBool() : v.isObject();
  };

  m_capabilities.hoverProvider = isTruthy(caps["hoverProvider"]);
  m_capabilities.completionProvider = isTruthy(caps["completionProvider"]);
  m_capabilities.definitionProvider = isTruthy(caps["definitionProvider"]);
  m_capabilities.declarationProvider = isTruthy(caps["declarationProvider"]);
  m_capabilities.typeDefinitionProvider =
      isTruthy(caps["typeDefinitionProvider"]);
  m_capabilities.referencesProvider = isTruthy(caps["referencesProvider"]);
  m_capabilities.renameProvider = isTruthy(caps["renameProvider"]);
  m_capabilities.documentSymbolProvider =
      isTruthy(caps["documentSymbolProvider"]);
  m_capabilities.workspaceSymbolProvider =
      isTruthy(caps["workspaceSymbolProvider"]);
  m_capabilities.signatureHelpProvider =
      isTruthy(caps["signatureHelpProvider"]);
  m_capabilities.documentFormattingProvider =
      isTruthy(caps["documentFormattingProvider"]);
  m_capabilities.codeActionProvider = isTruthy(caps["codeActionProvider"]);
  m_capabilities.semanticTokensProvider =
      isTruthy(caps["semanticTokensProvider"]);

  QJsonValue syncVal = caps["textDocumentSync"];
  if (syncVal.isDouble()) {
    m_capabilities.textDocumentSyncKind = syncVal.toInt(1);
  } else if (syncVal.isObject()) {
    m_capabilities.textDocumentSyncKind = syncVal.toObject()["change"].toInt(1);
  }

  LOG_INFO(QString("Server capabilities: hover=%1 completion=%2 definition=%3 "
                   "declaration=%4 typeDefinition=%5 references=%6 rename=%7 "
                   "formatting=%8 codeAction=%9")
               .arg(m_capabilities.hoverProvider)
               .arg(m_capabilities.completionProvider)
               .arg(m_capabilities.definitionProvider)
               .arg(m_capabilities.declarationProvider)
               .arg(m_capabilities.typeDefinitionProvider)
               .arg(m_capabilities.referencesProvider)
               .arg(m_capabilities.renameProvider)
               .arg(m_capabilities.documentFormattingProvider)
               .arg(m_capabilities.codeActionProvider));
}

void LspClient::setState(State state) {
  if (m_state != state) {
    m_state = state;
    emit stateChanged(state);
  }
}
