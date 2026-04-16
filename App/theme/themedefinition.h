#ifndef THEMEDEFINITION_H
#define THEMEDEFINITION_H

#include <QColor>
#include <QJsonObject>
#include <QString>

struct Theme; // forward declare

struct ThemeColors {
  // Editor
  QColor editorBg, editorFg, editorGutter, editorGutterFg, editorLineHighlight;
  QColor editorSelection, editorSelectionFg, editorCursor;
  QColor editorFindMatch, editorFindMatchActive, editorBracketMatch;
  QColor editorIndentGuide, editorWhitespace;

  // Syntax
  QColor syntaxKeyword, syntaxKeyword2, syntaxKeyword3;
  QColor syntaxString, syntaxComment, syntaxFunction, syntaxClass;
  QColor syntaxNumber, syntaxOperator, syntaxType, syntaxConstant;
  QColor syntaxTag, syntaxAttribute, syntaxRegex, syntaxEscape;

  // Surfaces
  QColor surfaceBase, surfaceRaised, surfaceOverlay, surfaceSunken, surfacePopover;

  // Borders
  QColor borderDefault, borderSubtle, borderStrong, borderFocus;

  // Text
  QColor textPrimary, textSecondary, textMuted, textDisabled, textInverse, textLink;

  // Buttons
  QColor btnPrimaryBg, btnPrimaryFg, btnPrimaryHover, btnPrimaryActive;
  QColor btnSecondaryBg, btnSecondaryFg, btnSecondaryHover, btnSecondaryActive;
  QColor btnDangerBg, btnDangerFg, btnDangerHover, btnDangerActive;
  QColor btnGhostHover, btnGhostActive;

  // Input
  QColor inputBg, inputFg, inputBorder, inputBorderFocus, inputPlaceholder, inputSelection;

  // Accent
  QColor accentPrimary, accentSoft, accentGlow;

  // Scrollbar
  QColor scrollTrack, scrollThumb, scrollThumbHover;

  // Tab
  QColor tabBg, tabActiveBg, tabActiveBorder, tabHoverBg, tabFg, tabActiveFg;

  // Tree
  QColor treeSelectedBg, treeHoverBg, treeIndentGuide;

  // Terminal (ANSI)
  QColor termBg, termFg, termCursor, termSelection;
  QColor ansiBlack, ansiRed, ansiGreen, ansiYellow, ansiBlue, ansiMagenta, ansiCyan, ansiWhite;
  QColor ansiBrightBlack, ansiBrightRed, ansiBrightGreen, ansiBrightYellow;
  QColor ansiBrightBlue, ansiBrightMagenta, ansiBrightCyan, ansiBrightWhite;

  // Status
  QColor statusSuccess, statusWarning, statusError, statusInfo;
};

struct ThemeUiConfig {
  int borderRadius = 6;
  qreal glowIntensity = 0.3;
  QString animationSpeed = "normal"; // "off", "subtle", "normal", "fancy"
  bool scanlineEffect = false;
};

class ThemeDefinition {
public:
  QString name;
  QString author;
  QString type; // "dark" or "light"

  ThemeColors colors;
  ThemeUiConfig ui;

  ThemeDefinition();

  // Convert to legacy Theme for backward compat
  Theme toClassicTheme() const;

  // Build from legacy Theme
  static ThemeDefinition fromClassicTheme(const Theme &classic, const QString &name = "Custom");

  // JSON serialization
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;

  // Color interpolation helper
  static QColor lerp(const QColor &a, const QColor &b, qreal t);
};

#endif
