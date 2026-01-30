#ifndef ACCESSIBILITYMANAGER_H
#define ACCESSIBILITYMANAGER_H

#include <QObject>
#include <QString>
#include <QWidget>
#include <QFont>

/**
 * @brief Manages accessibility features for Lightpad
 * 
 * Provides support for:
 * - Screen reader compatibility
 * - High contrast themes
 * - Font scaling
 * - Keyboard navigation
 */
class AccessibilityManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Accessibility profile presets
     */
    enum class Profile {
        Default,        // Normal settings
        HighContrast,   // High contrast colors
        LargeText,      // Larger font sizes
        ReducedMotion,  // Minimal animations
        ScreenReader    // Optimized for screen readers
    };
    Q_ENUM(Profile)

    /**
     * @brief Get the singleton instance
     * @return Reference to the AccessibilityManager
     */
    static AccessibilityManager& instance();

    /**
     * @brief Initialize accessibility features
     */
    void initialize();

    /**
     * @brief Apply an accessibility profile
     * @param profile Profile to apply
     */
    void applyProfile(Profile profile);

    /**
     * @brief Get current profile
     * @return Current accessibility profile
     */
    Profile currentProfile() const;

    // High Contrast

    /**
     * @brief Enable or disable high contrast mode
     * @param enabled Whether to enable high contrast
     */
    void setHighContrastEnabled(bool enabled);

    /**
     * @brief Check if high contrast mode is enabled
     * @return true if high contrast is enabled
     */
    bool isHighContrastEnabled() const;

    // Font Scaling

    /**
     * @brief Set the font scale factor
     * @param scale Scale factor (1.0 = normal, 2.0 = double size)
     */
    void setFontScale(double scale);

    /**
     * @brief Get current font scale factor
     * @return Current scale factor
     */
    double fontScale() const;

    /**
     * @brief Get scaled font based on base font
     * @param baseFont Base font to scale
     * @return Scaled font
     */
    QFont scaledFont(const QFont& baseFont) const;

    /**
     * @brief Increase font scale
     */
    void increaseFontScale();

    /**
     * @brief Decrease font scale
     */
    void decreaseFontScale();

    /**
     * @brief Reset font scale to default
     */
    void resetFontScale();

    // Reduced Motion

    /**
     * @brief Enable or disable reduced motion
     * @param enabled Whether to reduce animations
     */
    void setReducedMotionEnabled(bool enabled);

    /**
     * @brief Check if reduced motion is enabled
     * @return true if reduced motion is enabled
     */
    bool isReducedMotionEnabled() const;

    // Screen Reader

    /**
     * @brief Enable or disable screen reader optimizations
     * @param enabled Whether to enable screen reader mode
     */
    void setScreenReaderEnabled(bool enabled);

    /**
     * @brief Check if screen reader mode is enabled
     * @return true if screen reader mode is enabled
     */
    bool isScreenReaderEnabled() const;

    /**
     * @brief Announce text to screen reader
     * @param text Text to announce
     * @param priority Priority (true for immediate)
     */
    void announce(const QString& text, bool priority = false);

    // Widget Accessibility

    /**
     * @brief Set accessibility properties on a widget
     * @param widget Widget to configure
     * @param name Accessible name
     * @param description Accessible description
     */
    void setAccessibleProperties(QWidget* widget, 
                                  const QString& name,
                                  const QString& description = QString());

    /**
     * @brief Ensure widget is keyboard accessible
     * @param widget Widget to configure
     */
    void ensureKeyboardAccessible(QWidget* widget);

signals:
    /**
     * @brief Emitted when profile changes
     * @param profile New profile
     */
    void profileChanged(Profile profile);

    /**
     * @brief Emitted when high contrast setting changes
     * @param enabled New state
     */
    void highContrastChanged(bool enabled);

    /**
     * @brief Emitted when font scale changes
     * @param scale New scale factor
     */
    void fontScaleChanged(double scale);

    /**
     * @brief Emitted when reduced motion setting changes
     * @param enabled New state
     */
    void reducedMotionChanged(bool enabled);

    /**
     * @brief Emitted when screen reader setting changes
     * @param enabled New state
     */
    void screenReaderChanged(bool enabled);

private:
    AccessibilityManager();
    ~AccessibilityManager();
    AccessibilityManager(const AccessibilityManager&) = delete;
    AccessibilityManager& operator=(const AccessibilityManager&) = delete;

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

#endif // ACCESSIBILITYMANAGER_H
