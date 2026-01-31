#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>

/**
 * @brief LSP position in a document
 */
struct LspPosition {
    int line;
    int character;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["line"] = line;
        obj["character"] = character;
        return obj;
    }
    
    static LspPosition fromJson(const QJsonObject& obj) {
        return {obj["line"].toInt(), obj["character"].toInt()};
    }
};

/**
 * @brief LSP range in a document
 */
struct LspRange {
    LspPosition start;
    LspPosition end;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["start"] = start.toJson();
        obj["end"] = end.toJson();
        return obj;
    }
    
    static LspRange fromJson(const QJsonObject& obj) {
        return {
            LspPosition::fromJson(obj["start"].toObject()),
            LspPosition::fromJson(obj["end"].toObject())
        };
    }
};

/**
 * @brief LSP location (file + range)
 */
struct LspLocation {
    QString uri;
    LspRange range;
};

/**
 * @brief LSP diagnostic severity
 */
enum class LspDiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

/**
 * @brief LSP diagnostic message
 */
struct LspDiagnostic {
    LspRange range;
    LspDiagnosticSeverity severity;
    QString code;
    QString source;
    QString message;
};

/**
 * @brief LSP completion item
 */
struct LspCompletionItem {
    QString label;
    int kind;  // CompletionItemKind
    QString detail;
    QString documentation;
    QString insertText;
};

/**
 * @brief LSP parameter information for signature help
 */
struct LspParameterInfo {
    QString label;
    QString documentation;
};

/**
 * @brief LSP signature information for signature help
 */
struct LspSignatureInfo {
    QString label;
    QString documentation;
    QList<LspParameterInfo> parameters;
    int activeParameter;
};

/**
 * @brief LSP signature help response
 */
struct LspSignatureHelp {
    QList<LspSignatureInfo> signatures;
    int activeSignature;
    int activeParameter;
};

/**
 * @brief LSP symbol kind enumeration
 */
enum class LspSymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

/**
 * @brief LSP document symbol
 */
struct LspDocumentSymbol {
    QString name;
    QString detail;
    LspSymbolKind kind;
    LspRange range;
    LspRange selectionRange;
    QList<LspDocumentSymbol> children;
};

/**
 * @brief LSP text edit for rename operations
 */
struct LspTextEdit {
    LspRange range;
    QString newText;
};

/**
 * @brief LSP workspace edit for rename operations
 */
struct LspWorkspaceEdit {
    QMap<QString, QList<LspTextEdit>> changes;  // uri -> list of edits
};

/**
 * @brief Language Server Protocol client
 * 
 * Provides communication with language servers using JSON-RPC over stdio.
 * Supports:
 * - Initialize/shutdown lifecycle
 * - Text document synchronization
 * - Completion requests
 * - Hover information
 * - Go to definition
 * - Diagnostics
 */
class LspClient : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Client state
     */
    enum class State {
        Disconnected,
        Connecting,
        Initializing,
        Ready,
        ShuttingDown,
        Error
    };
    Q_ENUM(State)

    explicit LspClient(QObject* parent = nullptr);
    ~LspClient();

    /**
     * @brief Start the language server
     * @param program Path to the language server executable
     * @param arguments Command line arguments
     * @return true if server started
     */
    bool start(const QString& program, const QStringList& arguments = {});

    /**
     * @brief Stop the language server
     */
    void stop();

    /**
     * @brief Get current state
     * @return Client state
     */
    State state() const;

    /**
     * @brief Check if client is ready
     * @return true if ready for requests
     */
    bool isReady() const;

    // Document lifecycle
    void didOpen(const QString& uri, const QString& languageId, 
                 int version, const QString& text);
    void didChange(const QString& uri, int version, const QString& text);
    void didChangeDebounced(const QString& uri, int version, const QString& text);
    void didChangeIncremental(const QString& uri, int version, 
                              LspRange range, const QString& text);
    void didSave(const QString& uri);
    void didClose(const QString& uri);

    // Requests
    void requestCompletion(const QString& uri, LspPosition position);
    void requestHover(const QString& uri, LspPosition position);
    void requestDefinition(const QString& uri, LspPosition position);
    void requestReferences(const QString& uri, LspPosition position);
    void requestSignatureHelp(const QString& uri, LspPosition position);
    void requestDocumentSymbols(const QString& uri);
    void requestRename(const QString& uri, LspPosition position, const QString& newName);

signals:
    void stateChanged(State state);
    void initialized();
    void error(const QString& message);
    
    // Document notifications
    void diagnosticsReceived(const QString& uri, const QList<LspDiagnostic>& diagnostics);
    
    // Request responses
    void completionReceived(int requestId, const QList<LspCompletionItem>& items);
    void hoverReceived(int requestId, const QString& contents);
    void definitionReceived(int requestId, const QList<LspLocation>& locations);
    void referencesReceived(int requestId, const QList<LspLocation>& locations);
    void signatureHelpReceived(int requestId, const LspSignatureHelp& signatureHelp);
    void documentSymbolsReceived(int requestId, const QList<LspDocumentSymbol>& symbols);
    void renameReceived(int requestId, const LspWorkspaceEdit& workspaceEdit);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void sendRequest(const QString& method, const QJsonObject& params, int id);
    void sendNotification(const QString& method, const QJsonObject& params);
    void handleMessage(const QJsonObject& message);
    void handleResponse(int id, const QJsonValue& result, const QJsonValue& error);
    void handleNotification(const QString& method, const QJsonObject& params);
    
    void doInitialize();
    void setState(State state);
    void cancelPendingCompletionRequest();
    
    QProcess* m_process;
    State m_state;
    int m_nextRequestId;
    QString m_buffer;
    QMap<int, QString> m_pendingRequests;  // id -> method
    QString m_rootUri;
    int m_pendingCompletionRequestId = -1;  // Track current completion request for cancellation
    
    // Debounced didChange state
    QTimer* m_changeDebounceTimer = nullptr;
    QString m_pendingChangeUri;
    int m_pendingChangeVersion = 0;
    QString m_pendingChangeText;
};

#endif // LSPCLIENT_H
