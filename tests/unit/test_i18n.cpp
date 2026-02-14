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

  QVERIFY(languages.contains("en"));
  QCOMPARE(languages["en"], QString("English"));
}

void TestI18n::testSystemLanguage() {
  I18n &i18n = I18n::instance();
  QString sysLang = i18n.systemLanguage();

  QVERIFY(!sysLang.isEmpty());
  QVERIFY(sysLang.length() <= 2);
}

void TestI18n::testSetLanguage() {
  I18n &i18n = I18n::instance();

  bool result = i18n.setLanguage("en");

  Q_UNUSED(result);

  QString current = i18n.currentLanguage();
  QVERIFY(!current.isEmpty());
}

void TestI18n::testTranslationsDirectory() {
  I18n &i18n = I18n::instance();
  QString dir = i18n.translationsDirectory();

  QVERIFY(!dir.isEmpty());
}

QTEST_MAIN(TestI18n)
#include "test_i18n.moc"
