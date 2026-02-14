#ifndef ACCESSIBILITYMANAGER_H
#define ACCESSIBILITYMANAGER_H

#include <QFont>
#include <QObject>
#include <QString>
#include <QWidget>

class AccessibilityManager : public QObject {
  Q_OBJECT

public:
  enum class Profile {
    Default,
    HighContrast,
    LargeText,
    ReducedMotion,
    ScreenReader
  };
  Q_ENUM(Profile)

  static AccessibilityManager &instance();

  void initialize();

  void applyProfile(Profile profile);

  Profile currentProfile() const;

  void setHighContrastEnabled(bool enabled);

  bool isHighContrastEnabled() const;

  void setFontScale(double scale);

  double fontScale() const;

  QFont scaledFont(const QFont &baseFont) const;

  void increaseFontScale();

  void decreaseFontScale();

  void resetFontScale();

  void setReducedMotionEnabled(bool enabled);

  bool isReducedMotionEnabled() const;

  void setScreenReaderEnabled(bool enabled);

  bool isScreenReaderEnabled() const;

  void announce(const QString &text, bool priority = false);

  void setAccessibleProperties(QWidget *widget, const QString &name,
                               const QString &description = QString());

  void ensureKeyboardAccessible(QWidget *widget);

signals:

  void profileChanged(Profile profile);

  void highContrastChanged(bool enabled);

  void fontScaleChanged(double scale);

  void reducedMotionChanged(bool enabled);

  void screenReaderChanged(bool enabled);

private:
  AccessibilityManager();
  ~AccessibilityManager();
  AccessibilityManager(const AccessibilityManager &) = delete;
  AccessibilityManager &operator=(const AccessibilityManager &) = delete;

  void loadSettings();
  void saveSettings();
  void applyHighContrast();
  void applyNormalContrast();

  Profile m_currentProfile;
  bool m_highContrastEnabled;
  double m_fontScale;
  bool m_reducedMotionEnabled;
  bool m_screenReaderEnabled;
};

#endif
