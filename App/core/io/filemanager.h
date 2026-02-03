#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

/**
 * @brief Manages file operations for the editor
 *
 * Provides a centralized interface for file I/O operations,
 * abstracting file handling from the UI layer.
 */
class FileManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Result of a file operation
   */
  struct FileResult {
    bool success;
    QString content;
    QString errorMessage;
  };

  /**
   * @brief Get the singleton instance
   * @return Reference to the FileManager instance
   */
  static FileManager &instance();

  /**
   * @brief Read the contents of a file
   * @param filePath Path to the file
   * @return FileResult containing success status and content or error
   */
  FileResult readFile(const QString &filePath);

  /**
   * @brief Write content to a file
   * @param filePath Path to the file
   * @param content Content to write
   * @return FileResult containing success status or error
   */
  FileResult writeFile(const QString &filePath, const QString &content);

  /**
   * @brief Check if a file exists
   * @param filePath Path to check
   * @return true if the file exists
   */
  bool fileExists(const QString &filePath);

  /**
   * @brief Get file extension
   * @param filePath Path to the file
   * @return File extension without the dot
   */
  QString getFileExtension(const QString &filePath);

  /**
   * @brief Get file name from path
   * @param filePath Full file path
   * @return Just the file name
   */
  QString getFileName(const QString &filePath);

  /**
   * @brief Get directory from file path
   * @param filePath Full file path
   * @return Directory path
   */
  QString getDirectory(const QString &filePath);

signals:
  /**
   * @brief Emitted when a file is successfully opened
   * @param filePath Path to the opened file
   */
  void fileOpened(const QString &filePath);

  /**
   * @brief Emitted when a file is successfully saved
   * @param filePath Path to the saved file
   */
  void fileSaved(const QString &filePath);

  /**
   * @brief Emitted when a file operation fails
   * @param filePath Path to the file
   * @param error Error message
   */
  void fileError(const QString &filePath, const QString &error);

private:
  FileManager();
  ~FileManager() = default;
  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;
};

#endif // FILEMANAGER_H
