#include "themedefinition.h"
#include "../settings/theme.h"

#include <QJsonArray>

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
  ui.animationSpeed = "normal";
  ui.scanlineEffect = false;
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
  d.colors.statusInfo = c.accentColor;

  return d;
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

  if (json.contains("ui") && json["ui"].isObject()) {
    QJsonObject uiObj = json["ui"].toObject();
    if (uiObj.contains("borderRadius") && uiObj["borderRadius"].isDouble())
      ui.borderRadius = uiObj["borderRadius"].toInt();
    if (uiObj.contains("glowIntensity") && uiObj["glowIntensity"].isDouble())
      ui.glowIntensity = uiObj["glowIntensity"].toDouble();
    if (uiObj.contains("animationSpeed") && uiObj["animationSpeed"].isString())
      ui.animationSpeed = uiObj["animationSpeed"].toString();
    if (uiObj.contains("scanlineEffect") && uiObj["scanlineEffect"].isBool())
      ui.scanlineEffect = uiObj["scanlineEffect"].toBool();
  }
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

  QJsonObject uiObj;
  uiObj["borderRadius"] = ui.borderRadius;
  uiObj["glowIntensity"] = ui.glowIntensity;
  uiObj["animationSpeed"] = ui.animationSpeed;
  uiObj["scanlineEffect"] = ui.scanlineEffect;
  json["ui"] = uiObj;
}

QColor ThemeDefinition::lerp(const QColor &a, const QColor &b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
