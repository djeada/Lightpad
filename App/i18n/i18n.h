#ifndef I18N_H
#define I18N_H

#include <QObject>
#include <QString>
#include <QLocale>
#include <QTranslator>
#include <QCoreApplication>
#include <QMap>

/**
 * @brief Internationalization manager for Lightpad
 * 
 * Provides support for multiple languages using Qt's translation system.
 * Manages loading and switching of translation files.
 */
class I18n : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the I18n manager
     */
    static I18n& instance();

    /**
     * @brief Initialize the i18n system
     * @param app Application to install translator on
     */
    void initialize(QCoreApplication* app);

    /**
     * @brief Get current language code
     * @return Current language code (e.g., "en", "de", "fr")
     */
    QString currentLanguage() const;

    /**
     * @brief Set the current language
     * @param languageCode Language code to switch to
     * @return true if language was changed successfully
     */
    bool setLanguage(const QString& languageCode);

    /**
     * @brief Get list of available languages
     * @return Map of language code to display name
     */
    QMap<QString, QString> availableLanguages() const;

    /**
     * @brief Get the system's default language
     * @return System language code
     */
    QString systemLanguage() const;

    /**
     * @brief Check if a language is available
     * @param languageCode Language code to check
     * @return true if language is available
     */
    bool isLanguageAvailable(const QString& languageCode) const;

    /**
     * @brief Get directory where translations are stored
     * @return Path to translations directory
     */
    QString translationsDirectory() const;

signals:
    /**
     * @brief Emitted when the language changes
     * @param newLanguage New language code
     */
    void languageChanged(const QString& newLanguage);

private:
    I18n();
    ~I18n();
    I18n(const I18n&) = delete;
    I18n& operator=(const I18n&) = delete;

    void loadAvailableLanguages();
    bool loadTranslation(const QString& languageCode);

    QCoreApplication* m_app;
    QTranslator* m_translator;
    QTranslator* m_qtTranslator;
    QString m_currentLanguage;
    QMap<QString, QString> m_availableLanguages;
};

/**
 * @brief Convenience macro for translation
 * 
 * Usage: TR("Hello World")
 */
#define TR(text) QCoreApplication::translate("Lightpad", text)

/**
 * @brief Convenience macro for translation with context
 * 
 * Usage: TRC("MainWindow", "File")
 */
#define TRC(context, text) QCoreApplication::translate(context, text)

#endif // I18N_H
