#include "markdownpreviewpanel.h"
#include "../core/logging/logger.h"
#include "../theme/colorcontrast.h"
#include "markdowntools.h"
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollBar>
#include <QUrl>

#ifdef HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineSettings>

#include <functional>

namespace {
class MarkdownPreviewPage : public QWebEnginePage {
public:
  MarkdownPreviewPage(std::function<void(const QUrl &)> linkHandler,
                      QObject *parent)
      : QWebEnginePage(parent), m_linkHandler(std::move(linkHandler)) {}

protected:
  bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                               bool isMainFrame) override {
    if (type == NavigationTypeLinkClicked && isMainFrame) {
      QUrl withoutFragment = url;
      withoutFragment.setFragment(QString());
      QUrl current = this->url();
      current.setFragment(QString());
      if (url.hasFragment() && withoutFragment == current)
        return true;
      if (m_linkHandler)
        m_linkHandler(url);
      return false;
    }
    if (type == NavigationTypeFormSubmitted)
      return false;
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
  }

private:
  std::function<void(const QUrl &)> m_linkHandler;
};
} // namespace
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
  m_webView->setPage(new MarkdownPreviewPage(
      [this](const QUrl &url) {
        if (url.scheme() == "http" || url.scheme() == "https" ||
            url.scheme() == "mailto") {
          QDesktopServices::openUrl(url);
        } else if (url.isLocalFile() && !url.toLocalFile().isEmpty()) {
          const QString localPath = url.toLocalFile();
          if (QFileInfo::exists(localPath))
            emit linkClicked(localPath);
        }
      },
      m_webView));
  m_webView->page()->setBackgroundColor(palette().color(QPalette::Base));
  m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                      false);
  m_webView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
  m_webView->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessFileUrls, true);

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
  layout->addWidget(m_wordCountLabel);

  setLayout(layout);
  applyPaletteStyles();
}

void MarkdownPreviewPanel::applyPaletteStyles() {
  using ColorContrast::ensure;
  using ColorContrast::mix;
  const QPalette pal = palette();
  const QColor base = pal.color(QPalette::Base);
  const QColor text = pal.color(QPalette::Text);
  const QColor bar = pal.color(QPalette::Button);
  const QColor border = pal.color(QPalette::Midlight);
  const QColor hover = mix(bar, text, 0.1);
  const QColor muted = ensure(pal.color(QPalette::PlaceholderText), base,
                              ColorContrast::GlyphRatio);
  const QColor buttonText =
      ensure(mix(text, bar, 0.3), bar, ColorContrast::SecondaryTextRatio);
  const QColor checked = mix(bar, pal.color(QPalette::Highlight), 0.3);

  m_toolbar->setStyleSheet(
      QString("QToolBar { background: %1; border: none; border-bottom: 1px "
              "solid %2; spacing: 2px; }"
              "QToolButton { color: %3; background: transparent; "
              "padding: 4px 8px; border: none; border-radius: 4px; "
              "font-size: 12px; }"
              "QToolButton:hover { color: %4; background: %5; }"
              "QToolButton:checked { color: %6; background: %7; }")
          .arg(bar.name(), border.name(), buttonText.name(),
               ensure(text, hover).name(), hover.name(),
               ensure(text, checked).name(), checked.name()));
  m_wordCountLabel->setStyleSheet(
      QString("QLabel { padding: 2px 8px; color: %1; font-size: 11px; }")
          .arg(muted.name()));
#ifdef HAVE_WEBENGINE
  if (m_webView)
    m_webView->page()->setBackgroundColor(base);
#endif
}

void MarkdownPreviewPanel::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange) {
    applyPaletteStyles();
    if (!m_markdown.isEmpty())
      m_updateTimer.start();
  }
}

void MarkdownPreviewPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("markdownPreviewToolbar");
  m_toolbar->setMovable(false);

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

  QString html = MarkdownTools::toHtml(
      m_markdown, m_basePath, MarkdownPreviewColors::fromPalette(palette()));

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
    m_webView->page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);
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
  return MarkdownTools::toHtml(m_markdown, m_basePath,
                               MarkdownPreviewColors::fromPalette(palette()));
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
