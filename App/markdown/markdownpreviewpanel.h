#ifndef MARKDOWNPREVIEWPANEL_H
#define MARKDOWNPREVIEWPANEL_H

#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#ifdef HAVE_WEBENGINE
#include <QWebEngineView>
#else
#include <QTextBrowser>
#endif

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

  void setSyncScrollEnabled(bool enabled);
  bool isSyncScrollEnabled() const { return m_syncScrollEnabled; }

  void setSourceScrollRatio(double ratio);

  QString exportToHtml() const;

public slots:
  void updatePreview();

signals:
  void linkClicked(const QString &url);

private:
  void setupUi();
  void setupToolbar();
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();

#ifdef HAVE_WEBENGINE
  QWebEngineView *m_webView;
#else
  QTextBrowser *m_browser;
#endif
  QToolBar *m_toolbar;
  QTimer m_updateTimer;
  QString m_markdown;
  QString m_basePath;
  QString m_filePath;
  QLabel *m_wordCountLabel;
  bool m_syncScrollEnabled;
  int m_zoomLevel;
};

#endif
