#include "markdownpreviewpanel.h"
#include "markdowntools.h"
#include "../core/logging/logger.h"
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>

MarkdownPreviewPanel::MarkdownPreviewPanel(QWidget *parent) : QWidget(parent) {
  setupUi();

  m_updateTimer.setSingleShot(true);
  m_updateTimer.setInterval(300);
  connect(&m_updateTimer, &QTimer::timeout, this,
          &MarkdownPreviewPanel::updatePreview);
}

void MarkdownPreviewPanel::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  setupToolbar();
  layout->addWidget(m_toolbar);

  m_browser = new QTextBrowser(this);
  m_browser->setObjectName("markdownPreviewBrowser");
  m_browser->setOpenLinks(false);
  m_browser->setOpenExternalLinks(false);
  m_browser->setReadOnly(true);

  connect(m_browser, &QTextBrowser::anchorClicked, this,
          &MarkdownPreviewPanel::onAnchorClicked);

  layout->addWidget(m_browser, 1);
  setLayout(layout);
}

void MarkdownPreviewPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("markdownPreviewToolbar");
  m_toolbar->setMovable(false);

  QAction *refreshAction = m_toolbar->addAction("⟳ Refresh");
  refreshAction->setToolTip("Refresh Preview");
  connect(refreshAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::updatePreview);
}

void MarkdownPreviewPanel::setMarkdown(const QString &markdown) {
  m_markdown = markdown;
  m_updateTimer.start();
}

void MarkdownPreviewPanel::setBasePath(const QString &basePath) {
  m_basePath = basePath;
}

void MarkdownPreviewPanel::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  QFileInfo fi(filePath);
  m_basePath = fi.absolutePath();
}

bool MarkdownPreviewPanel::isMarkdownFile(const QString &extension) {
  static const QStringList markdownExtensions = {"md",   "markdown", "mdown",
                                                  "mkd",  "mkdn",    "mdx"};
  return markdownExtensions.contains(extension.toLower());
}

void MarkdownPreviewPanel::updatePreview() {
  if (m_markdown.isEmpty()) {
    m_browser->clear();
    return;
  }

  int scrollPos = m_browser->verticalScrollBar()->value();

  QString html = MarkdownTools::toHtml(m_markdown, m_basePath);

  if (!m_basePath.isEmpty()) {
    m_browser->setSearchPaths({m_basePath});
  }

  m_browser->setHtml(html);

  m_browser->verticalScrollBar()->setValue(scrollPos);
}

void MarkdownPreviewPanel::onAnchorClicked(const QUrl &url) {
  QString urlStr = url.toString();

  if (url.scheme() == "file") {
    QString localPath = url.toLocalFile();
    if (!localPath.isEmpty()) {
      emit linkClicked(localPath);
      return;
    }
  }

  if (urlStr.startsWith('#')) {
    m_browser->scrollToAnchor(urlStr.mid(1));
    return;
  }

  if (url.scheme() == "http" || url.scheme() == "https") {
    QDesktopServices::openUrl(url);
    return;
  }

  if (!m_basePath.isEmpty()) {
    QString resolved = QDir(m_basePath).absoluteFilePath(urlStr);
    if (QFileInfo::exists(resolved)) {
      emit linkClicked(resolved);
      return;
    }
  }

  emit linkClicked(urlStr);
}
