#include "notificationwidget.h"
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

NotificationWidget::NotificationWidget(QWidget *parent)
    : QFrame(parent), m_iconLabel(nullptr), m_titleLabel(nullptr),
      m_messageLabel(nullptr), m_closeButton(nullptr), m_dismissTimer(nullptr),
      m_fadeAnimation(nullptr) {
  setupUi();
  setVisible(false);
}

void NotificationWidget::setupUi() {
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
  setMinimumWidth(320);

  auto *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(12, 10, 8, 10);
  mainLayout->setSpacing(10);

  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(20, 20);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  m_iconLabel->setStyleSheet("font-size: 16px; background: transparent;");
  mainLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);

  auto *textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(2);

  m_titleLabel = new QLabel(this);
  m_titleLabel->setStyleSheet(
      "font-weight: 600; font-size: 12px; background: transparent;");
  m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_titleLabel->setWordWrap(true);
  textLayout->addWidget(m_titleLabel);

  m_messageLabel = new QLabel(this);
  m_messageLabel->setStyleSheet("font-size: 11px; background: transparent;");
  m_messageLabel->setSizePolicy(QSizePolicy::Expanding,
                                QSizePolicy::Preferred);
  m_messageLabel->setWordWrap(true);
  textLayout->addWidget(m_messageLabel);

  mainLayout->addLayout(textLayout, 1);

  m_closeButton = new QPushButton("✕", this);
  m_closeButton->setFixedSize(20, 20);
  m_closeButton->setCursor(Qt::PointingHandCursor);
  m_closeButton->setStyleSheet(
      "QPushButton {"
      "  background: transparent;"
      "  border: none;"
      "  color: #8b949e;"
      "  font-size: 12px;"
      "  padding: 0;"
      "}"
      "QPushButton:hover {"
      "  color: #e6edf3;"
      "}");
  connect(m_closeButton, &QPushButton::clicked, this,
          &NotificationWidget::dismiss);
  mainLayout->addWidget(m_closeButton, 0, Qt::AlignTop);

  m_dismissTimer = new QTimer(this);
  m_dismissTimer->setSingleShot(true);
  connect(m_dismissTimer, &QTimer::timeout, this,
          &NotificationWidget::dismiss);

  auto *opacityEffect = new QGraphicsOpacityEffect(this);
  opacityEffect->setOpacity(1.0);
  setGraphicsEffect(opacityEffect);

  m_fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
  m_fadeAnimation->setDuration(300);
  connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
    if (m_fadeAnimation->direction() == QAbstractAnimation::Backward ||
        m_fadeAnimation->endValue().toDouble() < 0.1) {
      setVisible(false);
      deleteLater();
    }
  });
}

void NotificationWidget::showNotification(const QString &title,
                                          const QString &message, Level level,
                                          int durationMs) {
  m_titleLabel->setText(title);
  m_messageLabel->setText(message);
  m_messageLabel->setVisible(!message.isEmpty());
  applyStyle(level);
  positionInParent();

  setVisible(true);
  raise();

  auto *effect = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
  if (effect) {
    effect->setOpacity(0.0);
  }

  m_fadeAnimation->stop();
  m_fadeAnimation->setStartValue(0.0);
  m_fadeAnimation->setEndValue(1.0);
  m_fadeAnimation->setDirection(QAbstractAnimation::Forward);
  m_fadeAnimation->start();

  if (durationMs > 0) {
    m_dismissTimer->start(durationMs);
  }
}

void NotificationWidget::dismiss() {
  m_dismissTimer->stop();
  m_fadeAnimation->stop();
  m_fadeAnimation->setStartValue(1.0);
  m_fadeAnimation->setEndValue(0.0);
  m_fadeAnimation->setDirection(QAbstractAnimation::Forward);
  m_fadeAnimation->start();
}

void NotificationWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QPainterPath path;
  path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
  painter.fillPath(path, QColor("#1c2128"));
  painter.setPen(QPen(QColor("#30363d"), 1));
  painter.drawPath(path);
}

void NotificationWidget::applyStyle(Level level) {
  QString accentColor;
  QString icon;

  switch (level) {
  case Level::Info:
    accentColor = "#58a6ff";
    icon = "ℹ";
    break;
  case Level::Warning:
    accentColor = "#d29922";
    icon = "⚠";
    break;
  case Level::Error:
    accentColor = "#f85149";
    icon = "✖";
    break;
  }

  m_iconLabel->setText(icon);
  m_iconLabel->setStyleSheet(
      QString("font-size: 16px; color: %1; background: transparent;")
          .arg(accentColor));
  m_titleLabel->setStyleSheet(
      QString(
          "font-weight: 600; font-size: 12px; color: #e6edf3; background: "
          "transparent;"));
  m_messageLabel->setStyleSheet(
      "font-size: 11px; color: #8b949e; background: transparent;");

  setStyleSheet(QString("NotificationWidget {"
                        "  border-left: 3px solid %1;"
                        "}")
                    .arg(accentColor));
}

void NotificationWidget::positionInParent() {
  if (!parentWidget()) {
    return;
  }

  QWidget *parent = parentWidget();
  updateSizeForContent();

  int x = qMax(16, parent->width() - width() - 16);
  int y = qMax(16, parent->height() - height() - 50);
  move(x, y);
}

void NotificationWidget::updateSizeForContent() {
  const QWidget *parent = parentWidget();
  const int notificationWidth =
      parent ? qBound(280, parent->width() - 32, 520) : 380;

  setFixedWidth(notificationWidth);
  updateGeometry();
  layout()->activate();
  adjustSize();
}

// NotificationManager

NotificationManager::NotificationManager(QWidget *parentWidget,
                                         QObject *parent)
    : QObject(parent), m_parentWidget(parentWidget) {}

void NotificationManager::showInfo(const QString &title, const QString &message,
                                   int durationMs) {
  auto *notification = new NotificationWidget(m_parentWidget);
  notification->showNotification(title, message, NotificationWidget::Level::Info,
                                 durationMs);
}

void NotificationManager::showWarning(const QString &title,
                                      const QString &message, int durationMs) {
  auto *notification = new NotificationWidget(m_parentWidget);
  notification->showNotification(title, message,
                                 NotificationWidget::Level::Warning, durationMs);
}

void NotificationManager::showError(const QString &title,
                                    const QString &message, int durationMs) {
  auto *notification = new NotificationWidget(m_parentWidget);
  notification->showNotification(title, message,
                                 NotificationWidget::Level::Error, durationMs);
}
