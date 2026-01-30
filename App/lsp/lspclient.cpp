#include "lspclient.h"
#include "../core/logging/logger.h"

#include <QJsonDocument>
#include <QDir>

LspClient::LspClient(QObject* parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_state(State::Disconnected)
    , m_nextRequestId(1)
{
}

LspClient::~LspClient()
{
    stop();
}

bool LspClient::start(const QString& program, const QStringList& arguments)
{
    if (m_process) {
        LOG_WARNING("LSP client already started");
        return false;
    }
    
    m_process = new QProcess(this);
    m_process->setProgram(program);
    m_process->setArguments(arguments);
    
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &LspClient::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &LspClient::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred,
            this, &LspClient::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LspClient::onProcessFinished);
    
    setState(State::Connecting);
    m_process->start();
    
    if (!m_process->waitForStarted(5000)) {
        LOG_ERROR(QString("Failed to start LSP server: %1").arg(program));
        setState(State::Error);
        emit error("Failed to start language server");
        delete m_process;
        m_process = nullptr;
        return false;
    }
    
    LOG_INFO(QString("Started LSP server: %1").arg(program));
    doInitialize();
    
    return true;
}

void LspClient::stop()
{
    if (!m_process) {
        return;
    }
    
    setState(State::ShuttingDown);
    
    // Send shutdown request
    QJsonObject params;
    sendRequest("shutdown", params, m_nextRequestId++);
    
    // Wait briefly for shutdown response
    m_process->waitForReadyRead(1000);
    
    // Send exit notification
    sendNotification("exit", {});
    
    // Give server time to exit gracefully
    if (!m_process->waitForFinished(3000)) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
        }
    }
    
    delete m_process;
    m_process = nullptr;
    setState(State::Disconnected);
    
    LOG_INFO("LSP server stopped");
}

LspClient::State LspClient::state() const
{
    return m_state;
}

bool LspClient::isReady() const
{
    return m_state == State::Ready;
}

void LspClient::didOpen(const QString& uri, const QString& languageId,
                        int version, const QString& text)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    textDocument["languageId"] = languageId;
    textDocument["version"] = version;
    textDocument["text"] = text;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    
    sendNotification("textDocument/didOpen", params);
}

void LspClient::didChange(const QString& uri, int version, const QString& text)
{
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

void LspClient::didSave(const QString& uri)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    
    sendNotification("textDocument/didSave", params);
}

void LspClient::didClose(const QString& uri)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    
    sendNotification("textDocument/didClose", params);
}

void LspClient::requestCompletion(const QString& uri, LspPosition position)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    params["position"] = position.toJson();
    
    int id = m_nextRequestId++;
    m_pendingRequests[id] = "textDocument/completion";
    sendRequest("textDocument/completion", params, id);
}

void LspClient::requestHover(const QString& uri, LspPosition position)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    params["position"] = position.toJson();
    
    int id = m_nextRequestId++;
    m_pendingRequests[id] = "textDocument/hover";
    sendRequest("textDocument/hover", params, id);
}

void LspClient::requestDefinition(const QString& uri, LspPosition position)
{
    QJsonObject textDocument;
    textDocument["uri"] = uri;
    
    QJsonObject params;
    params["textDocument"] = textDocument;
    params["position"] = position.toJson();
    
    int id = m_nextRequestId++;
    m_pendingRequests[id] = "textDocument/definition";
    sendRequest("textDocument/definition", params, id);
}

void LspClient::requestReferences(const QString& uri, LspPosition position)
{
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

void LspClient::sendRequest(const QString& method, const QJsonObject& params, int id)
{
    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["id"] = id;
    message["method"] = method;
    if (!params.isEmpty()) {
        message["params"] = params;
    }
    
    QJsonDocument doc(message);
    QByteArray content = doc.toJson(QJsonDocument::Compact);
    
    if (!m_process) {
        LOG_WARNING("LSP: Cannot send request, process not started");
        return;
    }
    
    QString header = QString("Content-Length: %1\r\n\r\n").arg(content.size());
    m_process->write(header.toUtf8());
    m_process->write(content);
    
    LOG_DEBUG(QString("LSP request: %1 (id=%2)").arg(method).arg(id));
}

void LspClient::sendNotification(const QString& method, const QJsonObject& params)
{
    QJsonObject message;
    message["jsonrpc"] = "2.0";
    message["method"] = method;
    if (!params.isEmpty()) {
        message["params"] = params;
    }
    
    QJsonDocument doc(message);
    QByteArray content = doc.toJson(QJsonDocument::Compact);
    
    if (!m_process) {
        LOG_WARNING("LSP: Cannot send notification, process not started");
        return;
    }
    
    QString header = QString("Content-Length: %1\r\n\r\n").arg(content.size());
    m_process->write(header.toUtf8());
    m_process->write(content);
    
    LOG_DEBUG(QString("LSP notification: %1").arg(method));
}

void LspClient::onReadyReadStandardOutput()
{
    if (!m_process) {
        return;
    }
    m_buffer += QString::fromUtf8(m_process->readAllStandardOutput());
    
    // Parse LSP messages from buffer (limit iterations to prevent infinite loops)
    const int maxIterations = 100;
    int iterations = 0;
    
    while (iterations < maxIterations) {
        ++iterations;
        
        int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            break;
        }
        
        QString header = m_buffer.left(headerEnd);
        int contentLength = 0;
        
        // Parse Content-Length header
        QStringList lines = header.split("\r\n");
        for (const QString& line : lines) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
                contentLength = line.mid(15).trimmed().toInt();
                break;
            }
        }
        
        if (contentLength == 0) {
            LOG_WARNING("LSP message without Content-Length, skipping header");
            m_buffer = m_buffer.mid(headerEnd + 4);
            continue;
        }
        
        int messageStart = headerEnd + 4;
        int messageEnd = messageStart + contentLength;
        
        if (m_buffer.size() < messageEnd) {
            // Not enough data yet
            break;
        }
        
        QString content = m_buffer.mid(messageStart, contentLength);
        m_buffer = m_buffer.mid(messageEnd);
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            LOG_ERROR(QString("Failed to parse LSP message: %1").arg(parseError.errorString()));
            continue;
        }
        
        handleMessage(doc.object());
    }
}

void LspClient::onReadyReadStandardError()
{
    QString stderr = QString::fromUtf8(m_process->readAllStandardError());
    LOG_DEBUG(QString("LSP stderr: %1").arg(stderr.trimmed()));
}

void LspClient::onProcessError(QProcess::ProcessError processError)
{
    Q_UNUSED(processError);
    QString errorMsg = m_process ? m_process->errorString() : "Unknown error";
    LOG_ERROR(QString("LSP process error: %1").arg(errorMsg));
    setState(State::Error);
    emit error(errorMsg);
}

void LspClient::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    LOG_INFO(QString("LSP server exited with code: %1").arg(exitCode));
    setState(State::Disconnected);
}

void LspClient::handleMessage(const QJsonObject& message)
{
    if (message.contains("id")) {
        // Response or request
        int id = message["id"].toInt();
        
        if (message.contains("method")) {
            // Server request (not common, but possible)
            LOG_DEBUG(QString("LSP server request: %1").arg(message["method"].toString()));
        } else {
            // Response to our request
            handleResponse(id, message["result"], message["error"]);
        }
    } else if (message.contains("method")) {
        // Notification
        handleNotification(message["method"].toString(), message["params"].toObject());
    }
}

void LspClient::handleResponse(int id, const QJsonValue& result, const QJsonValue& errorVal)
{
    QString method = m_pendingRequests.take(id);
    
    if (!errorVal.isNull() && !errorVal.isUndefined()) {
        QJsonObject errorObj = errorVal.toObject();
        LOG_ERROR(QString("LSP error for %1: %2")
            .arg(method)
            .arg(errorObj["message"].toString()));
        return;
    }
    
    if (method == "initialize") {
        setState(State::Ready);
        sendNotification("initialized", {});
        emit initialized();
        LOG_INFO("LSP client initialized");
    } else if (method == "textDocument/completion") {
        QList<LspCompletionItem> items;
        QJsonArray itemsArray = result.isArray() ? result.toArray() 
                                                 : result.toObject()["items"].toArray();
        for (const QJsonValue& val : itemsArray) {
            QJsonObject obj = val.toObject();
            LspCompletionItem item;
            item.label = obj["label"].toString();
            item.kind = obj["kind"].toInt();
            item.detail = obj["detail"].toString();
            item.insertText = obj["insertText"].toString(item.label);
            items.append(item);
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
        QList<LspLocation> locations;
        QJsonArray locArray = result.isArray() ? result.toArray() : QJsonArray{result.toObject()};
        for (const QJsonValue& val : locArray) {
            QJsonObject obj = val.toObject();
            LspLocation loc;
            loc.uri = obj["uri"].toString();
            loc.range = LspRange::fromJson(obj["range"].toObject());
            locations.append(loc);
        }
        emit definitionReceived(id, locations);
    } else if (method == "textDocument/references") {
        QList<LspLocation> locations;
        QJsonArray locArray = result.toArray();
        for (const QJsonValue& val : locArray) {
            QJsonObject obj = val.toObject();
            LspLocation loc;
            loc.uri = obj["uri"].toString();
            loc.range = LspRange::fromJson(obj["range"].toObject());
            locations.append(loc);
        }
        emit referencesReceived(id, locations);
    }
}

void LspClient::handleNotification(const QString& method, const QJsonObject& params)
{
    if (method == "textDocument/publishDiagnostics") {
        QString uri = params["uri"].toString();
        QList<LspDiagnostic> diagnostics;
        
        QJsonArray diagArray = params["diagnostics"].toArray();
        for (const QJsonValue& val : diagArray) {
            QJsonObject obj = val.toObject();
            LspDiagnostic diag;
            diag.range = LspRange::fromJson(obj["range"].toObject());
            diag.severity = static_cast<LspDiagnosticSeverity>(obj["severity"].toInt(1));
            diag.code = obj["code"].toString();
            diag.source = obj["source"].toString();
            diag.message = obj["message"].toString();
            diagnostics.append(diag);
        }
        
        emit diagnosticsReceived(uri, diagnostics);
    }
}

void LspClient::doInitialize()
{
    setState(State::Initializing);
    
    QJsonObject capabilities;
    
    // Text document capabilities
    QJsonObject textDocumentSync;
    textDocumentSync["openClose"] = true;
    textDocumentSync["change"] = 1;  // Full sync
    textDocumentSync["save"] = true;
    
    QJsonObject textDocumentCaps;
    textDocumentCaps["synchronization"] = textDocumentSync;
    textDocumentCaps["completion"] = QJsonObject{{"dynamicRegistration", false}};
    textDocumentCaps["hover"] = QJsonObject{{"dynamicRegistration", false}};
    textDocumentCaps["definition"] = QJsonObject{{"dynamicRegistration", false}};
    textDocumentCaps["references"] = QJsonObject{{"dynamicRegistration", false}};
    
    capabilities["textDocument"] = textDocumentCaps;
    
    QJsonObject params;
    params["processId"] = QJsonValue::Null;
    params["rootUri"] = m_rootUri.isEmpty() ? QJsonValue::Null : QJsonValue(m_rootUri);
    params["capabilities"] = capabilities;
    
    int id = m_nextRequestId++;
    m_pendingRequests[id] = "initialize";
    sendRequest("initialize", params, id);
}

void LspClient::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}
