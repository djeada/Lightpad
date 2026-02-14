#ifndef AUTOSAVEMANAGER_H
#define AUTOSAVEMANAGER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class MainWindow;

class AutoSaveManager : public QObject {
  Q_OBJECT

public:
  explicit AutoSaveManager(MainWindow *mainWindow, QObject *parent = nullptr);
  ~AutoSaveManager();

  void setEnabled(bool enabled);

  bool isEnabled() const;

  void setDelay(int seconds);

  int delay() const;

  void markModified(const QString &filePath);

  void markSaved(const QString &filePath);

  void saveAllPending();

  int pendingCount() const;

signals:

  void stateChanged();

  void fileSaved(const QString &filePath);

  void saveError(const QString &filePath, const QString &error);

private slots:
  void onTimer();

private:
  MainWindow *m_mainWindow;
  QTimer *m_timer;
  QSet<QString> m_pendingFiles;
  bool m_enabled;
  int m_delaySeconds;
  static const int DEFAULT_DELAY_SECONDS = 30;
};

#endif
