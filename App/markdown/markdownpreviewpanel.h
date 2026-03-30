#ifndef MARKDOWNPREVIEWPANEL_H
#define MARKDOWNPREVIEWPANEL_H

#include <QLabel>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
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

  void setSyncScrollEnabled(bool enabled);
  bool isSyncScrollEnabled() const { return m_syncScrollEnabled; }

  void setSourceScrollRatio(double ratio);

  QString exportToHtml() const;

public slots:
  void updatePreview();

signals:
  void linkClicked(const QString &url);
  void outlineEntryClicked(int lineNumber);

private:
  void setupUi();
  void setupToolbar();
  void setupOutlinePanel();
  void onAnchorClicked(const QUrl &url);
  void updateWordCountLabel();
  void updateOutline();
  void onZoomIn();
  void onZoomOut();
  void onZoomReset();

  QTextBrowser *m_browser;
  QToolBar *m_toolbar;
  QTimer m_updateTimer;
  QString m_markdown;
  QString m_basePath;
  QString m_filePath;
  QLabel *m_wordCountLabel;
  QTreeWidget *m_outlineTree;
  QWidget *m_outlinePanel;
  bool m_syncScrollEnabled;
  int m_zoomLevel;
};

#endif
