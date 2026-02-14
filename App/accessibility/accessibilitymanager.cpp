#include "accessibilitymanager.h"
#include "../core/logging/logger.h"
#include "../settings/settingsmanager.h"

#include <QAccessible>
#include <QApplication>
#include <QPalette>
#include <QStyle>

AccessibilityManager &AccessibilityManager::instance() {
  static AccessibilityManager instance;
  return instance;
}

AccessibilityManager::AccessibilityManager()
    : QObject(nullptr), m_currentProfile(Profile::Default),
      m_highContrastEnabled(false), m_fontScale(1.0),
      m_reducedMotionEnabled(false), m_screenReaderEnabled(false) {}

AccessibilityManager::~AccessibilityManager() {}

void AccessibilityManager::initialize() {
  loadSettings();

  if (m_highContrastEnabled) {
    applyHighContrast();
  }

  LOG_INFO(QString("Accessibility initialized: profile=%1, fontScale=%2")
               .arg(static_cast<int>(m_currentProfile))
               .arg(m_fontScale));
}

void AccessibilityManager::applyProfile(Profile profile) {
  m_currentProfile = profile;

  switch (profile) {
  case Profile::Default:
    setHighContrastEnabled(false);
    setFontScale(1.0);
    setReducedMotionEnabled(false);
    setScreenReaderEnabled(false);
    break;

  case Profile::HighContrast:
    setHighContrastEnabled(true);
    setFontScale(1.0);
    setReducedMotionEnabled(false);
    setScreenReaderEnabled(false);
    break;

  case Profile::LargeText:
    setHighContrastEnabled(false);
    setFontScale(1.5);
    setReducedMotionEnabled(false);
    setScreenReaderEnabled(false);
    break;

  case Profile::ReducedMotion:
    setHighContrastEnabled(false);
    setFontScale(1.0);
    setReducedMotionEnabled(true);
    setScreenReaderEnabled(false);
    break;

  case Profile::ScreenReader:
    setHighContrastEnabled(true);
    setFontScale(1.2);
    setReducedMotionEnabled(true);
    setScreenReaderEnabled(true);
    break;
  }

  saveSettings();
  emit profileChanged(profile);
  LOG_INFO(QString("Applied accessibility profile: %1")
               .arg(static_cast<int>(profile)));
}

AccessibilityManager::Profile AccessibilityManager::currentProfile() const {
  return m_currentProfile;
}

void AccessibilityManager::setHighContrastEnabled(bool enabled) {
  if (m_highContrastEnabled != enabled) {
    m_highContrastEnabled = enabled;

    if (enabled) {
      applyHighContrast();
    } else {
      applyNormalContrast();
    }

    saveSettings();
    emit highContrastChanged(enabled);
    LOG_DEBUG(
        QString("High contrast: %1").arg(enabled ? "enabled" : "disabled"));
  }
}

bool AccessibilityManager::isHighContrastEnabled() const {
  return m_highContrastEnabled;
}

void AccessibilityManager::setFontScale(double scale) {

  scale = qBound(0.5, scale, 3.0);

  if (qAbs(m_fontScale - scale) > 0.01) {
    m_fontScale = scale;

    QFont font = QApplication::font();
    int baseSize = 10;
    font.setPointSize(static_cast<int>(baseSize * scale));
    QApplication::setFont(font);

    saveSettings();
    emit fontScaleChanged(scale);
    LOG_DEBUG(QString("Font scale set to: %1").arg(scale));
  }
}

double AccessibilityManager::fontScale() const { return m_fontScale; }

QFont AccessibilityManager::scaledFont(const QFont &baseFont) const {
  QFont scaled = baseFont;
  int newSize = static_cast<int>(baseFont.pointSize() * m_fontScale);
  scaled.setPointSize(qMax(6, newSize));
  return scaled;
}

void AccessibilityManager::increaseFontScale() {
  setFontScale(m_fontScale + 0.1);
}

void AccessibilityManager::decreaseFontScale() {
  setFontScale(m_fontScale - 0.1);
}

void AccessibilityManager::resetFontScale() { setFontScale(1.0); }

void AccessibilityManager::setReducedMotionEnabled(bool enabled) {
  if (m_reducedMotionEnabled != enabled) {
    m_reducedMotionEnabled = enabled;

    saveSettings();
    emit reducedMotionChanged(enabled);
    LOG_DEBUG(
        QString("Reduced motion: %1").arg(enabled ? "enabled" : "disabled"));
  }
}

bool AccessibilityManager::isReducedMotionEnabled() const {
  return m_reducedMotionEnabled;
}

void AccessibilityManager::setScreenReaderEnabled(bool enabled) {
  if (m_screenReaderEnabled != enabled) {
    m_screenReaderEnabled = enabled;

    if (enabled) {

      QAccessible::setActive(true);
    }

    saveSettings();
    emit screenReaderChanged(enabled);
    LOG_DEBUG(QString("Screen reader mode: %1")
                  .arg(enabled ? "enabled" : "disabled"));
  }
}

bool AccessibilityManager::isScreenReaderEnabled() const {
  return m_screenReaderEnabled;
}

void AccessibilityManager::announce(const QString &text, bool priority) {
  Q_UNUSED(priority);

  if (m_screenReaderEnabled && !text.isEmpty()) {

    QAccessibleEvent event(QApplication::focusWidget(), QAccessible::Alert);
    QAccessible::updateAccessibility(&event);

    LOG_DEBUG(QString("Announced: %1").arg(text));
  }
}

void AccessibilityManager::setAccessibleProperties(QWidget *widget,
                                                   const QString &name,
                                                   const QString &description) {
  if (widget) {
    widget->setAccessibleName(name);
    if (!description.isEmpty()) {
      widget->setAccessibleDescription(description);
    }
  }
}

void AccessibilityManager::ensureKeyboardAccessible(QWidget *widget) {
  if (widget) {
    widget->setFocusPolicy(Qt::StrongFocus);
  }
}

void AccessibilityManager::loadSettings() {
  SettingsManager &settings = SettingsManager::instance();

  m_highContrastEnabled =
      settings.getValue("accessibility/highContrast", false).toBool();
  m_fontScale = settings.getValue("accessibility/fontScale", 1.0).toDouble();
  m_reducedMotionEnabled =
      settings.getValue("accessibility/reducedMotion", false).toBool();
  m_screenReaderEnabled =
      settings.getValue("accessibility/screenReader", false).toBool();
  m_currentProfile = static_cast<Profile>(
      settings.getValue("accessibility/profile", 0).toInt());
}

void AccessibilityManager::saveSettings() {
  SettingsManager &settings = SettingsManager::instance();

  settings.setValue("accessibility/highContrast", m_highContrastEnabled);
  settings.setValue("accessibility/fontScale", m_fontScale);
  settings.setValue("accessibility/reducedMotion", m_reducedMotionEnabled);
  settings.setValue("accessibility/screenReader", m_screenReaderEnabled);
  settings.setValue("accessibility/profile",
                    static_cast<int>(m_currentProfile));
}

void AccessibilityManager::applyHighContrast() {
  QPalette palette;

  palette.setColor(QPalette::Window, Qt::black);
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Base, Qt::black);
  palette.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
  palette.setColor(QPalette::ToolTipBase, Qt::black);
  palette.setColor(QPalette::ToolTipText, Qt::white);
  palette.setColor(QPalette::Text, Qt::white);
  palette.setColor(QPalette::Button, QColor(30, 30, 30));
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::BrightText, Qt::yellow);
  palette.setColor(QPalette::Link, QColor(100, 200, 255));
  palette.setColor(QPalette::Highlight, QColor(0, 100, 200));
  palette.setColor(QPalette::HighlightedText, Qt::white);

  palette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::gray);
  palette.setColor(QPalette::Disabled, QPalette::Text, Qt::gray);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::gray);

  QApplication::setPalette(palette);
  LOG_DEBUG("Applied high contrast palette");
}

void AccessibilityManager::applyNormalContrast() {

  QApplication::setPalette(QApplication::style()->standardPalette());
  LOG_DEBUG("Restored normal contrast palette");
}
