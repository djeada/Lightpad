#include "accessibility/accessibilitymanager.h"
#include <QtTest/QtTest>

class TestAccessibility : public QObject {
  Q_OBJECT

private slots:
  void testSingletonInstance();
  void testDefaultValues();
  void testFontScale();
  void testFontScaleBounds();
  void testHighContrast();
  void testReducedMotion();
  void testScreenReader();
  void testProfiles();
};

void TestAccessibility::testSingletonInstance() {
  AccessibilityManager &am1 = AccessibilityManager::instance();
  AccessibilityManager &am2 = AccessibilityManager::instance();
  QCOMPARE(&am1, &am2);
}

void TestAccessibility::testDefaultValues() {
  AccessibilityManager &am = AccessibilityManager::instance();

  am.applyProfile(AccessibilityManager::Profile::Default);

  QCOMPARE(am.currentProfile(), AccessibilityManager::Profile::Default);
  QCOMPARE(am.fontScale(), 1.0);
  QVERIFY(!am.isHighContrastEnabled());
  QVERIFY(!am.isReducedMotionEnabled());
  QVERIFY(!am.isScreenReaderEnabled());
}

void TestAccessibility::testFontScale() {
  AccessibilityManager &am = AccessibilityManager::instance();

  am.setFontScale(1.5);
  QCOMPARE(am.fontScale(), 1.5);

  am.increaseFontScale();
  QVERIFY(am.fontScale() > 1.5);

  am.decreaseFontScale();
  am.decreaseFontScale();
  QVERIFY(am.fontScale() < 1.5);

  am.resetFontScale();
  QCOMPARE(am.fontScale(), 1.0);
}

void TestAccessibility::testFontScaleBounds() {
  AccessibilityManager &am = AccessibilityManager::instance();

  am.setFontScale(0.1);
  QVERIFY(am.fontScale() >= 0.5);

  am.setFontScale(10.0);
  QVERIFY(am.fontScale() <= 3.0);

  am.resetFontScale();
}

void TestAccessibility::testHighContrast() {
  AccessibilityManager &am = AccessibilityManager::instance();

  QSignalSpy spy(&am, &AccessibilityManager::highContrastChanged);

  am.setHighContrastEnabled(true);
  QVERIFY(am.isHighContrastEnabled());
  QCOMPARE(spy.count(), 1);

  am.setHighContrastEnabled(false);
  QVERIFY(!am.isHighContrastEnabled());
  QCOMPARE(spy.count(), 2);
}

void TestAccessibility::testReducedMotion() {
  AccessibilityManager &am = AccessibilityManager::instance();

  QSignalSpy spy(&am, &AccessibilityManager::reducedMotionChanged);

  am.setReducedMotionEnabled(true);
  QVERIFY(am.isReducedMotionEnabled());
  QCOMPARE(spy.count(), 1);

  am.setReducedMotionEnabled(false);
  QVERIFY(!am.isReducedMotionEnabled());
}

void TestAccessibility::testScreenReader() {
  AccessibilityManager &am = AccessibilityManager::instance();

  QSignalSpy spy(&am, &AccessibilityManager::screenReaderChanged);

  am.setScreenReaderEnabled(true);
  QVERIFY(am.isScreenReaderEnabled());
  QCOMPARE(spy.count(), 1);

  am.setScreenReaderEnabled(false);
  QVERIFY(!am.isScreenReaderEnabled());
}

void TestAccessibility::testProfiles() {
  AccessibilityManager &am = AccessibilityManager::instance();

  am.applyProfile(AccessibilityManager::Profile::HighContrast);
  QCOMPARE(am.currentProfile(), AccessibilityManager::Profile::HighContrast);
  QVERIFY(am.isHighContrastEnabled());

  am.applyProfile(AccessibilityManager::Profile::LargeText);
  QCOMPARE(am.currentProfile(), AccessibilityManager::Profile::LargeText);
  QCOMPARE(am.fontScale(), 1.5);

  am.applyProfile(AccessibilityManager::Profile::ScreenReader);
  QCOMPARE(am.currentProfile(), AccessibilityManager::Profile::ScreenReader);
  QVERIFY(am.isScreenReaderEnabled());
  QVERIFY(am.isReducedMotionEnabled());

  am.applyProfile(AccessibilityManager::Profile::Default);
  QCOMPARE(am.currentProfile(), AccessibilityManager::Profile::Default);
}

QTEST_MAIN(TestAccessibility)
#include "test_accessibility.moc"
