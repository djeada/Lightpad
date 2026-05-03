#include "themedefinition.h"
#include "../settings/theme.h"

#include <QJsonArray>
#include <QtGlobal>

namespace {
QColor withAlpha(QColor color, int alpha) {
  color.setAlpha(qBound(0, alpha, 255));
  return color;
}

QColor mix(const QColor &a, const QColor &b, qreal t) {
  return ThemeDefinition::lerp(a, b, t);
}

QColor accentVariant(const QColor &accent, int hueShift, int saturationDelta,
                     int valueDelta) {
  QColor hsv = accent.toHsv();
  int hue = hsv.hue();
  if (hue < 0)
    hue = 200;
  const int saturation = qBound(0, hsv.saturation() + saturationDelta, 255);
  const int value = qBound(48, hsv.value() + valueDelta, 255);
  return QColor::fromHsv((hue + hueShift + 360) % 360, saturation, value,
                         accent.alpha());
}
} // namespace

ThemeDefinition::ThemeDefinition() {
  name = "Hacker Dark";
  author = "Lightpad";
  type = "dark";

  colors.editorBg = QColor("#101418");
  colors.editorFg = QColor("#c3d3cc");
  colors.editorGutter = QColor("#0c1014");
  colors.editorGutterFg = QColor("#58645f");
  colors.editorLineHighlight = QColor("#151c20");
  colors.editorSelection = QColor("#213a35");
  colors.editorSelectionFg = QColor("#e4f3ed");
  colors.editorCursor = QColor("#7dffb2");
  colors.editorFindMatch = QColor("#2fbf7140");
  colors.editorFindMatchActive = QColor("#7dffb260");
  colors.editorBracketMatch = QColor("#7dffb238");
  colors.editorIndentGuide = QColor("#222b30");
  colors.editorWhitespace = QColor("#2d383d");

  colors.syntaxKeyword = QColor("#65d6c4");
  colors.syntaxKeyword2 = QColor("#7bb7ff");
  colors.syntaxKeyword3 = QColor("#c7a7ff");
  colors.syntaxString = QColor("#9bd889");
  colors.syntaxComment = QColor("#63746d");
  colors.syntaxFunction = QColor("#8fcfff");
  colors.syntaxClass = QColor("#8ee6b8");
  colors.syntaxNumber = QColor("#f0b36a");
  colors.syntaxOperator = QColor("#d99adf");
  colors.syntaxType = QColor("#8ee6b8");
  colors.syntaxConstant = QColor("#e7d789");
  colors.syntaxTag = QColor("#65d6c4");
  colors.syntaxAttribute = QColor("#c7a7ff");
  colors.syntaxRegex = QColor("#d2a8ff");
  colors.syntaxEscape = QColor("#ffb06b");

  colors.surfaceBase = QColor("#101418");
  colors.surfaceRaised = QColor("#151b20");
  colors.surfaceOverlay = QColor("#1a2329");
  colors.surfaceSunken = QColor("#0b0f12");
  colors.surfacePopover = QColor("#1d282f");

  colors.borderDefault = QColor("#263832");
  colors.borderSubtle = QColor("#1b2825");
  colors.borderStrong = QColor("#3a5b51");
  colors.borderFocus = QColor("#7dffb2");

  colors.textPrimary = QColor("#c3d3cc");
  colors.textSecondary = QColor("#899890");
  colors.textMuted = QColor("#5f6f68");
  colors.textDisabled = QColor("#3f4a46");
  colors.textInverse = QColor("#0c1014");
  colors.textLink = QColor("#7bb7ff");

  colors.btnPrimaryBg = QColor("#2fbf71");
  colors.btnPrimaryFg = QColor("#07100c");
  colors.btnPrimaryHover = QColor("#55d98e");
  colors.btnPrimaryActive = QColor("#23975a");
  colors.btnSecondaryBg = QColor("#1d2b28");
  colors.btnSecondaryFg = QColor("#c3d3cc");
  colors.btnSecondaryHover = QColor("#243832");
  colors.btnSecondaryActive = QColor("#17231f");
  colors.btnDangerBg = QColor("#f85149");
  colors.btnDangerFg = QColor("#ffffff");
  colors.btnDangerHover = QColor("#fa6e68");
  colors.btnDangerActive = QColor("#d63d36");
  colors.btnGhostHover = QColor("#151c20");
  colors.btnGhostActive = QColor("#1a2329");

  colors.inputBg = QColor("#12191d");
  colors.inputFg = QColor("#c3d3cc");
  colors.inputBorder = QColor("#263832");
  colors.inputBorderFocus = QColor("#7dffb2");
  colors.inputPlaceholder = QColor("#5f6f68");
  colors.inputSelection = QColor("#213a35");

  colors.accentPrimary = QColor("#7dffb2");
  colors.accentSoft = QColor("#18372d");
  colors.accentGlow = QColor("#7dffb226");

  colors.scrollTrack = QColor("#0c1014");
  colors.scrollThumb = QColor("#263832");
  colors.scrollThumbHover = QColor("#3a5b51");

  colors.tabBg = QColor("#101418");
  colors.tabActiveBg = QColor("#151b20");
  colors.tabActiveBorder = QColor("#7dffb2");
  colors.tabHoverBg = QColor("#172025");
  colors.tabFg = QColor("#899890");
  colors.tabActiveFg = QColor("#d7e6df");

  colors.treeSelectedBg = QColor("#182a25");
  colors.treeHoverBg = QColor("#151c20");
  colors.treeIndentGuide = QColor("#222b30");

  colors.termBg = QColor("#101418");
  colors.termFg = QColor("#b8c9c1");
  colors.termCursor = QColor("#7dffb2");
  colors.termSelection = QColor("#213a35");

  colors.ansiBlack = QColor("#101418");
  colors.ansiRed = QColor("#f85149");
  colors.ansiGreen = QColor("#7dffb2");
  colors.ansiYellow = QColor("#e6b450");
  colors.ansiBlue = QColor("#7bb7ff");
  colors.ansiMagenta = QColor("#c7a7ff");
  colors.ansiCyan = QColor("#65d6c4");
  colors.ansiWhite = QColor("#c3d3cc");

  colors.ansiBrightBlack = QColor("#5f6f68");
  colors.ansiBrightRed = QColor("#fa6e68");
  colors.ansiBrightGreen = QColor("#9dffc7");
  colors.ansiBrightYellow = QColor("#ffee99");
  colors.ansiBrightBlue = QColor("#9dccff");
  colors.ansiBrightMagenta = QColor("#d8c2ff");
  colors.ansiBrightCyan = QColor("#9ce7da");
  colors.ansiBrightWhite = QColor("#e4f3ed");

  colors.statusSuccess = QColor("#7dffb2");
  colors.statusWarning = QColor("#e6b450");
  colors.statusError = QColor("#f85149");
  colors.statusInfo = QColor("#7bb7ff");

  ui.borderRadius = 7;
  ui.glowIntensity = 0.16;
  ui.chromeOpacity = 1.0;
  ui.animationSpeed = "normal";
  ui.scanlineEffect = false;
  ui.panelBorders = true;
  normalize();
}

Theme ThemeDefinition::toClassicTheme() const {
  Theme t;
  t.backgroundColor = colors.editorBg;
  t.foregroundColor = colors.editorFg;
  t.highlightColor = colors.editorLineHighlight;
  t.lineNumberAreaColor = colors.editorGutter;

  t.keywordFormat_0 = colors.syntaxKeyword;
  t.keywordFormat_1 = colors.syntaxKeyword2;
  t.keywordFormat_2 = colors.syntaxKeyword3;
  t.searchFormat = colors.editorFindMatch;
  t.singleLineCommentFormat = colors.syntaxComment;
  t.functionFormat = colors.syntaxFunction;
  t.quotationFormat = colors.syntaxString;
  t.classFormat = colors.syntaxClass;
  t.numberFormat = colors.syntaxNumber;

  t.surfaceColor = colors.surfaceRaised;
  t.surfaceAltColor = colors.surfaceOverlay;
  t.borderColor = colors.borderDefault;
  t.hoverColor = colors.btnGhostHover;
  t.pressedColor = colors.btnGhostActive;
  t.accentColor = colors.accentPrimary;
  t.accentSoftColor = colors.accentSoft;
  t.successColor = colors.statusSuccess;
  t.warningColor = colors.statusWarning;
  t.errorColor = colors.statusError;
  t.infoColor = colors.statusInfo;
  t.diagnosticErrorColor = colors.diagnosticError;
  t.diagnosticWarningColor = colors.diagnosticWarning;
  t.diagnosticInfoColor = colors.diagnosticInfo;
  t.diagnosticHintColor = colors.diagnosticHint;
  t.gitAddedColor = colors.gitAdded;
  t.gitModifiedColor = colors.gitModified;
  t.gitDeletedColor = colors.gitDeleted;
  t.gitRenamedColor = colors.gitRenamed;
  t.gitCopiedColor = colors.gitCopied;
  t.gitUntrackedColor = colors.gitUntracked;
  t.gitConflictedColor = colors.gitConflicted;
  t.gitIgnoredColor = colors.gitIgnored;
  t.diffAddedColor = colors.diffAdded;
  t.diffModifiedColor = colors.diffModified;
  t.diffRemovedColor = colors.diffRemoved;
  t.diffConflictColor = colors.diffConflict;
  t.testPassedColor = colors.testPassed;
  t.testFailedColor = colors.testFailed;
  t.testSkippedColor = colors.testSkipped;
  t.testRunningColor = colors.testRunning;
  t.testQueuedColor = colors.testQueued;
  t.debugReadyColor = colors.debugReady;
  t.debugStartingColor = colors.debugStarting;
  t.debugRunningColor = colors.debugRunning;
  t.debugPausedColor = colors.debugPaused;
  t.debugErrorColor = colors.debugError;
  t.debugBreakpointColor = colors.debugBreakpoint;
  t.debugCurrentLineColor = colors.debugCurrentLine;
  t.borderRadius = ui.borderRadius;
  t.glowIntensity = ui.glowIntensity;
  t.chromeOpacity = ui.chromeOpacity;
  t.scanlineEffect = ui.scanlineEffect;
  t.panelBorders = ui.panelBorders;
  t.normalize();
  return t;
}

ThemeDefinition ThemeDefinition::fromClassicTheme(const Theme &c,
                                                  const QString &themeName) {
  ThemeDefinition d;
  d.name = themeName;
  d.author = "User";
  d.type = "dark";

  d.colors.editorBg = c.backgroundColor;
  d.colors.editorFg = c.foregroundColor;
  d.colors.editorGutter = c.lineNumberAreaColor;
  d.colors.editorGutterFg = lerp(c.foregroundColor, c.backgroundColor, 0.5);
  d.colors.editorLineHighlight = c.highlightColor;
  d.colors.editorSelection = lerp(c.accentColor, c.backgroundColor, 0.7);
  d.colors.editorSelectionFg = c.foregroundColor;
  d.colors.editorCursor = c.accentColor;
  d.colors.editorFindMatch = c.searchFormat;
  d.colors.editorFindMatchActive = lerp(c.searchFormat, c.foregroundColor, 0.3);
  d.colors.editorBracketMatch = lerp(c.accentColor, c.backgroundColor, 0.6);
  d.colors.editorIndentGuide = lerp(c.backgroundColor, c.foregroundColor, 0.08);
  d.colors.editorWhitespace = lerp(c.backgroundColor, c.foregroundColor, 0.12);

  d.colors.syntaxKeyword = c.keywordFormat_0;
  d.colors.syntaxKeyword2 = c.keywordFormat_1;
  d.colors.syntaxKeyword3 = c.keywordFormat_2;
  d.colors.syntaxString = c.quotationFormat;
  d.colors.syntaxComment = c.singleLineCommentFormat;
  d.colors.syntaxFunction = c.functionFormat;
  d.colors.syntaxClass = c.classFormat;
  d.colors.syntaxNumber = c.numberFormat;
  d.colors.syntaxOperator = lerp(c.foregroundColor, c.keywordFormat_0, 0.3);
  d.colors.syntaxType = c.classFormat;
  d.colors.syntaxConstant = c.keywordFormat_1;
  d.colors.syntaxTag = c.keywordFormat_0;
  d.colors.syntaxAttribute = c.quotationFormat;
  d.colors.syntaxRegex = lerp(c.quotationFormat, c.keywordFormat_2, 0.5);
  d.colors.syntaxEscape = lerp(c.quotationFormat, c.keywordFormat_2, 0.5);

  d.colors.surfaceBase = c.backgroundColor;
  d.colors.surfaceRaised = c.surfaceColor;
  d.colors.surfaceOverlay = c.surfaceAltColor;
  d.colors.surfaceSunken = lerp(c.backgroundColor, QColor(Qt::black), 0.15);
  d.colors.surfacePopover = lerp(c.surfaceColor, c.surfaceAltColor, 0.5);

  d.colors.borderDefault = c.borderColor;
  d.colors.borderSubtle = lerp(c.borderColor, c.backgroundColor, 0.4);
  d.colors.borderStrong = lerp(c.borderColor, c.foregroundColor, 0.2);
  d.colors.borderFocus = c.accentColor;

  d.colors.textPrimary = c.foregroundColor;
  d.colors.textSecondary = lerp(c.foregroundColor, c.backgroundColor, 0.35);
  d.colors.textMuted = lerp(c.foregroundColor, c.backgroundColor, 0.55);
  d.colors.textDisabled = lerp(c.foregroundColor, c.backgroundColor, 0.7);
  d.colors.textInverse = c.backgroundColor;
  d.colors.textLink = c.accentColor;

  d.colors.btnPrimaryBg = c.accentColor;
  d.colors.btnPrimaryFg = c.backgroundColor;
  d.colors.btnPrimaryHover = lerp(c.accentColor, c.foregroundColor, 0.15);
  d.colors.btnPrimaryActive = lerp(c.accentColor, c.backgroundColor, 0.2);
  d.colors.btnSecondaryBg = c.surfaceColor;
  d.colors.btnSecondaryFg = c.foregroundColor;
  d.colors.btnSecondaryHover = c.hoverColor;
  d.colors.btnSecondaryActive = c.pressedColor;
  d.colors.btnDangerBg = c.errorColor;
  d.colors.btnDangerFg = QColor("#ffffff");
  d.colors.btnDangerHover = lerp(c.errorColor, c.foregroundColor, 0.15);
  d.colors.btnDangerActive = lerp(c.errorColor, c.backgroundColor, 0.2);
  d.colors.btnGhostHover = c.hoverColor;
  d.colors.btnGhostActive = c.pressedColor;

  d.colors.inputBg = c.surfaceColor;
  d.colors.inputFg = c.foregroundColor;
  d.colors.inputBorder = c.borderColor;
  d.colors.inputBorderFocus = c.accentColor;
  d.colors.inputPlaceholder = lerp(c.foregroundColor, c.backgroundColor, 0.55);
  d.colors.inputSelection = lerp(c.accentColor, c.backgroundColor, 0.7);

  d.colors.accentPrimary = c.accentColor;
  d.colors.accentSoft = c.accentSoftColor;
  d.colors.accentGlow = QColor(c.accentColor.red(), c.accentColor.green(),
                               c.accentColor.blue(), 64);

  d.colors.scrollTrack = c.backgroundColor;
  d.colors.scrollThumb = lerp(c.borderColor, c.backgroundColor, 0.3);
  d.colors.scrollThumbHover = c.borderColor;

  d.colors.tabBg = c.backgroundColor;
  d.colors.tabActiveBg = c.surfaceColor;
  d.colors.tabActiveBorder = c.accentColor;
  d.colors.tabHoverBg = c.hoverColor;
  d.colors.tabFg = lerp(c.foregroundColor, c.backgroundColor, 0.4);
  d.colors.tabActiveFg = c.foregroundColor;

  d.colors.treeSelectedBg = lerp(c.accentColor, c.backgroundColor, 0.85);
  d.colors.treeHoverBg = c.hoverColor;
  d.colors.treeIndentGuide = lerp(c.backgroundColor, c.foregroundColor, 0.08);

  d.colors.termBg = c.backgroundColor;
  d.colors.termFg = c.foregroundColor;
  d.colors.termCursor = c.accentColor;
  d.colors.termSelection = lerp(c.accentColor, c.backgroundColor, 0.7);

  d.colors.ansiBlack = c.backgroundColor;
  d.colors.ansiRed = c.errorColor;
  d.colors.ansiGreen = c.successColor;
  d.colors.ansiYellow = c.warningColor;
  d.colors.ansiBlue = c.accentColor;
  d.colors.ansiMagenta = lerp(c.errorColor, c.accentColor, 0.5);
  d.colors.ansiCyan = lerp(c.accentColor, c.successColor, 0.5);
  d.colors.ansiWhite = c.foregroundColor;

  d.colors.ansiBrightBlack = lerp(c.backgroundColor, c.foregroundColor, 0.35);
  d.colors.ansiBrightRed = lerp(c.errorColor, c.foregroundColor, 0.2);
  d.colors.ansiBrightGreen = lerp(c.successColor, c.foregroundColor, 0.2);
  d.colors.ansiBrightYellow = lerp(c.warningColor, c.foregroundColor, 0.2);
  d.colors.ansiBrightBlue = lerp(c.accentColor, c.foregroundColor, 0.2);
  d.colors.ansiBrightMagenta =
      lerp(lerp(c.errorColor, c.accentColor, 0.5), c.foregroundColor, 0.2);
  d.colors.ansiBrightCyan =
      lerp(lerp(c.accentColor, c.successColor, 0.5), c.foregroundColor, 0.2);
  d.colors.ansiBrightWhite = lerp(c.foregroundColor, QColor(Qt::white), 0.2);

  d.colors.statusSuccess = c.successColor;
  d.colors.statusWarning = c.warningColor;
  d.colors.statusError = c.errorColor;
  d.colors.statusInfo = c.infoColor.isValid() ? c.infoColor : c.accentColor;
  d.colors.diagnosticError =
      c.diagnosticErrorColor.isValid() ? c.diagnosticErrorColor : c.errorColor;
  d.colors.diagnosticWarning = c.diagnosticWarningColor.isValid()
                                   ? c.diagnosticWarningColor
                                   : c.warningColor;
  d.colors.diagnosticInfo = c.diagnosticInfoColor.isValid()
                                ? c.diagnosticInfoColor
                                : d.colors.statusInfo;
  d.colors.diagnosticHint = c.diagnosticHintColor.isValid()
                                ? c.diagnosticHintColor
                                : c.singleLineCommentFormat;
  d.colors.gitAdded =
      c.gitAddedColor.isValid() ? c.gitAddedColor : c.successColor;
  d.colors.gitModified =
      c.gitModifiedColor.isValid() ? c.gitModifiedColor : c.warningColor;
  d.colors.gitDeleted =
      c.gitDeletedColor.isValid() ? c.gitDeletedColor : c.errorColor;
  d.colors.gitRenamed =
      c.gitRenamedColor.isValid() ? c.gitRenamedColor : d.colors.statusInfo;
  d.colors.gitCopied =
      c.gitCopiedColor.isValid() ? c.gitCopiedColor : d.colors.gitRenamed;
  d.colors.gitUntracked =
      c.gitUntrackedColor.isValid() ? c.gitUntrackedColor : c.successColor;
  d.colors.gitConflicted =
      c.gitConflictedColor.isValid() ? c.gitConflictedColor : c.errorColor;
  d.colors.gitIgnored = c.gitIgnoredColor.isValid() ? c.gitIgnoredColor
                                                    : c.singleLineCommentFormat;
  d.colors.diffAdded =
      c.diffAddedColor.isValid() ? c.diffAddedColor : d.colors.gitAdded;
  d.colors.diffModified = c.diffModifiedColor.isValid() ? c.diffModifiedColor
                                                        : d.colors.gitModified;
  d.colors.diffRemoved =
      c.diffRemovedColor.isValid() ? c.diffRemovedColor : d.colors.gitDeleted;
  d.colors.diffConflict = c.diffConflictColor.isValid()
                              ? c.diffConflictColor
                              : d.colors.gitConflicted;
  d.colors.testPassed =
      c.testPassedColor.isValid() ? c.testPassedColor : c.successColor;
  d.colors.testFailed =
      c.testFailedColor.isValid() ? c.testFailedColor : c.errorColor;
  d.colors.testSkipped = c.testSkippedColor.isValid()
                             ? c.testSkippedColor
                             : c.singleLineCommentFormat;
  d.colors.testRunning =
      c.testRunningColor.isValid() ? c.testRunningColor : d.colors.statusInfo;
  d.colors.testQueued = c.testQueuedColor.isValid() ? c.testQueuedColor
                                                    : c.singleLineCommentFormat;
  d.colors.debugReady = c.debugReadyColor.isValid()
                            ? c.debugReadyColor
                            : lerp(c.foregroundColor, c.backgroundColor, 0.35);
  d.colors.debugStarting =
      c.debugStartingColor.isValid() ? c.debugStartingColor : c.warningColor;
  d.colors.debugRunning =
      c.debugRunningColor.isValid() ? c.debugRunningColor : d.colors.statusInfo;
  d.colors.debugPaused =
      c.debugPausedColor.isValid() ? c.debugPausedColor : c.successColor;
  d.colors.debugError =
      c.debugErrorColor.isValid() ? c.debugErrorColor : c.errorColor;
  d.colors.debugBreakpoint =
      c.debugBreakpointColor.isValid() ? c.debugBreakpointColor : c.errorColor;
  d.colors.debugCurrentLine = c.debugCurrentLineColor.isValid()
                                  ? c.debugCurrentLineColor
                                  : c.accentColor;

  d.ui.borderRadius = c.borderRadius;
  d.ui.glowIntensity = c.glowIntensity;
  d.ui.chromeOpacity = c.chromeOpacity;
  d.ui.scanlineEffect = c.scanlineEffect;
  d.ui.panelBorders = c.panelBorders;

  d.normalize();
  return d;
}

ThemeDefinition ThemeDefinition::generateFromSeed(const ThemeDefinition &base,
                                                  const QString &themeName,
                                                  const QString &themeAuthor,
                                                  const QColor &background,
                                                  const QColor &foreground,
                                                  const QColor &accent) {
  ThemeDefinition generated = base;
  generated.name = themeName.trimmed().isEmpty()
                       ? QStringLiteral("Custom Theme")
                       : themeName.trimmed();
  generated.author = themeAuthor.trimmed().isEmpty() ? QStringLiteral("User")
                                                     : themeAuthor.trimmed();
  generated.type = background.lightness() > 150 ? QStringLiteral("light")
                                                : QStringLiteral("dark");

  const bool isLight = generated.type == QLatin1String("light");
  const QColor raised = isLight ? mix(background, QColor("#ffffff"), 0.28)
                                : mix(background, foreground, 0.06);
  const QColor overlay = isLight ? mix(background, QColor("#000000"), 0.04)
                                 : mix(background, foreground, 0.1);
  const QColor sunken = isLight ? mix(background, QColor("#000000"), 0.06)
                                : mix(background, QColor("#000000"), 0.18);
  const QColor accentSoft = withAlpha(
      mix(accent, background, isLight ? 0.82 : 0.72), isLight ? 96 : 128);

  generated.colors.editorBg = background;
  generated.colors.editorFg = foreground;
  generated.colors.editorGutter =
      isLight ? mix(background, QColor("#000000"), 0.03)
              : mix(background, QColor("#000000"), 0.18);
  generated.colors.editorGutterFg = mix(foreground, background, 0.48);
  generated.colors.editorLineHighlight =
      isLight ? mix(accent, background, 0.94)
              : mix(foreground, background, 0.08);
  generated.colors.editorSelection = accentSoft;
  generated.colors.editorSelectionFg = foreground;
  generated.colors.editorCursor = accent;
  generated.colors.editorFindMatch =
      withAlpha(mix(accent, generated.colors.statusWarning, 0.35), 96);
  generated.colors.editorFindMatchActive =
      withAlpha(mix(accent, foreground, 0.22), 128);
  generated.colors.editorBracketMatch = withAlpha(accent, 64);
  generated.colors.editorIndentGuide = mix(background, foreground, 0.12);
  generated.colors.editorWhitespace = mix(background, foreground, 0.16);

  generated.colors.surfaceBase = background;
  generated.colors.surfaceRaised = raised;
  generated.colors.surfaceOverlay = overlay;
  generated.colors.surfaceSunken = sunken;
  generated.colors.surfacePopover = mix(raised, overlay, 0.45);

  generated.colors.borderDefault =
      mix(background, foreground, isLight ? 0.18 : 0.15);
  generated.colors.borderSubtle =
      mix(background, foreground, isLight ? 0.1 : 0.08);
  generated.colors.borderStrong =
      mix(background, foreground, isLight ? 0.28 : 0.24);
  generated.colors.borderFocus = accent;

  generated.colors.textPrimary = foreground;
  generated.colors.textSecondary = mix(foreground, background, 0.32);
  generated.colors.textMuted = mix(foreground, background, 0.5);
  generated.colors.textDisabled = mix(foreground, background, 0.66);
  generated.colors.textInverse =
      isLight ? QColor("#ffffff") : QColor("#0c1014");
  generated.colors.textLink = accentVariant(accent, isLight ? -10 : 8, 10, 10);

  generated.colors.btnPrimaryBg = accent;
  generated.colors.btnPrimaryFg =
      accent.lightness() > 150 ? QColor("#0b1117") : QColor("#ffffff");
  generated.colors.btnPrimaryHover =
      isLight ? accent.darker(108) : accent.lighter(112);
  generated.colors.btnPrimaryActive =
      isLight ? accent.darker(118) : accent.darker(112);
  generated.colors.btnSecondaryBg = raised;
  generated.colors.btnSecondaryFg = foreground;
  generated.colors.btnSecondaryHover = mix(raised, foreground, 0.08);
  generated.colors.btnSecondaryActive = mix(raised, foreground, 0.14);
  generated.colors.btnDangerBg =
      accentVariant(accent, -28, 24, isLight ? -12 : 16);
  generated.colors.btnDangerFg = QColor("#ffffff");
  generated.colors.btnDangerHover = generated.colors.btnDangerBg.lighter(108);
  generated.colors.btnDangerActive = generated.colors.btnDangerBg.darker(112);
  generated.colors.btnGhostHover = mix(background, foreground, 0.05);
  generated.colors.btnGhostActive = mix(background, foreground, 0.1);

  generated.colors.inputBg = mix(raised, background, isLight ? 0.3 : 0.45);
  generated.colors.inputFg = foreground;
  generated.colors.inputBorder = generated.colors.borderDefault;
  generated.colors.inputBorderFocus = accent;
  generated.colors.inputPlaceholder = generated.colors.textMuted;
  generated.colors.inputSelection = accentSoft;

  generated.colors.accentPrimary = accent;
  generated.colors.accentSoft = accentSoft;
  generated.colors.accentGlow = withAlpha(accent, isLight ? 40 : 56);

  generated.colors.scrollTrack = sunken;
  generated.colors.scrollThumb = mix(background, foreground, 0.18);
  generated.colors.scrollThumbHover = mix(background, foreground, 0.28);

  generated.colors.tabBg = background;
  generated.colors.tabActiveBg = raised;
  generated.colors.tabActiveBorder = accent;
  generated.colors.tabHoverBg = generated.colors.btnGhostHover;
  generated.colors.tabFg = generated.colors.textSecondary;
  generated.colors.tabActiveFg = foreground;

  generated.colors.treeSelectedBg = accentSoft;
  generated.colors.treeHoverBg = generated.colors.btnGhostHover;
  generated.colors.treeIndentGuide = generated.colors.editorIndentGuide;

  generated.colors.termBg = background;
  generated.colors.termFg = foreground;
  generated.colors.termCursor = accent;
  generated.colors.termSelection = accentSoft;

  generated.colors.syntaxKeyword =
      accentVariant(accent, 0, 12, isLight ? -10 : 14);
  generated.colors.syntaxKeyword2 =
      accentVariant(accent, 32, 6, isLight ? -6 : 18);
  generated.colors.syntaxKeyword3 =
      accentVariant(accent, -36, 10, isLight ? -8 : 16);
  generated.colors.syntaxString =
      mix(base.colors.syntaxString, accentVariant(accent, 92, 18, 12), 0.45);
  generated.colors.syntaxComment = mix(foreground, background, 0.58);
  generated.colors.syntaxFunction =
      mix(base.colors.syntaxFunction, accentVariant(accent, 18, 4, 18), 0.42);
  generated.colors.syntaxClass =
      mix(base.colors.syntaxClass, accentVariant(accent, 74, 8, 14), 0.42);
  generated.colors.syntaxNumber =
      mix(base.colors.syntaxNumber, accentVariant(accent, -62, 22, 10), 0.45);
  generated.colors.syntaxOperator =
      mix(base.colors.syntaxOperator, accentVariant(accent, -22, 6, 8), 0.4);
  generated.colors.syntaxType =
      mix(generated.colors.syntaxClass, foreground, 0.1);
  generated.colors.syntaxConstant =
      mix(generated.colors.syntaxNumber, generated.colors.syntaxKeyword2, 0.35);
  generated.colors.syntaxTag = generated.colors.syntaxKeyword;
  generated.colors.syntaxAttribute = generated.colors.syntaxKeyword3;
  generated.colors.syntaxRegex =
      mix(generated.colors.syntaxString, generated.colors.syntaxKeyword3, 0.3);
  generated.colors.syntaxEscape =
      mix(generated.colors.syntaxString, generated.colors.syntaxNumber, 0.45);

  generated.colors.statusSuccess =
      mix(base.colors.statusSuccess, accentVariant(accent, 118, 14, 8), 0.35);
  generated.colors.statusWarning =
      mix(base.colors.statusWarning, accentVariant(accent, -76, 18, 10), 0.25);
  generated.colors.statusError =
      mix(base.colors.statusError, accentVariant(accent, -24, 28, 10), 0.22);
  generated.colors.statusInfo = accentVariant(accent, 8, 4, 8);

  generated.normalize();
  return generated;
}

static QColor readColor(const QJsonObject &obj, const QString &key) {
  QStringList parts = key.split('.');
  QJsonObject current = obj;
  for (int i = 0; i < parts.size() - 1; ++i) {
    if (current.contains(parts[i]) && current[parts[i]].isObject())
      current = current[parts[i]].toObject();
    else
      return QColor();
  }
  const QString &last = parts.last();
  if (current.contains(last) && current[last].isString())
    return QColor(current[last].toString());
  return QColor();
}

static void writeColor(QJsonObject &root, const QString &key, const QColor &c) {
  if (!c.isValid())
    return;
  QStringList parts = key.split('.');
  if (parts.size() == 1) {
    root[key] = c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb);
    return;
  }

  QString group = parts.first();
  QJsonObject sub =
      root.contains(group) ? root[group].toObject() : QJsonObject();
  QString childKey = parts.mid(1).join('.');
  writeColor(sub, childKey, c);
  root[group] = sub;
}

void ThemeDefinition::normalize() {
  auto ensure = [](QColor &dest, const QColor &fallback) {
    if (!dest.isValid())
      dest = fallback;
  };

  ensure(colors.statusInfo, colors.accentPrimary);

  ensure(colors.diagnosticError, colors.statusError);
  ensure(colors.diagnosticWarning, colors.statusWarning);
  ensure(colors.diagnosticInfo, colors.statusInfo);
  ensure(colors.diagnosticHint, mix(colors.textMuted, colors.textPrimary, 0.2));

  ensure(colors.gitAdded, colors.statusSuccess);
  ensure(colors.gitModified, colors.statusWarning);
  ensure(colors.gitDeleted, colors.statusError);
  ensure(colors.gitRenamed, colors.statusInfo);
  ensure(colors.gitCopied, mix(colors.gitRenamed, colors.accentPrimary, 0.35));
  ensure(colors.gitUntracked,
         mix(colors.statusSuccess, colors.accentPrimary, 0.2));
  ensure(colors.gitConflicted, colors.statusError);
  ensure(colors.gitIgnored, colors.textMuted);

  ensure(colors.diffAdded, mix(colors.gitAdded, colors.accentPrimary, 0.15));
  ensure(colors.diffModified, colors.gitModified);
  ensure(colors.diffRemoved, colors.gitDeleted);
  ensure(colors.diffConflict,
         mix(colors.gitConflicted, colors.gitModified, 0.3));

  ensure(colors.testPassed, colors.statusSuccess);
  ensure(colors.testFailed, colors.statusError);
  ensure(colors.testSkipped, mix(colors.textMuted, colors.statusWarning, 0.15));
  ensure(colors.testRunning, colors.statusInfo);
  ensure(colors.testQueued, colors.textMuted);

  ensure(colors.debugReady, colors.textSecondary);
  ensure(colors.debugStarting, colors.statusWarning);
  ensure(colors.debugRunning, colors.statusInfo);
  ensure(colors.debugPaused, colors.statusSuccess);
  ensure(colors.debugError, colors.statusError);
  ensure(colors.debugBreakpoint,
         mix(colors.statusError, colors.accentPrimary, 0.12));
  ensure(colors.debugCurrentLine, colors.accentPrimary);
}

void ThemeDefinition::read(const QJsonObject &json) {
  if (json.contains("name") && json["name"].isString())
    name = json["name"].toString();
  if (json.contains("author") && json["author"].isString())
    author = json["author"].toString();
  if (json.contains("type") && json["type"].isString())
    type = json["type"].toString();

  auto c = [&](const QString &key) { return readColor(json, key); };
  auto apply = [](QColor &dest, const QColor &src) {
    if (src.isValid())
      dest = src;
  };

  apply(colors.editorBg, c("editor.background"));
  apply(colors.editorFg, c("editor.foreground"));
  apply(colors.editorGutter, c("editor.gutter"));
  apply(colors.editorGutterFg, c("editor.gutterForeground"));
  apply(colors.editorLineHighlight, c("editor.lineHighlight"));
  apply(colors.editorSelection, c("editor.selection"));
  apply(colors.editorSelectionFg, c("editor.selectionForeground"));
  apply(colors.editorCursor, c("editor.cursor"));
  apply(colors.editorFindMatch, c("editor.findMatch"));
  apply(colors.editorFindMatchActive, c("editor.findMatchActive"));
  apply(colors.editorBracketMatch, c("editor.bracketMatch"));
  apply(colors.editorIndentGuide, c("editor.indentGuide"));
  apply(colors.editorWhitespace, c("editor.whitespace"));

  apply(colors.syntaxKeyword, c("syntax.keyword"));
  apply(colors.syntaxKeyword2, c("syntax.keyword2"));
  apply(colors.syntaxKeyword3, c("syntax.keyword3"));
  apply(colors.syntaxString, c("syntax.string"));
  apply(colors.syntaxComment, c("syntax.comment"));
  apply(colors.syntaxFunction, c("syntax.function"));
  apply(colors.syntaxClass, c("syntax.class"));
  apply(colors.syntaxNumber, c("syntax.number"));
  apply(colors.syntaxOperator, c("syntax.operator"));
  apply(colors.syntaxType, c("syntax.type"));
  apply(colors.syntaxConstant, c("syntax.constant"));
  apply(colors.syntaxTag, c("syntax.tag"));
  apply(colors.syntaxAttribute, c("syntax.attribute"));
  apply(colors.syntaxRegex, c("syntax.regex"));
  apply(colors.syntaxEscape, c("syntax.escape"));

  apply(colors.surfaceBase, c("surface.base"));
  apply(colors.surfaceRaised, c("surface.raised"));
  apply(colors.surfaceOverlay, c("surface.overlay"));
  apply(colors.surfaceSunken, c("surface.sunken"));
  apply(colors.surfacePopover, c("surface.popover"));

  apply(colors.borderDefault, c("border.default"));
  apply(colors.borderSubtle, c("border.subtle"));
  apply(colors.borderStrong, c("border.strong"));
  apply(colors.borderFocus, c("border.focus"));

  apply(colors.textPrimary, c("text.primary"));
  apply(colors.textSecondary, c("text.secondary"));
  apply(colors.textMuted, c("text.muted"));
  apply(colors.textDisabled, c("text.disabled"));
  apply(colors.textInverse, c("text.inverse"));
  apply(colors.textLink, c("text.link"));

  apply(colors.btnPrimaryBg, c("button.primaryBg"));
  apply(colors.btnPrimaryFg, c("button.primaryFg"));
  apply(colors.btnPrimaryHover, c("button.primaryHover"));
  apply(colors.btnPrimaryActive, c("button.primaryActive"));
  apply(colors.btnSecondaryBg, c("button.secondaryBg"));
  apply(colors.btnSecondaryFg, c("button.secondaryFg"));
  apply(colors.btnSecondaryHover, c("button.secondaryHover"));
  apply(colors.btnSecondaryActive, c("button.secondaryActive"));
  apply(colors.btnDangerBg, c("button.dangerBg"));
  apply(colors.btnDangerFg, c("button.dangerFg"));
  apply(colors.btnDangerHover, c("button.dangerHover"));
  apply(colors.btnDangerActive, c("button.dangerActive"));
  apply(colors.btnGhostHover, c("button.ghostHover"));
  apply(colors.btnGhostActive, c("button.ghostActive"));

  apply(colors.inputBg, c("input.background"));
  apply(colors.inputFg, c("input.foreground"));
  apply(colors.inputBorder, c("input.border"));
  apply(colors.inputBorderFocus, c("input.borderFocus"));
  apply(colors.inputPlaceholder, c("input.placeholder"));
  apply(colors.inputSelection, c("input.selection"));

  apply(colors.accentPrimary, c("accent.primary"));
  apply(colors.accentSoft, c("accent.soft"));
  apply(colors.accentGlow, c("accent.glow"));

  apply(colors.scrollTrack, c("scrollbar.track"));
  apply(colors.scrollThumb, c("scrollbar.thumb"));
  apply(colors.scrollThumbHover, c("scrollbar.thumbHover"));

  apply(colors.tabBg, c("tab.background"));
  apply(colors.tabActiveBg, c("tab.activeBackground"));
  apply(colors.tabActiveBorder, c("tab.activeBorder"));
  apply(colors.tabHoverBg, c("tab.hoverBackground"));
  apply(colors.tabFg, c("tab.foreground"));
  apply(colors.tabActiveFg, c("tab.activeForeground"));

  apply(colors.treeSelectedBg, c("tree.selectedBackground"));
  apply(colors.treeHoverBg, c("tree.hoverBackground"));
  apply(colors.treeIndentGuide, c("tree.indentGuide"));

  apply(colors.termBg, c("terminal.background"));
  apply(colors.termFg, c("terminal.foreground"));
  apply(colors.termCursor, c("terminal.cursor"));
  apply(colors.termSelection, c("terminal.selection"));

  apply(colors.ansiBlack, c("terminal.ansiBlack"));
  apply(colors.ansiRed, c("terminal.ansiRed"));
  apply(colors.ansiGreen, c("terminal.ansiGreen"));
  apply(colors.ansiYellow, c("terminal.ansiYellow"));
  apply(colors.ansiBlue, c("terminal.ansiBlue"));
  apply(colors.ansiMagenta, c("terminal.ansiMagenta"));
  apply(colors.ansiCyan, c("terminal.ansiCyan"));
  apply(colors.ansiWhite, c("terminal.ansiWhite"));

  apply(colors.ansiBrightBlack, c("terminal.ansiBrightBlack"));
  apply(colors.ansiBrightRed, c("terminal.ansiBrightRed"));
  apply(colors.ansiBrightGreen, c("terminal.ansiBrightGreen"));
  apply(colors.ansiBrightYellow, c("terminal.ansiBrightYellow"));
  apply(colors.ansiBrightBlue, c("terminal.ansiBrightBlue"));
  apply(colors.ansiBrightMagenta, c("terminal.ansiBrightMagenta"));
  apply(colors.ansiBrightCyan, c("terminal.ansiBrightCyan"));
  apply(colors.ansiBrightWhite, c("terminal.ansiBrightWhite"));

  apply(colors.statusSuccess, c("status.success"));
  apply(colors.statusWarning, c("status.warning"));
  apply(colors.statusError, c("status.error"));
  apply(colors.statusInfo, c("status.info"));
  apply(colors.diagnosticError, c("diagnostic.error"));
  apply(colors.diagnosticWarning, c("diagnostic.warning"));
  apply(colors.diagnosticInfo, c("diagnostic.info"));
  apply(colors.diagnosticHint, c("diagnostic.hint"));
  apply(colors.gitAdded, c("git.added"));
  apply(colors.gitModified, c("git.modified"));
  apply(colors.gitDeleted, c("git.deleted"));
  apply(colors.gitRenamed, c("git.renamed"));
  apply(colors.gitCopied, c("git.copied"));
  apply(colors.gitUntracked, c("git.untracked"));
  apply(colors.gitConflicted, c("git.conflicted"));
  apply(colors.gitIgnored, c("git.ignored"));
  apply(colors.diffAdded, c("diff.added"));
  apply(colors.diffModified, c("diff.modified"));
  apply(colors.diffRemoved, c("diff.removed"));
  apply(colors.diffConflict, c("diff.conflict"));
  apply(colors.testPassed, c("test.passed"));
  apply(colors.testFailed, c("test.failed"));
  apply(colors.testSkipped, c("test.skipped"));
  apply(colors.testRunning, c("test.running"));
  apply(colors.testQueued, c("test.queued"));
  apply(colors.debugReady, c("debug.ready"));
  apply(colors.debugStarting, c("debug.starting"));
  apply(colors.debugRunning, c("debug.running"));
  apply(colors.debugPaused, c("debug.paused"));
  apply(colors.debugError, c("debug.error"));
  apply(colors.debugBreakpoint, c("debug.breakpoint"));
  apply(colors.debugCurrentLine, c("debug.currentLine"));

  if (json.contains("ui") && json["ui"].isObject()) {
    QJsonObject uiObj = json["ui"].toObject();
    if (uiObj.contains("borderRadius") && uiObj["borderRadius"].isDouble())
      ui.borderRadius = uiObj["borderRadius"].toInt();
    if (uiObj.contains("glowIntensity") && uiObj["glowIntensity"].isDouble())
      ui.glowIntensity = uiObj["glowIntensity"].toDouble();
    if (uiObj.contains("chromeOpacity") && uiObj["chromeOpacity"].isDouble())
      ui.chromeOpacity = uiObj["chromeOpacity"].toDouble();
    if (uiObj.contains("animationSpeed") && uiObj["animationSpeed"].isString())
      ui.animationSpeed = uiObj["animationSpeed"].toString();
    if (uiObj.contains("scanlineEffect") && uiObj["scanlineEffect"].isBool())
      ui.scanlineEffect = uiObj["scanlineEffect"].toBool();
    if (uiObj.contains("panelBorders") && uiObj["panelBorders"].isBool())
      ui.panelBorders = uiObj["panelBorders"].toBool();
  }
  normalize();
}

void ThemeDefinition::write(QJsonObject &json) const {
  json["name"] = name;
  json["author"] = author;
  json["type"] = type;

  auto w = [&](const QString &key, const QColor &c) {
    writeColor(json, key, c);
  };

  w("editor.background", colors.editorBg);
  w("editor.foreground", colors.editorFg);
  w("editor.gutter", colors.editorGutter);
  w("editor.gutterForeground", colors.editorGutterFg);
  w("editor.lineHighlight", colors.editorLineHighlight);
  w("editor.selection", colors.editorSelection);
  w("editor.selectionForeground", colors.editorSelectionFg);
  w("editor.cursor", colors.editorCursor);
  w("editor.findMatch", colors.editorFindMatch);
  w("editor.findMatchActive", colors.editorFindMatchActive);
  w("editor.bracketMatch", colors.editorBracketMatch);
  w("editor.indentGuide", colors.editorIndentGuide);
  w("editor.whitespace", colors.editorWhitespace);

  w("syntax.keyword", colors.syntaxKeyword);
  w("syntax.keyword2", colors.syntaxKeyword2);
  w("syntax.keyword3", colors.syntaxKeyword3);
  w("syntax.string", colors.syntaxString);
  w("syntax.comment", colors.syntaxComment);
  w("syntax.function", colors.syntaxFunction);
  w("syntax.class", colors.syntaxClass);
  w("syntax.number", colors.syntaxNumber);
  w("syntax.operator", colors.syntaxOperator);
  w("syntax.type", colors.syntaxType);
  w("syntax.constant", colors.syntaxConstant);
  w("syntax.tag", colors.syntaxTag);
  w("syntax.attribute", colors.syntaxAttribute);
  w("syntax.regex", colors.syntaxRegex);
  w("syntax.escape", colors.syntaxEscape);

  w("surface.base", colors.surfaceBase);
  w("surface.raised", colors.surfaceRaised);
  w("surface.overlay", colors.surfaceOverlay);
  w("surface.sunken", colors.surfaceSunken);
  w("surface.popover", colors.surfacePopover);

  w("border.default", colors.borderDefault);
  w("border.subtle", colors.borderSubtle);
  w("border.strong", colors.borderStrong);
  w("border.focus", colors.borderFocus);

  w("text.primary", colors.textPrimary);
  w("text.secondary", colors.textSecondary);
  w("text.muted", colors.textMuted);
  w("text.disabled", colors.textDisabled);
  w("text.inverse", colors.textInverse);
  w("text.link", colors.textLink);

  w("button.primaryBg", colors.btnPrimaryBg);
  w("button.primaryFg", colors.btnPrimaryFg);
  w("button.primaryHover", colors.btnPrimaryHover);
  w("button.primaryActive", colors.btnPrimaryActive);
  w("button.secondaryBg", colors.btnSecondaryBg);
  w("button.secondaryFg", colors.btnSecondaryFg);
  w("button.secondaryHover", colors.btnSecondaryHover);
  w("button.secondaryActive", colors.btnSecondaryActive);
  w("button.dangerBg", colors.btnDangerBg);
  w("button.dangerFg", colors.btnDangerFg);
  w("button.dangerHover", colors.btnDangerHover);
  w("button.dangerActive", colors.btnDangerActive);
  w("button.ghostHover", colors.btnGhostHover);
  w("button.ghostActive", colors.btnGhostActive);

  w("input.background", colors.inputBg);
  w("input.foreground", colors.inputFg);
  w("input.border", colors.inputBorder);
  w("input.borderFocus", colors.inputBorderFocus);
  w("input.placeholder", colors.inputPlaceholder);
  w("input.selection", colors.inputSelection);

  w("accent.primary", colors.accentPrimary);
  w("accent.soft", colors.accentSoft);
  w("accent.glow", colors.accentGlow);

  w("scrollbar.track", colors.scrollTrack);
  w("scrollbar.thumb", colors.scrollThumb);
  w("scrollbar.thumbHover", colors.scrollThumbHover);

  w("tab.background", colors.tabBg);
  w("tab.activeBackground", colors.tabActiveBg);
  w("tab.activeBorder", colors.tabActiveBorder);
  w("tab.hoverBackground", colors.tabHoverBg);
  w("tab.foreground", colors.tabFg);
  w("tab.activeForeground", colors.tabActiveFg);

  w("tree.selectedBackground", colors.treeSelectedBg);
  w("tree.hoverBackground", colors.treeHoverBg);
  w("tree.indentGuide", colors.treeIndentGuide);

  w("terminal.background", colors.termBg);
  w("terminal.foreground", colors.termFg);
  w("terminal.cursor", colors.termCursor);
  w("terminal.selection", colors.termSelection);

  w("terminal.ansiBlack", colors.ansiBlack);
  w("terminal.ansiRed", colors.ansiRed);
  w("terminal.ansiGreen", colors.ansiGreen);
  w("terminal.ansiYellow", colors.ansiYellow);
  w("terminal.ansiBlue", colors.ansiBlue);
  w("terminal.ansiMagenta", colors.ansiMagenta);
  w("terminal.ansiCyan", colors.ansiCyan);
  w("terminal.ansiWhite", colors.ansiWhite);

  w("terminal.ansiBrightBlack", colors.ansiBrightBlack);
  w("terminal.ansiBrightRed", colors.ansiBrightRed);
  w("terminal.ansiBrightGreen", colors.ansiBrightGreen);
  w("terminal.ansiBrightYellow", colors.ansiBrightYellow);
  w("terminal.ansiBrightBlue", colors.ansiBrightBlue);
  w("terminal.ansiBrightMagenta", colors.ansiBrightMagenta);
  w("terminal.ansiBrightCyan", colors.ansiBrightCyan);
  w("terminal.ansiBrightWhite", colors.ansiBrightWhite);

  w("status.success", colors.statusSuccess);
  w("status.warning", colors.statusWarning);
  w("status.error", colors.statusError);
  w("status.info", colors.statusInfo);
  w("diagnostic.error", colors.diagnosticError);
  w("diagnostic.warning", colors.diagnosticWarning);
  w("diagnostic.info", colors.diagnosticInfo);
  w("diagnostic.hint", colors.diagnosticHint);
  w("git.added", colors.gitAdded);
  w("git.modified", colors.gitModified);
  w("git.deleted", colors.gitDeleted);
  w("git.renamed", colors.gitRenamed);
  w("git.copied", colors.gitCopied);
  w("git.untracked", colors.gitUntracked);
  w("git.conflicted", colors.gitConflicted);
  w("git.ignored", colors.gitIgnored);
  w("diff.added", colors.diffAdded);
  w("diff.modified", colors.diffModified);
  w("diff.removed", colors.diffRemoved);
  w("diff.conflict", colors.diffConflict);
  w("test.passed", colors.testPassed);
  w("test.failed", colors.testFailed);
  w("test.skipped", colors.testSkipped);
  w("test.running", colors.testRunning);
  w("test.queued", colors.testQueued);
  w("debug.ready", colors.debugReady);
  w("debug.starting", colors.debugStarting);
  w("debug.running", colors.debugRunning);
  w("debug.paused", colors.debugPaused);
  w("debug.error", colors.debugError);
  w("debug.breakpoint", colors.debugBreakpoint);
  w("debug.currentLine", colors.debugCurrentLine);

  QJsonObject uiObj;
  uiObj["borderRadius"] = ui.borderRadius;
  uiObj["glowIntensity"] = ui.glowIntensity;
  uiObj["chromeOpacity"] = ui.chromeOpacity;
  uiObj["animationSpeed"] = ui.animationSpeed;
  uiObj["scanlineEffect"] = ui.scanlineEffect;
  uiObj["panelBorders"] = ui.panelBorders;
  json["ui"] = uiObj;
}

QColor ThemeDefinition::lerp(const QColor &a, const QColor &b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
