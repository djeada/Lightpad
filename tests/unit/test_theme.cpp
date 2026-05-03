#include "settings/theme.h"
#include "theme/themeengine.h"
#include "theme/themepresets.h"
#include "ui/uistylehelper.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestTheme : public QObject {
  Q_OBJECT

private slots:
  void testDefaultConstructor();
  void testWriteToJson();
  void testReadFromJson();
  void testBuiltInPresetsStayDistinct();
  void testDaylightUsesLightSemantics();
  void testMatrixKeepsSemanticStatusColors();
  void testLegacyThemeBridgeCarriesExpandedSemanticTokens();
  void testGenerateFromSeedBuildsSemanticTheme();
  void testUiStyleHelperHonorsPanelBorders();
  void testUiStyleHelperGlowChangesSharedStyles();
  void testThemeEngineDeletesOnlyCustomThemes();
};

void TestTheme::testDefaultConstructor() {
  Theme theme;

  QCOMPARE(theme.backgroundColor, QColor("#0a0e14"));
  QCOMPARE(theme.foregroundColor, QColor("#b3b1ad"));
  QVERIFY(theme.highlightColor.isValid());
  QCOMPARE(theme.lineNumberAreaColor, QColor("#0a0e14"));
}

void TestTheme::testWriteToJson() {
  Theme theme;
  QJsonObject json;

  theme.write(json);

  QVERIFY(json.contains("backgroundColor"));
  QVERIFY(json.contains("foregroundColor"));
  QVERIFY(json.contains("highlightColor"));
  QVERIFY(json.contains("lineNumberAreaColor"));
  QVERIFY(json.contains("keywordFormat_0"));
  QVERIFY(json.contains("keywordFormat_1"));
  QVERIFY(json.contains("keywordFormat_2"));
  QVERIFY(json.contains("searchFormat"));
  QVERIFY(json.contains("singleLineCommentFormat"));
  QVERIFY(json.contains("functionFormat"));
  QVERIFY(json.contains("quotationFormat"));
  QVERIFY(json.contains("classFormat"));
  QVERIFY(json.contains("numberFormat"));

  QVERIFY(json["backgroundColor"].isString());
  QVERIFY(json["foregroundColor"].isString());
}

void TestTheme::testReadFromJson() {
  Theme theme;
  QJsonObject json;

  json["backgroundColor"] = "#ff0000";
  json["foregroundColor"] = "#00ff00";
  json["highlightColor"] = "#111111";
  json["keywordFormat_0"] = "#123456";
  json["numberFormat"] = "#654321";

  theme.read(json);

  QCOMPARE(theme.backgroundColor, QColor("#ff0000"));
  QCOMPARE(theme.foregroundColor, QColor("#00ff00"));
  QCOMPARE(theme.highlightColor, QColor("#111111"));
  QCOMPARE(theme.keywordFormat_0, QColor("#123456"));
  QCOMPARE(theme.numberFormat, QColor("#654321"));
}

void TestTheme::testBuiltInPresetsStayDistinct() {
  const ThemeDefinition minimal = ThemePresets::minimalDark();
  const ThemeDefinition github = ThemePresets::githubDark();
  const ThemeDefinition midnight = ThemePresets::midnightBlue();

  QCOMPARE(minimal.name, QString("Minimal Dark"));
  QVERIFY(!minimal.ui.panelBorders);
  QVERIFY(minimal.colors.surfaceRaised != minimal.colors.surfaceBase);
  QVERIFY(minimal.colors.borderDefault != minimal.colors.surfaceRaised);

  QCOMPARE(github.name, QString("GitHub Dark"));
  QCOMPARE(github.author, QString("Lightpad"));
  QVERIFY(github.colors.editorBg != midnight.colors.editorBg);
  QVERIFY(github.colors.accentPrimary != midnight.colors.accentPrimary);
  QCOMPARE(github.ui.glowIntensity, 0.0);
}

void TestTheme::testDaylightUsesLightSemantics() {
  const ThemeDefinition daylight = ThemePresets::daylight();

  QCOMPARE(daylight.name, QString("Daylight"));
  QCOMPARE(daylight.type, QString("light"));
  QVERIFY(daylight.colors.editorBg.lightness() >
          daylight.colors.textPrimary.lightness());
  QCOMPARE(daylight.colors.textInverse, QColor("#ffffff"));
  QCOMPARE(daylight.colors.btnPrimaryFg, QColor("#ffffff"));
  QVERIFY(daylight.colors.surfaceRaised != daylight.colors.surfaceSunken);
}

void TestTheme::testMatrixKeepsSemanticStatusColors() {
  const ThemeDefinition matrix = ThemePresets::matrix();

  QCOMPARE(matrix.name, QString("Matrix"));
  QVERIFY(matrix.colors.btnDangerBg != matrix.colors.btnPrimaryBg);
  QVERIFY(matrix.colors.statusError != matrix.colors.statusSuccess);
  QVERIFY(matrix.colors.statusWarning != matrix.colors.statusSuccess);
  QVERIFY(matrix.colors.ansiRed != matrix.colors.ansiGreen);
}

void TestTheme::testLegacyThemeBridgeCarriesExpandedSemanticTokens() {
  const ThemeDefinition preset = ThemePresets::githubDark();
  const Theme classic = preset.toClassicTheme();

  QCOMPARE(classic.infoColor, preset.colors.statusInfo);
  QCOMPARE(classic.diagnosticErrorColor, preset.colors.diagnosticError);
  QCOMPARE(classic.gitModifiedColor, preset.colors.gitModified);
  QCOMPARE(classic.testPassedColor, preset.colors.testPassed);
  QCOMPARE(classic.debugCurrentLineColor, preset.colors.debugCurrentLine);
}

void TestTheme::testGenerateFromSeedBuildsSemanticTheme() {
  const ThemeDefinition base = ThemePresets::minimalDark();
  const ThemeDefinition generated = ThemeDefinition::generateFromSeed(
      base, "Synthwave Custom", "Tester", QColor("#1a1033"), QColor("#f4eefe"),
      QColor("#ff4fd8"));

  QCOMPARE(generated.name, QString("Synthwave Custom"));
  QCOMPARE(generated.author, QString("Tester"));
  QCOMPARE(generated.colors.editorBg, QColor("#1a1033"));
  QCOMPARE(generated.colors.editorFg, QColor("#f4eefe"));
  QCOMPARE(generated.colors.accentPrimary, QColor("#ff4fd8"));
  QVERIFY(generated.colors.diagnosticWarning.isValid());
  QVERIFY(generated.colors.gitAdded.isValid());
  QVERIFY(generated.colors.testRunning.isValid());
  QVERIFY(generated.colors.debugBreakpoint.isValid());
  QVERIFY(generated.colors.statusError != generated.colors.statusSuccess);
}

void TestTheme::testUiStyleHelperHonorsPanelBorders() {
  ThemeDefinition theme = ThemePresets::githubDark();
  theme.ui.panelBorders = false;
  theme.normalize();

  const QString noBorders = UIStyleHelper::formDialogStyle(theme);
  QVERIFY(noBorders.contains("border: none;"));

  theme.ui.panelBorders = true;
  theme.normalize();

  const QString withBorders = UIStyleHelper::formDialogStyle(theme);
  QVERIFY(withBorders.contains("border: 1px solid"));
}

void TestTheme::testUiStyleHelperGlowChangesSharedStyles() {
  ThemeDefinition theme = ThemePresets::githubDark();
  theme.ui.glowIntensity = 0.0;
  theme.normalize();
  const QString noGlow = UIStyleHelper::lineEditStyle(theme);

  theme.ui.glowIntensity = 0.5;
  theme.normalize();
  const QString withGlow = UIStyleHelper::lineEditStyle(theme);

  QVERIFY(noGlow != withGlow);
}

void TestTheme::testThemeEngineDeletesOnlyCustomThemes() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());

  ThemeEngine &engine = ThemeEngine::instance();

  ThemeDefinition custom = ThemePresets::minimalDark();
  custom.name = QStringLiteral("Delete Me");
  custom.author = QStringLiteral("Tester");
  custom.type = QStringLiteral("custom");
  custom.normalize();

  custom = engine.saveUserTheme(custom);

  const QString filePath =
      tempDir.path() + QStringLiteral("/Lightpad/themes/Delete_Me.json");
  QVERIFY(engine.hasTheme(custom.name));
  QVERIFY(!engine.isBuiltinTheme(custom.name));
  QVERIFY(QFileInfo::exists(filePath));

  const ThemeDefinition builtinBefore =
      engine.themeByName(QStringLiteral("Matrix"));
  ThemeDefinition builtinCopy = builtinBefore;
  builtinCopy.type = QStringLiteral("custom");
  builtinCopy.colors.accentPrimary = QColor(QStringLiteral("#ff00ff"));
  builtinCopy.normalize();

  const ThemeDefinition savedCopy = engine.saveUserTheme(builtinCopy);
  QVERIFY(savedCopy.name != QStringLiteral("Matrix"));
  QVERIFY(savedCopy.name.startsWith(QStringLiteral("Matrix Custom")));
  QCOMPARE(engine.themeByName(QStringLiteral("Matrix")).colors.accentPrimary,
           builtinBefore.colors.accentPrimary);
  QVERIFY(engine.hasTheme(savedCopy.name));

  QVERIFY(!engine.deleteUserTheme(QStringLiteral("Hacker Dark")));
  QVERIFY(engine.hasTheme(QStringLiteral("Hacker Dark")));

  QVERIFY(engine.deleteUserTheme(custom.name));
  QVERIFY(!engine.hasTheme(custom.name));
  QVERIFY(!QFileInfo::exists(filePath));

  QVERIFY(engine.deleteUserTheme(savedCopy.name));
  QVERIFY(!engine.hasTheme(savedCopy.name));
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
