#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>

struct LspPosition {
  int line;
  int character;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["line"] = line;
    obj["character"] = character;
    return obj;
  }

  static LspPosition fromJson(const QJsonObject &obj) {
    return {obj["line"].toInt(), obj["character"].toInt()};
  }
};

struct LspRange {
  LspPosition start;
  LspPosition end;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["start"] = start.toJson();
    obj["end"] = end.toJson();
    return obj;
  }

  static LspRange fromJson(const QJsonObject &obj) {
    return {LspPosition::fromJson(obj["start"].toObject()),
            LspPosition::fromJson(obj["end"].toObject())};
  }
};

struct LspLocation {
  QString uri;
  LspRange range;
};

enum class LspDiagnosticSeverity {
  Error = 1,
  Warning = 2,
  Information = 3,
  Hint = 4
};

struct LspDiagnostic {
  LspRange range;
  LspDiagnosticSeverity severity;
  QString code;
  QString source;
  QString message;
};

struct LspCompletionItem {
  QString label;
  int kind;
  QString detail;
  QString documentation;
  QString insertText;
};

struct LspParameterInfo {
  QString label;
  QString documentation;
};

struct LspSignatureInfo {
  QString label;
  QString documentation;
  QList<LspParameterInfo> parameters;
  int activeParameter;
};

struct LspSignatureHelp {
  QList<LspSignatureInfo> signatures;
  int activeSignature;
  int activeParameter;
};

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

struct LspDocumentSymbol {
  QString name;
  QString detail;
  LspSymbolKind kind;
  LspRange range;
  LspRange selectionRange;
  QList<LspDocumentSymbol> children;
};

struct LspTextEdit {
  LspRange range;
  QString newText;
};

struct LspWorkspaceEdit {
  QMap<QString, QList<LspTextEdit>> changes;
};

namespace LspCodeActionKind {
inline const QString QuickFix = "quickfix";
inline const QString Refactor = "refactor";
inline const QString Source = "source";
inline const QString SourceOrganizeImports = "source.organizeImports";
} // namespace LspCodeActionKind

struct LspCodeAction {
  QString title;
  QString kind;
  QList<LspDiagnostic> diagnostics;
  LspWorkspaceEdit edit;
  bool isPreferred;
};

class LspClient : public QObject {
  Q_OBJECT

public:
  enum class State {
    Disconnected,
    Connecting,
    Initializing,
    Ready,
    ShuttingDown,
    Error
  };
  Q_ENUM(State)

  explicit LspClient(QObject *parent = nullptr);
  ~LspClient();

  bool start(const QString &program, const QStringList &arguments = {});

  void stop();

  State state() const;

  bool isReady() const;

  void didOpen(const QString &uri, const QString &languageId, int version,
               const QString &text);
  void didChange(const QString &uri, int version, const QString &text);
  void didChangeDebounced(const QString &uri, int version, const QString &text);
  void didChangeIncremental(const QString &uri, int version, LspRange range,
                            const QString &text);
  void didSave(const QString &uri);
  void didClose(const QString &uri);

  void requestCompletion(const QString &uri, LspPosition position);
  void requestHover(const QString &uri, LspPosition position);
  void requestDefinition(const QString &uri, LspPosition position);
  void requestReferences(const QString &uri, LspPosition position);
  void requestSignatureHelp(const QString &uri, LspPosition position);
  void requestDocumentSymbols(const QString &uri);
  void requestRename(const QString &uri, LspPosition position,
                     const QString &newName);

  void requestCodeAction(const QString &uri, LspRange range,
                         const QList<LspDiagnostic> &diagnostics = {});

signals:
  void stateChanged(State state);
  void initialized();
  void error(const QString &message);

  void diagnosticsReceived(const QString &uri,
                           const QList<LspDiagnostic> &diagnostics);

  void completionReceived(int requestId, const QList<LspCompletionItem> &items);
  void hoverReceived(int requestId, const QString &contents);
  void definitionReceived(int requestId, const QList<LspLocation> &locations);
  void referencesReceived(int requestId, const QList<LspLocation> &locations);
  void signatureHelpReceived(int requestId,
                             const LspSignatureHelp &signatureHelp);
  void documentSymbolsReceived(int requestId,
                               const QList<LspDocumentSymbol> &symbols);
  void renameReceived(int requestId, const LspWorkspaceEdit &workspaceEdit);
  void codeActionReceived(int requestId, const QList<LspCodeAction> &actions);

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessError(QProcess::ProcessError error);
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  void sendRequest(const QString &method, const QJsonObject &params, int id);
  void sendNotification(const QString &method, const QJsonObject &params);
  void handleMessage(const QJsonObject &message);
  void handleResponse(int id, const QJsonValue &result,
                      const QJsonValue &error);
  void handleNotification(const QString &method, const QJsonObject &params);

  void doInitialize();
  void setState(State state);
  void cancelPendingCompletionRequest();

  QProcess *m_process;
  State m_state;
  int m_nextRequestId;
  QString m_buffer;
  QMap<int, QString> m_pendingRequests;
  QString m_rootUri;
  int m_pendingCompletionRequestId = -1;

  QTimer *m_changeDebounceTimer = nullptr;
  QString m_pendingChangeUri;
  int m_pendingChangeVersion = 0;
  QString m_pendingChangeText;
};

#endif
