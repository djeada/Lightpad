#include "markdownpreviewpanel.h"
#include "../core/logging/logger.h"
#include "markdowntools.h"
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollBar>
#include <QUrl>

#ifdef HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineSettings>
#endif

MarkdownPreviewPanel::MarkdownPreviewPanel(QWidget *parent)
    : QWidget(parent),
#ifdef HAVE_WEBENGINE
      m_webView(nullptr),
#else
      m_browser(nullptr),
#endif
      m_wordCountLabel(nullptr), m_syncScrollEnabled(false), m_zoomLevel(100) {
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

#ifdef HAVE_WEBENGINE
  m_webView = new QWebEngineView(this);
  m_webView->setObjectName("markdownPreviewWebView");
  m_webView->page()->setBackgroundColor(QColor("#0d1117"));
  m_webView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  m_webView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessFileUrls, true);

  connect(m_webView->page(), &QWebEnginePage::linkHovered, this,
          [](const QString &) {});
  connect(m_webView, &QWebEngineView::urlChanged, this,
          [this](const QUrl &url) {
            if (url.scheme() == "http" || url.scheme() == "https") {
              m_webView->stop();
              QDesktopServices::openUrl(url);
            } else if (url.scheme() == "file" && !url.toLocalFile().isEmpty()) {
              QString localPath = url.toLocalFile();
              QFileInfo fi(localPath);
              if (fi.exists() && !isMarkdownFile(fi.suffix())) {
                m_webView->stop();
                emit linkClicked(localPath);
              }
            }
          });

  layout->addWidget(m_webView, 1);
#else
  m_browser = new QTextBrowser(this);
  m_browser->setObjectName("markdownPreviewBrowser");
  m_browser->setOpenLinks(false);
  m_browser->setOpenExternalLinks(false);
  m_browser->setReadOnly(true);

  connect(m_browser, &QTextBrowser::anchorClicked, this,
          [this](const QUrl &url) {
            QString urlStr = url.toString();
            if (url.scheme() == "http" || url.scheme() == "https") {
              QDesktopServices::openUrl(url);
            } else if (urlStr.startsWith('#')) {
              m_browser->scrollToAnchor(urlStr.mid(1));
            } else if (!m_basePath.isEmpty()) {
              QString resolved = QDir(m_basePath).absoluteFilePath(urlStr);
              if (QFileInfo::exists(resolved))
                emit linkClicked(resolved);
            } else {
              emit linkClicked(urlStr);
            }
          });

  layout->addWidget(m_browser, 1);
#endif

  m_wordCountLabel = new QLabel(this);
  m_wordCountLabel->setObjectName("markdownWordCountLabel");
  m_wordCountLabel->setStyleSheet(
      "QLabel { padding: 2px 8px; color: #8b949e; font-size: 11px; }");
  layout->addWidget(m_wordCountLabel);

  setLayout(layout);
}

void MarkdownPreviewPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("markdownPreviewToolbar");
  m_toolbar->setMovable(false);
  m_toolbar->setStyleSheet("QToolBar { background: #161b22; border-bottom: 1px "
                           "solid #30363d; spacing: 2px; }"
                           "QToolButton { color: #8b949e; padding: 4px 8px; "
                           "border: none; font-size: 12px; }"
                           "QToolButton:hover { color: #e6edf3; background: "
                           "#1f2937; border-radius: 4px; }");

  QAction *refreshAction = m_toolbar->addAction("⟳ Refresh");
  refreshAction->setToolTip("Refresh Preview");
  connect(refreshAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::updatePreview);

  m_toolbar->addSeparator();

  QAction *zoomInAction = m_toolbar->addAction("+ Zoom In");
  zoomInAction->setToolTip("Zoom In");
  connect(zoomInAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomIn);

  QAction *zoomOutAction = m_toolbar->addAction("− Zoom Out");
  zoomOutAction->setToolTip("Zoom Out");
  connect(zoomOutAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomOut);

  QAction *zoomResetAction = m_toolbar->addAction("1:1 Reset");
  zoomResetAction->setToolTip("Reset Zoom");
  connect(zoomResetAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomReset);

  m_toolbar->addSeparator();

  QAction *exportHtmlAction = m_toolbar->addAction("Export HTML");
  exportHtmlAction->setToolTip("Export to HTML file");
  connect(exportHtmlAction, &QAction::triggered, this, [this]() {
    QString html = exportToHtml();
    if (html.isEmpty())
      return;

    QString suggestedName;
    if (!m_filePath.isEmpty()) {
      QFileInfo fi(m_filePath);
      suggestedName = fi.completeBaseName() + ".html";
    } else {
      suggestedName = "preview.html";
    }

    QString path = QFileDialog::getSaveFileName(
        this, tr("Export HTML"), suggestedName, tr("HTML Files (*.html)"));
    if (path.isEmpty())
      return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      file.write(html.toUtf8());
      file.close();
    }
  });

  QAction *syncScrollAction = m_toolbar->addAction("⇅ Sync Scroll");
  syncScrollAction->setToolTip("Toggle synchronized scrolling");
  syncScrollAction->setCheckable(true);
  syncScrollAction->setChecked(m_syncScrollEnabled);
  connect(syncScrollAction, &QAction::toggled, this,
          &MarkdownPreviewPanel::setSyncScrollEnabled);
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
  static const QStringList markdownExtensions = {"md",  "markdown", "mdown",
                                                 "mkd", "mkdn",     "mdx"};
  return markdownExtensions.contains(extension.toLower());
}

void MarkdownPreviewPanel::updatePreview() {
  if (m_markdown.isEmpty()) {
#ifdef HAVE_WEBENGINE
    m_webView->setHtml("");
#else
    m_browser->clear();
#endif
    if (m_wordCountLabel)
      m_wordCountLabel->setText("");
    return;
  }

  QString html = MarkdownTools::toHtml(m_markdown, m_basePath);

  if (m_zoomLevel != 100) {
    QString zoomStyle =
        QString("<style>body { zoom: %1%; }</style>").arg(m_zoomLevel);
    html.replace("</head>", zoomStyle + "</head>");
  }

#ifdef HAVE_WEBENGINE
  QUrl baseUrl =
      m_basePath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_basePath + "/");
  m_webView->setHtml(html, baseUrl);
#else
  int scrollPos = m_browser->verticalScrollBar()->value();
  if (!m_basePath.isEmpty())
    m_browser->setSearchPaths({m_basePath});
  m_browser->setHtml(html);
  m_browser->verticalScrollBar()->setValue(scrollPos);
#endif

  if (m_wordCountLabel) {
    int words = MarkdownTools::wordCount(m_markdown);
    int readingTime = MarkdownTools::readingTimeMinutes(m_markdown);
    QString text =
        QString("%1 words · %2 min read").arg(words).arg(readingTime);
    m_wordCountLabel->setText(text);
  }
}

void MarkdownPreviewPanel::setSyncScrollEnabled(bool enabled) {
  m_syncScrollEnabled = enabled;
}

void MarkdownPreviewPanel::setSourceScrollRatio(double ratio) {
  if (!m_syncScrollEnabled)
    return;

#ifdef HAVE_WEBENGINE
  if (m_webView) {
    QString js = QString("window.scrollTo(0, document.body.scrollHeight * %1);")
                     .arg(ratio);
    m_webView->page()->runJavaScript(js);
  }
#else
  if (m_browser) {
    QScrollBar *scrollBar = m_browser->verticalScrollBar();
    if (scrollBar)
      scrollBar->setValue(static_cast<int>(ratio * scrollBar->maximum()));
  }
#endif
}

QString MarkdownPreviewPanel::exportToHtml() const {
  if (m_markdown.isEmpty())
    return QString();
  return MarkdownTools::toHtml(m_markdown, m_basePath);
}

void MarkdownPreviewPanel::onZoomIn() {
  m_zoomLevel = qMin(m_zoomLevel + 10, 300);
  updatePreview();
}

void MarkdownPreviewPanel::onZoomOut() {
  m_zoomLevel = qMax(m_zoomLevel - 10, 50);
  updatePreview();
}

void MarkdownPreviewPanel::onZoomReset() {
  m_zoomLevel = 100;
  updatePreview();
}
