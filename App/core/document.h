#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QDateTime>
#include <QObject>
#include <QString>

/**
 * @brief Represents a document in the editor
 *
 * Separates document data from the view (TextArea).
 * Manages file content, path, and modification state.
 */
class Document : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Document state enumeration
   */
  enum class State {
    New,      // Newly created, never saved
    Saved,    // Content matches file on disk
    Modified, // Has unsaved changes
    Error     // Error state (e.g., file access error)
  };
  Q_ENUM(State)

  explicit Document(QObject *parent = nullptr);
  explicit Document(const QString &filePath, QObject *parent = nullptr);
  ~Document() = default;

  // Content management
  /**
   * @brief Get the document content
   * @return Document content as QString
   */
  QString content() const;

  /**
   * @brief Set the document content
   * @param content New content
   */
  void setContent(const QString &content);

  // File path management
  /**
   * @brief Get the file path
   * @return File path or empty string if new document
   */
  QString filePath() const;

  /**
   * @brief Set the file path
   * @param path New file path
   */
  void setFilePath(const QString &path);

  /**
   * @brief Get just the file name
   * @return File name or "Untitled" for new documents
   */
  QString fileName() const;

  /**
   * @brief Get the file extension
   * @return File extension without dot
   */
  QString fileExtension() const;

  // State management
  /**
   * @brief Get the current document state
   * @return Current state
   */
  State state() const;

  /**
   * @brief Check if document has unsaved changes
   * @return true if modified
   */
  bool isModified() const;

  /**
   * @brief Check if document is new (never saved)
   * @return true if new document
   */
  bool isNew() const;

  /**
   * @brief Mark the document as saved
   */
  void markAsSaved();

  /**
   * @brief Mark the document as modified
   */
  void markAsModified();

  // File operations
  /**
   * @brief Load document from its file path
   * @return true if successful
   */
  bool load();

  /**
   * @brief Save document to its file path
   * @return true if successful
   */
  bool save();

  /**
   * @brief Save document to a new file path
   * @param path New file path
   * @return true if successful
   */
  bool saveAs(const QString &path);

  // Metadata
  /**
   * @brief Get the last modified timestamp
   * @return Last modification time
   */
  QDateTime lastModified() const;

  /**
   * @brief Get the language hint based on file extension
   * @return Language identifier (e.g., "cpp", "py", "js")
   */
  QString languageHint() const;

signals:
  /**
   * @brief Emitted when the content changes
   */
  void contentChanged();

  /**
   * @brief Emitted when the modification state changes
   * @param modified Whether the document is now modified
   */
  void modificationStateChanged(bool modified);

  /**
   * @brief Emitted when the file path changes
   * @param path New file path
   */
  void filePathChanged(const QString &path);

  /**
   * @brief Emitted when document is saved
   */
  void saved();

  /**
   * @brief Emitted when document is loaded
   */
  void loaded();

  /**
   * @brief Emitted on file operation error
   * @param error Error message
   */
  void error(const QString &error);

private:
  QString m_content;
  QString m_filePath;
  State m_state;
  QDateTime m_lastModified;

  void updateState(State newState);
  QString detectLanguage() const;
};

#endif // DOCUMENT_H
