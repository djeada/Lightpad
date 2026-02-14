#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QDateTime>
#include <QObject>
#include <QString>

class Document : public QObject {
  Q_OBJECT

public:
  enum class State { New, Saved, Modified, Error };
  Q_ENUM(State)

  explicit Document(QObject *parent = nullptr);
  explicit Document(const QString &filePath, QObject *parent = nullptr);
  ~Document() = default;

  QString content() const;

  void setContent(const QString &content);

  QString filePath() const;

  void setFilePath(const QString &path);

  QString fileName() const;

  QString fileExtension() const;

  State state() const;

  bool isModified() const;

  bool isNew() const;

  void markAsSaved();

  void markAsModified();

  bool load();

  bool save();

  bool saveAs(const QString &path);

  QDateTime lastModified() const;

  QString languageHint() const;

signals:

  void contentChanged();

  void modificationStateChanged(bool modified);

  void filePathChanged(const QString &path);

  void saved();

  void loaded();

  void error(const QString &error);

private:
  QString m_content;
  QString m_filePath;
  State m_state;
  QDateTime m_lastModified;

  void updateState(State newState);
  QString detectLanguage() const;
};

#endif
