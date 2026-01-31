#ifndef AUTOSAVEMANAGER_H
#define AUTOSAVEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QString>

class MainWindow;

/**
 * @brief Manages automatic saving of modified files
 * 
 * Provides auto-save functionality with configurable delay
 * and file tracking.
 */
class AutoSaveManager : public QObject {
    Q_OBJECT

public:
    explicit AutoSaveManager(MainWindow* mainWindow, QObject* parent = nullptr);
    ~AutoSaveManager();

    /**
     * @brief Enable or disable auto-save
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if auto-save is enabled
     */
    bool isEnabled() const;

    /**
     * @brief Set the auto-save delay in seconds
     */
    void setDelay(int seconds);

    /**
     * @brief Get the auto-save delay in seconds
     */
    int delay() const;

    /**
     * @brief Mark a file as modified (needs saving)
     */
    void markModified(const QString& filePath);

    /**
     * @brief Mark a file as saved (no longer needs saving)
     */
    void markSaved(const QString& filePath);

    /**
     * @brief Save all pending modified files
     */
    void saveAllPending();

    /**
     * @brief Get the number of files pending save
     */
    int pendingCount() const;

signals:
    /**
     * @brief Emitted when auto-save state changes
     */
    void stateChanged();

    /**
     * @brief Emitted when a file is auto-saved
     */
    void fileSaved(const QString& filePath);

    /**
     * @brief Emitted when auto-save encounters an error
     */
    void saveError(const QString& filePath, const QString& error);

private slots:
    void onTimer();

private:
    MainWindow* m_mainWindow;
    QTimer* m_timer;
    QSet<QString> m_pendingFiles;
    bool m_enabled;
    int m_delaySeconds;
    static const int DEFAULT_DELAY_SECONDS = 30;
};

#endif // AUTOSAVEMANAGER_H
