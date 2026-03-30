#ifndef LATEXPREVIEWPANEL_H
#define LATEXPREVIEWPANEL_H

#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

class LatexPreviewPanel : public QWidget {
  Q_OBJECT

public:
  explicit LatexPreviewPanel(QWidget *parent = nullptr);
  ~LatexPreviewPanel();

  void setFilePath(const QString &filePath);
  QString filePath() const { return m_filePath; }
  QString basePath() const { return m_basePath; }

  static bool isLatexFile(const QString &extension);

  void setAutoBuild(bool enabled);
  bool autoBuild() const { return m_autoBuild; }

public slots:
  void build();
  void clean();
  void onSourceSaved();
  void updatePreview();

signals:
  void buildStarted();
  void buildFinished(bool success);
  void diagnosticsReady(const QList<struct LspDiagnostic> &diagnostics);

private:
  void setupUi();
  void setupToolbar();
  void onBuildProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onBuildProcessOutput();
  void appendLog(const QString &text, const QString &color = QString());
  QString selectedEngine() const;
  QStringList engineArgs() const;

  QTextBrowser *m_logBrowser;
  QToolBar *m_toolbar;
  QComboBox *m_engineCombo;
  QLabel *m_statusLabel;
  QProcess *m_buildProcess;
  QTimer m_buildTimer;
  QString m_filePath;
  QString m_basePath;
  QString m_buildLog;
  bool m_autoBuild;
  bool m_building;
};

#endif
