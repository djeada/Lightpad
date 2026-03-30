#ifndef MARKDOWNPREVIEWPANEL_H
#define MARKDOWNPREVIEWPANEL_H

#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

class MarkdownPreviewPanel : public QWidget {
  Q_OBJECT

public:
  explicit MarkdownPreviewPanel(QWidget *parent = nullptr);
  ~MarkdownPreviewPanel() = default;

  void setMarkdown(const QString &markdown);
  void setBasePath(const QString &basePath);
  void setFilePath(const QString &filePath);

  QString filePath() const { return m_filePath; }
  QString basePath() const { return m_basePath; }

  static bool isMarkdownFile(const QString &extension);

public slots:
  void updatePreview();

signals:
  void linkClicked(const QString &url);

private:
  void setupUi();
  void setupToolbar();
  void onAnchorClicked(const QUrl &url);

  QTextBrowser *m_browser;
  QToolBar *m_toolbar;
  QTimer m_updateTimer;
  QString m_markdown;
  QString m_basePath;
  QString m_filePath;
};

#endif
