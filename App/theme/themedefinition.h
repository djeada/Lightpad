#ifndef THEMEDEFINITION_H
#define THEMEDEFINITION_H

#include <QColor>
#include <QJsonObject>
#include <QString>

struct Theme;

struct ThemeColors {

  QColor editorBg, editorFg, editorGutter, editorGutterFg, editorLineHighlight;
  QColor editorSelection, editorSelectionFg, editorCursor;
  QColor editorFindMatch, editorFindMatchActive, editorBracketMatch;
  QColor editorIndentGuide, editorWhitespace;

  QColor syntaxKeyword, syntaxKeyword2, syntaxKeyword3;
  QColor syntaxString, syntaxComment, syntaxFunction, syntaxClass;
  QColor syntaxNumber, syntaxOperator, syntaxType, syntaxConstant;
  QColor syntaxTag, syntaxAttribute, syntaxRegex, syntaxEscape;

  QColor surfaceBase, surfaceRaised, surfaceOverlay, surfaceSunken,
      surfacePopover;

  QColor borderDefault, borderSubtle, borderStrong, borderFocus;

  QColor textPrimary, textSecondary, textMuted, textDisabled, textInverse,
      textLink;

  QColor btnPrimaryBg, btnPrimaryFg, btnPrimaryHover, btnPrimaryActive;
  QColor btnSecondaryBg, btnSecondaryFg, btnSecondaryHover, btnSecondaryActive;
  QColor btnDangerBg, btnDangerFg, btnDangerHover, btnDangerActive;
  QColor btnGhostHover, btnGhostActive;

  QColor inputBg, inputFg, inputBorder, inputBorderFocus, inputPlaceholder,
      inputSelection;

  QColor accentPrimary, accentSoft, accentGlow;

  QColor scrollTrack, scrollThumb, scrollThumbHover;

  QColor tabBg, tabActiveBg, tabActiveBorder, tabHoverBg, tabFg, tabActiveFg;

  QColor treeSelectedBg, treeHoverBg, treeIndentGuide;

  QColor termBg, termFg, termCursor, termSelection;
  QColor ansiBlack, ansiRed, ansiGreen, ansiYellow, ansiBlue, ansiMagenta,
      ansiCyan, ansiWhite;
  QColor ansiBrightBlack, ansiBrightRed, ansiBrightGreen, ansiBrightYellow;
  QColor ansiBrightBlue, ansiBrightMagenta, ansiBrightCyan, ansiBrightWhite;

  QColor statusSuccess, statusWarning, statusError, statusInfo;
};

struct ThemeUiConfig {
  int borderRadius = 6;
  qreal glowIntensity = 0.3;
  qreal chromeOpacity = 1.0;
  QString animationSpeed = "normal";
  bool scanlineEffect = false;
  bool panelBorders = true;
};

class ThemeDefinition {
public:
  QString name;
  QString author;
  QString type;

  ThemeColors colors;
  ThemeUiConfig ui;

  ThemeDefinition();

  Theme toClassicTheme() const;

  static ThemeDefinition fromClassicTheme(const Theme &classic,
                                          const QString &name = "Custom");

  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;

  static QColor lerp(const QColor &a, const QColor &b, qreal t);
};

#endif
