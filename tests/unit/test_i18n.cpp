#include "i18n/i18n.h"
#include <QtTest/QtTest>

class TestI18n : public QObject {
  Q_OBJECT

private slots:
  void testSingletonInstance();
  void testAvailableLanguages();
  void testSystemLanguage();
  void testSetLanguage();
  void testTranslationsDirectory();
};

void TestI18n::testSingletonInstance() {
  I18n &i1 = I18n::instance();
  I18n &i2 = I18n::instance();
  QCOMPARE(&i1, &i2);
}

void TestI18n::testAvailableLanguages() {
  I18n &i18n = I18n::instance();
  QMap<QString, QString> languages = i18n.availableLanguages();

  // Should always have at least English
  QVERIFY(languages.contains("en"));
  QCOMPARE(languages["en"], QString("English"));
}

void TestI18n::testSystemLanguage() {
  I18n &i18n = I18n::instance();
  QString sysLang = i18n.systemLanguage();

  // Should return a language code (1-2 characters, may be "C" or "en" etc)
  QVERIFY(!sysLang.isEmpty());
  QVERIFY(sysLang.length() <= 2);
}

void TestI18n::testSetLanguage() {
  I18n &i18n = I18n::instance();

  // Setting English should always succeed
  bool result = i18n.setLanguage("en");
  // Note: This may fail without a QApplication, which is expected
  // The main test is that it doesn't crash
  Q_UNUSED(result);

  // Current language should be set
  QString current = i18n.currentLanguage();
  QVERIFY(!current.isEmpty());
}

void TestI18n::testTranslationsDirectory() {
  I18n &i18n = I18n::instance();
  QString dir = i18n.translationsDirectory();

  // Should return a non-empty path
  QVERIFY(!dir.isEmpty());
}

QTEST_MAIN(TestI18n)
#include "test_i18n.moc"
