#ifndef I18N_H
#define I18N_H

#include <QCoreApplication>
#include <QLocale>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTranslator>

class I18n : public QObject {
  Q_OBJECT

public:
  static I18n &instance();

  void initialize(QCoreApplication *app);

  QString currentLanguage() const;

  bool setLanguage(const QString &languageCode);

  QMap<QString, QString> availableLanguages() const;

  QString systemLanguage() const;

  bool isLanguageAvailable(const QString &languageCode) const;

  QString translationsDirectory() const;

signals:

  void languageChanged(const QString &newLanguage);

private:
  I18n();
  ~I18n();
  I18n(const I18n &) = delete;
  I18n &operator=(const I18n &) = delete;

  void loadAvailableLanguages();
  bool loadTranslation(const QString &languageCode);

  QCoreApplication *m_app;
  QTranslator *m_translator;
  QTranslator *m_qtTranslator;
  QString m_currentLanguage;
  QMap<QString, QString> m_availableLanguages;
};

#define TR(text) QCoreApplication::translate("Lightpad", text)

#define TRC(context, text) QCoreApplication::translate(context, text)

#endif
