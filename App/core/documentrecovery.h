#ifndef DOCUMENTRECOVERY_H
#define DOCUMENTRECOVERY_H

#include "io/recoverystore.h"
#include <QHash>
#include <QObject>
#include <QPointer>
#include <memory>

class QPlainTextEdit;
class QTimer;
class QTextDocument;

class DocumentRecovery : public QObject {
  Q_OBJECT
public:
  explicit DocumentRecovery(QObject *parent = nullptr,
                            const QString &rootPath = {});
  ~DocumentRecovery() override;
  void track(QPlainTextEdit *editor, const QString &path, const QString &title,
             const FileOpenGuard::TextFormat &format = {});
  void forget(QPlainTextEdit *editor);
  Recovery::Scan recoverable() const;
  bool adopt(const Recovery::Draft &draft, QPlainTextEdit *editor,
             QString *error = nullptr);
  bool discard(const Recovery::Draft &draft, QString *error = nullptr);
  bool finishSession(QString *error = nullptr);
  QString initializationError() const;
  QString sessionId() const;

public slots:
  void checkpoint();

signals:
  void checkpointed(const QString &documentId);
  void recoveryError(const QString &message);

private:
  struct State;
  struct Version;
  struct Document {
    QPointer<QPlainTextEdit> editor;
    Recovery::Draft draft;
    std::shared_ptr<Version> version;
    QString lastError;
    bool pending = false, hasSnapshot = false;
  };
  std::shared_ptr<State> m_state;
  QHash<QPlainTextEdit *, Document> m_documents;
  QTimer *m_timer;
  QString m_initializationError;
  bool m_checkpointRunning = false;
  void changed(QPlainTextEdit *editor);
  void clear(QPlainTextEdit *editor);
  void removeSnapshot(const Recovery::Draft &draft,
                      const std::shared_ptr<Version> &version);
  void updateTimer();
};
#endif
