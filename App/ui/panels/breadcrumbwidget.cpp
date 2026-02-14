#include "breadcrumbwidget.h"
#include "../uistylehelper.h"
#include <QDir>
#include <QFileInfo>

BreadcrumbWidget::BreadcrumbWidget(QWidget *parent)
    : QWidget(parent), m_layout(nullptr) {
  setupUI();
}

BreadcrumbWidget::~BreadcrumbWidget() {}

void BreadcrumbWidget::setupUI() {
  m_layout = new QHBoxLayout(this);
  m_layout->setContentsMargins(8, 4, 8, 4);
  m_layout->setSpacing(2);

  setStyleSheet("BreadcrumbWidget {"
                "  background: #171c24;"
                "  border-bottom: 1px solid #2a3241;"
                "}");

  m_layout->addStretch();
}

void BreadcrumbWidget::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  rebuildBreadcrumbs();
}

void BreadcrumbWidget::setProjectRoot(const QString &rootPath) {
  m_projectRoot = rootPath;
  rebuildBreadcrumbs();
}

void BreadcrumbWidget::clear() {
  m_filePath.clear();

  for (QPushButton *btn : m_segments) {
    m_layout->removeWidget(btn);
    delete btn;
  }
  m_segments.clear();

  for (QLabel *sep : m_separators) {
    m_layout->removeWidget(sep);
    delete sep;
  }
  m_separators.clear();
}

void BreadcrumbWidget::rebuildBreadcrumbs() {
  clear();

  if (m_filePath.isEmpty()) {
    return;
  }

  QString displayPath = m_filePath;
  if (!m_projectRoot.isEmpty() && m_filePath.startsWith(m_projectRoot)) {
    displayPath = m_filePath.mid(m_projectRoot.length());
    if (displayPath.startsWith('/') || displayPath.startsWith('\\')) {
      displayPath = displayPath.mid(1);
    }
  }

  QStringList segments = getPathSegments(displayPath);

  const QString buttonStyle = UIStyleHelper::breadcrumbButtonStyle(m_theme);
  const QString separatorStyle =
      UIStyleHelper::breadcrumbSeparatorStyle(m_theme);

  if (m_layout->count() > 0) {
    QLayoutItem *stretchItem = m_layout->itemAt(m_layout->count() - 1);
    if (stretchItem) {
      m_layout->removeItem(stretchItem);
      delete stretchItem;
    }
  }

  for (int i = 0; i < segments.size(); ++i) {

    QPushButton *btn = new QPushButton(segments[i], this);
    btn->setStyleSheet(buttonStyle);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("segmentIndex", i);

    connect(btn, &QPushButton::clicked, this,
            &BreadcrumbWidget::onSegmentClicked);

    m_layout->addWidget(btn);
    m_segments.append(btn);

    if (i < segments.size() - 1) {
      QLabel *sep = new QLabel("\u203A", this);
      sep->setStyleSheet(separatorStyle);
      m_layout->addWidget(sep);
      m_separators.append(sep);
    }
  }

  if (!m_segments.isEmpty()) {
    m_segments.last()->setStyleSheet(
        UIStyleHelper::breadcrumbActiveButtonStyle(m_theme));
  }

  m_layout->addStretch();
}

QStringList BreadcrumbWidget::getPathSegments(const QString &path) const {

  QString normalizedPath = path;
  normalizedPath.replace('\\', '/');
  return normalizedPath.split('/', Qt::SkipEmptyParts);
}

QString BreadcrumbWidget::buildPathUpTo(int segmentIndex) const {
  if (m_filePath.isEmpty() || segmentIndex < 0) {
    return QString();
  }

  QString basePath = m_projectRoot.isEmpty() ? QString() : m_projectRoot;

  QString displayPath = m_filePath;
  if (!m_projectRoot.isEmpty() && m_filePath.startsWith(m_projectRoot)) {
    displayPath = m_filePath.mid(m_projectRoot.length());
    if (displayPath.startsWith('/') || displayPath.startsWith('\\')) {
      displayPath = displayPath.mid(1);
    }
  }

  QStringList segments = getPathSegments(displayPath);

  QString result = basePath;
  for (int i = 0; i <= segmentIndex && i < segments.size(); ++i) {
    if (!result.isEmpty() && !result.endsWith('/') && !result.endsWith('\\')) {
      result += '/';
    }
    result += segments[i];
  }

  return result;
}

void BreadcrumbWidget::onSegmentClicked() {
  QPushButton *btn = qobject_cast<QPushButton *>(sender());
  if (!btn) {
    return;
  }

  int segmentIndex = btn->property("segmentIndex").toInt();
  QString path = buildPathUpTo(segmentIndex);

  if (!path.isEmpty()) {
    emit pathSegmentClicked(path);
  }
}

void BreadcrumbWidget::onDropdownClicked() {}

void BreadcrumbWidget::applyTheme(const Theme &theme) {
  m_theme = theme;
  setStyleSheet(QString("BreadcrumbWidget { %1 }")
                    .arg(UIStyleHelper::panelHeaderStyle(theme)));
  rebuildBreadcrumbs();
}
