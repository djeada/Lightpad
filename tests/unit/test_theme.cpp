#include "settings/theme.h"
#include "theme/colorcontrast.h"
#include "theme/themeengine.h"
#include "theme/themepresets.h"
#include "ui/uistylehelper.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

QVector<ThemeDefinition> allPresets() {
  return {
      ThemePresets::hackerDark(), ThemePresets::minimalDark(),
      ThemePresets::githubDark(), ThemePresets::midnightBlue(),
      ThemePresets::dracula(),    ThemePresets::monokaiPro(),
      ThemePresets::nord(),       ThemePresets::solarizedDark(),
      ThemePresets::cyberpunk(),  ThemePresets::matrix(),
      ThemePresets::ghost(),      ThemePresets::daylight(),
  };
}

QVector<QColor> declaredColors(const QString &style, const QString &property) {
  QVector<QColor> colors;
  const QRegularExpression re(
      QStringLiteral("(?:^|[;{\\s])%1:\\s*(#[0-9a-fA-F]{6,8})")
          .arg(QRegularExpression::escape(property)));
  auto it = re.globalMatch(style);
  while (it.hasNext())
    colors.append(QColor(it.next().captured(1)));
  return colors;
}

int colorDistanceSquared(const QColor &a, const QColor &b) {
  const int dr = a.red() - b.red();
  const int dg = a.green() - b.green();
  const int db = a.blue() - b.blue();
  return dr * dr + dg * dg + db * db;
}

} // namespace

class TestTheme : public QObject {
  Q_OBJECT

private slots:
  void testDefaultConstructor();
  void testWriteToJson();
  void testReadFromJson();
  void testBuiltInPresetsStayDistinct();
  void testBuiltInPresetsKeepStringsDistinctFromEditorText();
  void testDaylightUsesLightSemantics();
  void testMatrixKeepsSemanticStatusColors();
  void testLegacyThemeBridgeCarriesExpandedSemanticTokens();
  void testGenerateFromSeedBuildsSemanticTheme();
  void testUiStyleHelperHonorsPanelBorders();
  void testUiStyleHelperGlowChangesSharedStyles();
  void testThemeEngineDeletesOnlyCustomThemes();
  void testReadablePresetsMeetContrast();
  void testReadableIsIdempotent();
  void testPresetsDeriveSemanticColorsFromOwnPalette();
  void testStyleHelperSelectionTextIsReadable();
  void testThemeEngineKeepsAuthoredSourceTheme();
  void testContrastEnsureReachesTarget();
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

void TestTheme::testBuiltInPresetsKeepStringsDistinctFromEditorText() {
  const QVector<ThemeDefinition> presets = {
      ThemePresets::hackerDark(), ThemePresets::minimalDark(),
      ThemePresets::githubDark(), ThemePresets::midnightBlue(),
      ThemePresets::dracula(),    ThemePresets::monokaiPro(),
      ThemePresets::nord(),       ThemePresets::solarizedDark(),
      ThemePresets::cyberpunk(),  ThemePresets::matrix(),
      ThemePresets::ghost(),      ThemePresets::daylight(),
  };

  for (const ThemeDefinition &preset : presets) {
    QVERIFY2(colorDistanceSquared(preset.colors.editorFg,
                                  preset.colors.syntaxString) > 7000,
             qPrintable(QString("%1 string color is still too close to editor "
                                "foreground (%2 vs %3)")
                            .arg(preset.name, preset.colors.editorFg.name(),
                                 preset.colors.syntaxString.name())));
  }
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
  QVERIFY(matrix.colors.syntaxKeyword != matrix.colors.syntaxClass);
  QVERIFY(matrix.colors.syntaxFunction != matrix.colors.syntaxString);
  QVERIFY(matrix.colors.syntaxType != matrix.colors.syntaxKeyword2);
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

void TestTheme::testReadablePresetsMeetContrast() {
  using ColorContrast::flatten;
  using ColorContrast::ratio;
  struct Pair {
    const char *name;
    QColor ThemeColors::*fg;
    QColor ThemeColors::*bg;
    qreal minimum;
  };
  using C = ThemeColors;
  const QVector<Pair> pairs = {
      {"textPrimary/surfaceBase", &C::textPrimary, &C::surfaceBase, 4.5},
      {"textPrimary/surfaceRaised", &C::textPrimary, &C::surfaceRaised, 4.5},
      {"textPrimary/surfaceOverlay", &C::textPrimary, &C::surfaceOverlay, 4.5},
      {"textPrimary/accentSoft", &C::textPrimary, &C::accentSoft, 4.5},
      {"textPrimary/treeSelectedBg", &C::textPrimary, &C::treeSelectedBg, 4.5},
      {"textSecondary/surfaceBase", &C::textSecondary, &C::surfaceBase, 4.5},
      {"textSecondary/surfaceRaised", &C::textSecondary, &C::surfaceRaised,
       4.5},
      {"textMuted/surfaceBase", &C::textMuted, &C::surfaceBase, 3.0},
      {"textMuted/surfaceRaised", &C::textMuted, &C::surfaceRaised, 3.0},
      {"textInverse/accentPrimary", &C::textInverse, &C::accentPrimary, 4.5},
      {"accentPrimary/surfaceBase", &C::accentPrimary, &C::surfaceBase, 3.0},
      {"btnPrimaryFg/btnPrimaryBg", &C::btnPrimaryFg, &C::btnPrimaryBg, 4.5},
      {"btnPrimaryFg/btnPrimaryHover", &C::btnPrimaryFg, &C::btnPrimaryHover,
       4.5},
      {"btnSecondaryFg/btnSecondaryHover", &C::btnSecondaryFg,
       &C::btnSecondaryHover, 4.5},
      {"btnDangerFg/btnDangerBg", &C::btnDangerFg, &C::btnDangerBg, 4.5},
      {"btnDangerFg/btnDangerHover", &C::btnDangerFg, &C::btnDangerHover, 4.5},
      {"inputFg/inputBg", &C::inputFg, &C::inputBg, 4.5},
      {"inputFg/inputSelection", &C::inputFg, &C::inputSelection, 4.5},
      {"tabFg/tabBg", &C::tabFg, &C::tabBg, 3.5},
      {"tabActiveFg/tabActiveBg", &C::tabActiveFg, &C::tabActiveBg, 4.5},
      {"editorFg/editorBg", &C::editorFg, &C::editorBg, 4.5},
      {"editorGutterFg/editorGutter", &C::editorGutterFg, &C::editorGutter,
       3.0},
      {"termFg/termBg", &C::termFg, &C::termBg, 4.5},
      {"syntaxComment/editorBg", &C::syntaxComment, &C::editorBg, 2.5},
      {"statusError/surfaceRaised", &C::statusError, &C::surfaceRaised, 3.0},
      {"statusWarning/surfaceRaised", &C::statusWarning, &C::surfaceRaised,
       3.0},
      {"gitAdded/surfaceBase", &C::gitAdded, &C::surfaceBase, 3.0},
      {"gitModified/surfaceBase", &C::gitModified, &C::surfaceBase, 3.0},
      {"testFailed/surfaceBase", &C::testFailed, &C::surfaceBase, 3.0},
  };

  for (ThemeDefinition preset : allPresets()) {
    preset.normalize();
    const ThemeDefinition readable = preset.readable();
    const ThemeColors &c = readable.colors;
    for (const Pair &pair : pairs) {
      const QColor bg = flatten(c.*pair.bg, flatten(c.surfaceBase, Qt::black));
      const QColor fg = flatten(c.*pair.fg, bg);
      const qreal r = ratio(fg, bg);
      QVERIFY2(r + 0.01 >= pair.minimum,
               qPrintable(QString("%1 %2 contrast %3 < %4 (%5 on %6)")
                              .arg(readable.name, pair.name)
                              .arg(r, 0, 'f', 2)
                              .arg(pair.minimum)
                              .arg(fg.name(), bg.name())));
    }
  }
}

void TestTheme::testReadableIsIdempotent() {
  for (ThemeDefinition preset : allPresets()) {
    preset.normalize();
    const ThemeDefinition once = preset.readable();
    const ThemeDefinition twice = once.readable();
    QJsonObject a;
    QJsonObject b;
    once.write(a);
    twice.write(b);
    QVERIFY2(a == b, qPrintable(preset.name));
  }
}

void TestTheme::testPresetsDeriveSemanticColorsFromOwnPalette() {
  const ThemeDefinition hacker = ThemePresets::hackerDark();
  const ThemeDefinition daylight = ThemePresets::daylight();
  const ThemeDefinition dracula = ThemePresets::dracula();

  QCOMPARE(daylight.colors.gitAdded, daylight.colors.statusSuccess);
  QCOMPARE(daylight.colors.testFailed, daylight.colors.statusError);
  QCOMPARE(daylight.colors.diagnosticWarning, daylight.colors.statusWarning);
  QCOMPARE(dracula.colors.gitDeleted, dracula.colors.statusError);
  QVERIFY(dracula.colors.gitAdded != hacker.colors.gitAdded);

  QJsonObject json;
  daylight.write(json);
  json.remove(QStringLiteral("git"));
  ThemeDefinition reread;
  reread.read(json);
  QCOMPARE(reread.colors.gitAdded, reread.colors.statusSuccess);
}

void TestTheme::testStyleHelperSelectionTextIsReadable() {
  for (ThemeDefinition preset : allPresets()) {
    preset.normalize();
    const ThemeDefinition readable = preset.readable();
    const Theme classic = readable.toClassicTheme();
    const ThemeColors &c = readable.colors;

    struct Check {
      QString label;
      QString style;
      QColor background;
    };
    const QVector<Check> checks = {
        {"combo(def)", UIStyleHelper::comboBoxStyle(readable),
         c.inputSelection},
        {"combo(classic)", UIStyleHelper::comboBoxStyle(classic),
         classic.accentSoftColor},
        {"lineEdit(def)", UIStyleHelper::lineEditStyle(readable),
         c.inputSelection},
        {"lineEdit(classic)", UIStyleHelper::lineEditStyle(classic),
         classic.accentSoftColor},
        {"tree(def)", UIStyleHelper::treeWidgetStyle(readable),
         c.treeSelectedBg},
        {"tree(classic)", UIStyleHelper::treeWidgetStyle(classic),
         classic.accentSoftColor},
        {"table(def)", UIStyleHelper::tableWidgetStyle(readable), c.accentSoft},
        {"table(classic)", UIStyleHelper::tableWidgetStyle(classic),
         classic.accentSoftColor},
    };
    for (const Check &check : checks) {
      const QVector<QColor> colors =
          declaredColors(check.style, QStringLiteral("selection-color"));
      QVERIFY2(!colors.isEmpty(), qPrintable(check.label));
      const QColor bg = ColorContrast::flatten(check.background, c.surfaceBase);
      for (const QColor &fg : colors) {
        QVERIFY2(ColorContrast::ratio(fg, bg) >= 4.4,
                 qPrintable(QString("%1 %2: %3 on %4")
                                .arg(readable.name, check.label, fg.name(),
                                     bg.name())));
      }
    }

    for (UIStyleHelper::Tone tone :
         {UIStyleHelper::Tone::Neutral, UIStyleHelper::Tone::Accent,
          UIStyleHelper::Tone::Success, UIStyleHelper::Tone::Warning,
          UIStyleHelper::Tone::Error, UIStyleHelper::Tone::Info}) {
      const QString badge = UIStyleHelper::badgeStyle(readable, tone);
      const QVector<QColor> bgs =
          declaredColors(badge, QStringLiteral("background"));
      const QVector<QColor> fgs =
          declaredColors(badge, QStringLiteral("color"));
      QVERIFY(!bgs.isEmpty() && !fgs.isEmpty());
      QVERIFY2(ColorContrast::ratio(fgs.first(), bgs.first()) >= 4.4,
               qPrintable(QString("%1 badge tone %2")
                              .arg(readable.name)
                              .arg(static_cast<int>(tone))));
    }
  }
}

void TestTheme::testThemeEngineKeepsAuthoredSourceTheme() {
  ThemeEngine &engine = ThemeEngine::instance();
  engine.setActiveTheme(QStringLiteral("Nord"));
  const ThemeDefinition authored = engine.themeByName(QStringLiteral("Nord"));
  QCOMPARE(engine.activeThemeSource().colors.textMuted,
           authored.colors.textMuted);
  QVERIFY(engine.activeTheme().colors.textMuted != authored.colors.textMuted);
  QVERIFY(ColorContrast::ratio(engine.activeTheme().colors.textMuted,
                               engine.activeTheme().colors.surfaceBase) >= 3.0);
  engine.setActiveTheme(QStringLiteral("Hacker Dark"));
}

void TestTheme::testContrastEnsureReachesTarget() {
  const QColor darkBg("#101418");
  const QColor lightBg("#f6f8fa");
  const QColor adjustedOnDark =
      ColorContrast::ensure(QColor("#202830"), darkBg);
  QVERIFY(ColorContrast::ratio(adjustedOnDark, darkBg) >= 4.5);
  const QColor adjustedOnLight =
      ColorContrast::ensure(QColor("#e0e4e8"), lightBg);
  QVERIFY(ColorContrast::ratio(adjustedOnLight, lightBg) >= 4.5);
  const QColor untouched = ColorContrast::ensure(QColor("#ffffff"), darkBg);
  QCOMPARE(untouched, QColor("#ffffff"));
  QCOMPARE(ColorContrast::bestOf(QColor("#0969da"),
                                 {QColor("#0969da"), QColor("#ffffff")}),
           QColor("#ffffff"));
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
