#include "themepresets.h"
#include "../settings/theme.h"

namespace ThemePresets {

ThemeDefinition hackerDark() { return ThemeDefinition(); }

ThemeDefinition minimalDark() {
  Theme classic;
  classic.backgroundColor = QColor("#0a0b0d");
  classic.foregroundColor = QColor("#d7dde4");
  classic.highlightColor = QColor("#101318");
  classic.lineNumberAreaColor = QColor("#08090b");
  classic.keywordFormat_0 = QColor("#8ab4f8");
  classic.keywordFormat_1 = QColor("#c58af9");
  classic.keywordFormat_2 = QColor("#7dd3c7");
  classic.searchFormat = QColor("#d8b45f55");
  classic.singleLineCommentFormat = QColor("#68707a");
  classic.functionFormat = QColor("#93c5fd");
  classic.quotationFormat = QColor("#a7c080");
  classic.classFormat = QColor("#7dd3c7");
  classic.numberFormat = QColor("#e5b567");
  classic.surfaceColor = QColor("#111418");
  classic.surfaceAltColor = QColor("#171b21");
  classic.borderColor = QColor("#242a32");
  classic.hoverColor = QColor("#171d24");
  classic.pressedColor = QColor("#1e2630");
  classic.accentColor = QColor("#7dd3fc");
  classic.accentSoftColor = QColor("#10283a");
  classic.successColor = QColor("#86efac");
  classic.warningColor = QColor("#facc15");
  classic.errorColor = QColor("#f87171");

  ThemeDefinition t =
      ThemeDefinition::fromClassicTheme(classic, "Minimal Dark");
  t.author = "Lightpad";
  t.colors.termBg = QColor("#050607");
  t.colors.termFg = QColor("#d7dde4");
  t.ui.borderRadius = 4;
  t.ui.glowIntensity = 0.0;
  t.ui.chromeOpacity = 1.0;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;
  t.ui.panelBorders = false;
  return t;
}

ThemeDefinition githubDark() {
  ThemeDefinition t = midnightBlue();
  t.name = "GitHub Dark";
  t.ui.glowIntensity = 0.0;
  t.ui.chromeOpacity = 1.0;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;
  t.ui.panelBorders = true;
  return t;
}

ThemeDefinition midnightBlue() {
  ThemeDefinition t;
  t.name = "Midnight Blue";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#0d1117");
  c.editorFg = QColor("#e6edf3");
  c.editorGutter = QColor("#0d1117");
  c.editorGutterFg = QColor("#484f58");
  c.editorLineHighlight = QColor("#1a2230");
  c.editorSelection = QColor("#1f3a5f");
  c.editorSelectionFg = QColor("#e6edf3");
  c.editorCursor = QColor("#58a6ff");
  c.editorFindMatch = QColor("#f2cc6040");
  c.editorFindMatchActive = QColor("#f2cc6090");
  c.editorBracketMatch = QColor("#58a6ff40");
  c.editorIndentGuide = QColor("#21262d");
  c.editorWhitespace = QColor("#30363d");

  c.syntaxKeyword = QColor("#7ee787");
  c.syntaxKeyword2 = QColor("#f2cc60");
  c.syntaxKeyword3 = QColor("#58a6ff");
  c.syntaxString = QColor("#a5d6ff");
  c.syntaxComment = QColor("#8b949e");
  c.syntaxFunction = QColor("#79c0ff");
  c.syntaxClass = QColor("#56d4dd");
  c.syntaxNumber = QColor("#ff7b72");
  c.syntaxOperator = QColor("#e6edf3");
  c.syntaxType = QColor("#56d4dd");
  c.syntaxConstant = QColor("#79c0ff");
  c.syntaxTag = QColor("#7ee787");
  c.syntaxAttribute = QColor("#79c0ff");
  c.syntaxRegex = QColor("#a5d6ff");
  c.syntaxEscape = QColor("#56d4dd");

  c.surfaceBase = QColor("#0d1117");
  c.surfaceRaised = QColor("#161b22");
  c.surfaceOverlay = QColor("#1c2128");
  c.surfaceSunken = QColor("#090c10");
  c.surfacePopover = QColor("#1c2128");

  c.borderDefault = QColor("#30363d");
  c.borderSubtle = QColor("#21262d");
  c.borderStrong = QColor("#484f58");
  c.borderFocus = QColor("#58a6ff");

  c.textPrimary = QColor("#e6edf3");
  c.textSecondary = QColor("#8b949e");
  c.textMuted = QColor("#6e7681");
  c.textDisabled = QColor("#484f58");
  c.textInverse = QColor("#0d1117");
  c.textLink = QColor("#58a6ff");

  c.btnPrimaryBg = QColor("#58a6ff");
  c.btnPrimaryFg = QColor("#0d1117");
  c.btnPrimaryHover = QColor("#79b8ff");
  c.btnPrimaryActive = QColor("#3d8bfd");
  c.btnSecondaryBg = QColor("#21262d");
  c.btnSecondaryFg = QColor("#e6edf3");
  c.btnSecondaryHover = QColor("#30363d");
  c.btnSecondaryActive = QColor("#1c2128");
  c.btnDangerBg = QColor("#f85149");
  c.btnDangerFg = QColor("#ffffff");
  c.btnDangerHover = QColor("#fa6e68");
  c.btnDangerActive = QColor("#d63d36");
  c.btnGhostHover = QColor("#21262d");
  c.btnGhostActive = QColor("#2d333b");

  c.inputBg = QColor("#161b22");
  c.inputFg = QColor("#e6edf3");
  c.inputBorder = QColor("#30363d");
  c.inputBorderFocus = QColor("#58a6ff");
  c.inputPlaceholder = QColor("#6e7681");
  c.inputSelection = QColor("#1f3a5f");

  c.accentPrimary = QColor("#58a6ff");
  c.accentSoft = QColor("#1f3a5f");
  c.accentGlow = QColor("#58a6ff40");

  c.scrollTrack = QColor("#0d1117");
  c.scrollThumb = QColor("#30363d");
  c.scrollThumbHover = QColor("#484f58");

  c.tabBg = QColor("#0d1117");
  c.tabActiveBg = QColor("#161b22");
  c.tabActiveBorder = QColor("#58a6ff");
  c.tabHoverBg = QColor("#131920");
  c.tabFg = QColor("#8b949e");
  c.tabActiveFg = QColor("#e6edf3");

  c.treeSelectedBg = QColor("#1a2332");
  c.treeHoverBg = QColor("#131920");
  c.treeIndentGuide = QColor("#21262d");

  c.termBg = QColor("#0d1117");
  c.termFg = QColor("#e6edf3");
  c.termCursor = QColor("#58a6ff");
  c.termSelection = QColor("#1f3a5f");

  c.ansiBlack = QColor("#0d1117");
  c.ansiRed = QColor("#ff7b72");
  c.ansiGreen = QColor("#7ee787");
  c.ansiYellow = QColor("#f2cc60");
  c.ansiBlue = QColor("#58a6ff");
  c.ansiMagenta = QColor("#bc8cff");
  c.ansiCyan = QColor("#56d4dd");
  c.ansiWhite = QColor("#e6edf3");

  c.ansiBrightBlack = QColor("#6e7681");
  c.ansiBrightRed = QColor("#ffa198");
  c.ansiBrightGreen = QColor("#aff5b4");
  c.ansiBrightYellow = QColor("#f8e3a1");
  c.ansiBrightBlue = QColor("#a5d6ff");
  c.ansiBrightMagenta = QColor("#d2a8ff");
  c.ansiBrightCyan = QColor("#b3f0ff");
  c.ansiBrightWhite = QColor("#f0f6fc");

  c.statusSuccess = QColor("#3fb950");
  c.statusWarning = QColor("#d29922");
  c.statusError = QColor("#f85149");
  c.statusInfo = QColor("#58a6ff");

  t.ui.borderRadius = 6;
  t.ui.glowIntensity = 0.2;
  t.ui.animationSpeed = "normal";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition dracula() {
  ThemeDefinition t;
  t.name = "Dracula";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#282a36");
  c.editorFg = QColor("#f8f8f2");
  c.editorGutter = QColor("#282a36");
  c.editorGutterFg = QColor("#6272a4");
  c.editorLineHighlight = QColor("#44475a");
  c.editorSelection = QColor("#44475a");
  c.editorSelectionFg = QColor("#f8f8f2");
  c.editorCursor = QColor("#f8f8f2");
  c.editorFindMatch = QColor("#ffb86c40");
  c.editorFindMatchActive = QColor("#ffb86c80");
  c.editorBracketMatch = QColor("#bd93f940");
  c.editorIndentGuide = QColor("#3b3d4f");
  c.editorWhitespace = QColor("#4d4f66");

  c.syntaxKeyword = QColor("#ff79c6");
  c.syntaxKeyword2 = QColor("#bd93f9");
  c.syntaxKeyword3 = QColor("#ff79c6");
  c.syntaxString = QColor("#f1fa8c");
  c.syntaxComment = QColor("#6272a4");
  c.syntaxFunction = QColor("#50fa7b");
  c.syntaxClass = QColor("#8be9fd");
  c.syntaxNumber = QColor("#bd93f9");
  c.syntaxOperator = QColor("#ff79c6");
  c.syntaxType = QColor("#8be9fd");
  c.syntaxConstant = QColor("#bd93f9");
  c.syntaxTag = QColor("#ff79c6");
  c.syntaxAttribute = QColor("#50fa7b");
  c.syntaxRegex = QColor("#f1fa8c");
  c.syntaxEscape = QColor("#ffb86c");

  c.surfaceBase = QColor("#282a36");
  c.surfaceRaised = QColor("#44475a");
  c.surfaceOverlay = QColor("#4d5066");
  c.surfaceSunken = QColor("#21222c");
  c.surfacePopover = QColor("#44475a");

  c.borderDefault = QColor("#6272a4");
  c.borderSubtle = QColor("#44475a");
  c.borderStrong = QColor("#7c8ab8");
  c.borderFocus = QColor("#bd93f9");

  c.textPrimary = QColor("#f8f8f2");
  c.textSecondary = QColor("#bfc0cc");
  c.textMuted = QColor("#6272a4");
  c.textDisabled = QColor("#4d5066");
  c.textInverse = QColor("#282a36");
  c.textLink = QColor("#8be9fd");

  c.btnPrimaryBg = QColor("#bd93f9");
  c.btnPrimaryFg = QColor("#282a36");
  c.btnPrimaryHover = QColor("#caa4fa");
  c.btnPrimaryActive = QColor("#a87ef0");
  c.btnSecondaryBg = QColor("#44475a");
  c.btnSecondaryFg = QColor("#f8f8f2");
  c.btnSecondaryHover = QColor("#4d5066");
  c.btnSecondaryActive = QColor("#3b3d4f");
  c.btnDangerBg = QColor("#ff5555");
  c.btnDangerFg = QColor("#f8f8f2");
  c.btnDangerHover = QColor("#ff6e6e");
  c.btnDangerActive = QColor("#e04040");
  c.btnGhostHover = QColor("#383a4a");
  c.btnGhostActive = QColor("#44475a");

  c.inputBg = QColor("#383a4a");
  c.inputFg = QColor("#f8f8f2");
  c.inputBorder = QColor("#6272a4");
  c.inputBorderFocus = QColor("#bd93f9");
  c.inputPlaceholder = QColor("#6272a4");
  c.inputSelection = QColor("#44475a");

  c.accentPrimary = QColor("#bd93f9");
  c.accentSoft = QColor("#3a2d5a");
  c.accentGlow = QColor("#bd93f940");

  c.scrollTrack = QColor("#282a36");
  c.scrollThumb = QColor("#44475a");
  c.scrollThumbHover = QColor("#6272a4");

  c.tabBg = QColor("#21222c");
  c.tabActiveBg = QColor("#282a36");
  c.tabActiveBorder = QColor("#bd93f9");
  c.tabHoverBg = QColor("#2c2e3a");
  c.tabFg = QColor("#6272a4");
  c.tabActiveFg = QColor("#f8f8f2");

  c.treeSelectedBg = QColor("#383a4a");
  c.treeHoverBg = QColor("#2c2e3a");
  c.treeIndentGuide = QColor("#3b3d4f");

  c.termBg = QColor("#282a36");
  c.termFg = QColor("#f8f8f2");
  c.termCursor = QColor("#f8f8f2");
  c.termSelection = QColor("#44475a");

  c.ansiBlack = QColor("#21222c");
  c.ansiRed = QColor("#ff5555");
  c.ansiGreen = QColor("#50fa7b");
  c.ansiYellow = QColor("#f1fa8c");
  c.ansiBlue = QColor("#bd93f9");
  c.ansiMagenta = QColor("#ff79c6");
  c.ansiCyan = QColor("#8be9fd");
  c.ansiWhite = QColor("#f8f8f2");

  c.ansiBrightBlack = QColor("#6272a4");
  c.ansiBrightRed = QColor("#ff6e6e");
  c.ansiBrightGreen = QColor("#69ff94");
  c.ansiBrightYellow = QColor("#ffffa5");
  c.ansiBrightBlue = QColor("#d6acff");
  c.ansiBrightMagenta = QColor("#ff92df");
  c.ansiBrightCyan = QColor("#a4ffff");
  c.ansiBrightWhite = QColor("#ffffff");

  c.statusSuccess = QColor("#50fa7b");
  c.statusWarning = QColor("#ffb86c");
  c.statusError = QColor("#ff5555");
  c.statusInfo = QColor("#8be9fd");

  t.ui.borderRadius = 6;
  t.ui.glowIntensity = 0.25;
  t.ui.animationSpeed = "normal";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition monokaiPro() {
  ThemeDefinition t;
  t.name = "Monokai Pro";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#2d2a2e");
  c.editorFg = QColor("#fcfcfa");
  c.editorGutter = QColor("#2d2a2e");
  c.editorGutterFg = QColor("#5b595c");
  c.editorLineHighlight = QColor("#403e41");
  c.editorSelection = QColor("#403e41");
  c.editorSelectionFg = QColor("#fcfcfa");
  c.editorCursor = QColor("#fcfcfa");
  c.editorFindMatch = QColor("#ffd86640");
  c.editorFindMatchActive = QColor("#ffd86680");
  c.editorBracketMatch = QColor("#ffd86640");
  c.editorIndentGuide = QColor("#3a383b");
  c.editorWhitespace = QColor("#4a484b");

  c.syntaxKeyword = QColor("#ff6188");
  c.syntaxKeyword2 = QColor("#ab9df2");
  c.syntaxKeyword3 = QColor("#ff6188");
  c.syntaxString = QColor("#ffd866");
  c.syntaxComment = QColor("#727072");
  c.syntaxFunction = QColor("#a9dc76");
  c.syntaxClass = QColor("#78dce8");
  c.syntaxNumber = QColor("#ab9df2");
  c.syntaxOperator = QColor("#ff6188");
  c.syntaxType = QColor("#78dce8");
  c.syntaxConstant = QColor("#ab9df2");
  c.syntaxTag = QColor("#ff6188");
  c.syntaxAttribute = QColor("#a9dc76");
  c.syntaxRegex = QColor("#ffd866");
  c.syntaxEscape = QColor("#78dce8");

  c.surfaceBase = QColor("#2d2a2e");
  c.surfaceRaised = QColor("#403e41");
  c.surfaceOverlay = QColor("#4a484b");
  c.surfaceSunken = QColor("#221f22");
  c.surfacePopover = QColor("#403e41");

  c.borderDefault = QColor("#5b595c");
  c.borderSubtle = QColor("#403e41");
  c.borderStrong = QColor("#727072");
  c.borderFocus = QColor("#ffd866");

  c.textPrimary = QColor("#fcfcfa");
  c.textSecondary = QColor("#c1c0c0");
  c.textMuted = QColor("#727072");
  c.textDisabled = QColor("#5b595c");
  c.textInverse = QColor("#2d2a2e");
  c.textLink = QColor("#78dce8");

  c.btnPrimaryBg = QColor("#ffd866");
  c.btnPrimaryFg = QColor("#2d2a2e");
  c.btnPrimaryHover = QColor("#ffe08a");
  c.btnPrimaryActive = QColor("#e6c24f");
  c.btnSecondaryBg = QColor("#403e41");
  c.btnSecondaryFg = QColor("#fcfcfa");
  c.btnSecondaryHover = QColor("#4a484b");
  c.btnSecondaryActive = QColor("#3a383b");
  c.btnDangerBg = QColor("#ff6188");
  c.btnDangerFg = QColor("#ffffff");
  c.btnDangerHover = QColor("#ff7ea0");
  c.btnDangerActive = QColor("#e6506e");
  c.btnGhostHover = QColor("#363337");
  c.btnGhostActive = QColor("#403e41");

  c.inputBg = QColor("#353336");
  c.inputFg = QColor("#fcfcfa");
  c.inputBorder = QColor("#5b595c");
  c.inputBorderFocus = QColor("#ffd866");
  c.inputPlaceholder = QColor("#727072");
  c.inputSelection = QColor("#403e41");

  c.accentPrimary = QColor("#ffd866");
  c.accentSoft = QColor("#4a4230");
  c.accentGlow = QColor("#ffd86640");

  c.scrollTrack = QColor("#2d2a2e");
  c.scrollThumb = QColor("#5b595c");
  c.scrollThumbHover = QColor("#727072");

  c.tabBg = QColor("#221f22");
  c.tabActiveBg = QColor("#2d2a2e");
  c.tabActiveBorder = QColor("#ffd866");
  c.tabHoverBg = QColor("#2a272b");
  c.tabFg = QColor("#727072");
  c.tabActiveFg = QColor("#fcfcfa");

  c.treeSelectedBg = QColor("#403e41");
  c.treeHoverBg = QColor("#353336");
  c.treeIndentGuide = QColor("#3a383b");

  c.termBg = QColor("#2d2a2e");
  c.termFg = QColor("#fcfcfa");
  c.termCursor = QColor("#fcfcfa");
  c.termSelection = QColor("#403e41");

  c.ansiBlack = QColor("#2d2a2e");
  c.ansiRed = QColor("#ff6188");
  c.ansiGreen = QColor("#a9dc76");
  c.ansiYellow = QColor("#ffd866");
  c.ansiBlue = QColor("#78dce8");
  c.ansiMagenta = QColor("#ab9df2");
  c.ansiCyan = QColor("#78dce8");
  c.ansiWhite = QColor("#fcfcfa");

  c.ansiBrightBlack = QColor("#727072");
  c.ansiBrightRed = QColor("#ff7ea0");
  c.ansiBrightGreen = QColor("#bde89e");
  c.ansiBrightYellow = QColor("#ffe08a");
  c.ansiBrightBlue = QColor("#9ae5f0");
  c.ansiBrightMagenta = QColor("#c1b3f5");
  c.ansiBrightCyan = QColor("#9ae5f0");
  c.ansiBrightWhite = QColor("#ffffff");

  c.statusSuccess = QColor("#a9dc76");
  c.statusWarning = QColor("#ffd866");
  c.statusError = QColor("#ff6188");
  c.statusInfo = QColor("#78dce8");

  t.ui.borderRadius = 6;
  t.ui.glowIntensity = 0.15;
  t.ui.animationSpeed = "normal";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition nord() {
  ThemeDefinition t;
  t.name = "Nord";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#2e3440");
  c.editorFg = QColor("#eceff4");
  c.editorGutter = QColor("#2e3440");
  c.editorGutterFg = QColor("#4c566a");
  c.editorLineHighlight = QColor("#3b4252");
  c.editorSelection = QColor("#434c5e");
  c.editorSelectionFg = QColor("#eceff4");
  c.editorCursor = QColor("#d8dee9");
  c.editorFindMatch = QColor("#ebcb8b40");
  c.editorFindMatchActive = QColor("#ebcb8b80");
  c.editorBracketMatch = QColor("#88c0d040");
  c.editorIndentGuide = QColor("#3b4252");
  c.editorWhitespace = QColor("#434c5e");

  c.syntaxKeyword = QColor("#81a1c1");
  c.syntaxKeyword2 = QColor("#b48ead");
  c.syntaxKeyword3 = QColor("#81a1c1");
  c.syntaxString = QColor("#a3be8c");
  c.syntaxComment = QColor("#4c566a");
  c.syntaxFunction = QColor("#88c0d0");
  c.syntaxClass = QColor("#8fbcbb");
  c.syntaxNumber = QColor("#b48ead");
  c.syntaxOperator = QColor("#81a1c1");
  c.syntaxType = QColor("#8fbcbb");
  c.syntaxConstant = QColor("#5e81ac");
  c.syntaxTag = QColor("#81a1c1");
  c.syntaxAttribute = QColor("#8fbcbb");
  c.syntaxRegex = QColor("#ebcb8b");
  c.syntaxEscape = QColor("#d08770");

  c.surfaceBase = QColor("#2e3440");
  c.surfaceRaised = QColor("#3b4252");
  c.surfaceOverlay = QColor("#434c5e");
  c.surfaceSunken = QColor("#272c36");
  c.surfacePopover = QColor("#3b4252");

  c.borderDefault = QColor("#4c566a");
  c.borderSubtle = QColor("#3b4252");
  c.borderStrong = QColor("#5e6a82");
  c.borderFocus = QColor("#88c0d0");

  c.textPrimary = QColor("#eceff4");
  c.textSecondary = QColor("#d8dee9");
  c.textMuted = QColor("#4c566a");
  c.textDisabled = QColor("#434c5e");
  c.textInverse = QColor("#2e3440");
  c.textLink = QColor("#88c0d0");

  c.btnPrimaryBg = QColor("#88c0d0");
  c.btnPrimaryFg = QColor("#2e3440");
  c.btnPrimaryHover = QColor("#9ecfde");
  c.btnPrimaryActive = QColor("#70b0c0");
  c.btnSecondaryBg = QColor("#3b4252");
  c.btnSecondaryFg = QColor("#eceff4");
  c.btnSecondaryHover = QColor("#434c5e");
  c.btnSecondaryActive = QColor("#353c4a");
  c.btnDangerBg = QColor("#bf616a");
  c.btnDangerFg = QColor("#eceff4");
  c.btnDangerHover = QColor("#cc7a82");
  c.btnDangerActive = QColor("#a3545c");
  c.btnGhostHover = QColor("#353c4a");
  c.btnGhostActive = QColor("#3b4252");

  c.inputBg = QColor("#3b4252");
  c.inputFg = QColor("#eceff4");
  c.inputBorder = QColor("#4c566a");
  c.inputBorderFocus = QColor("#88c0d0");
  c.inputPlaceholder = QColor("#4c566a");
  c.inputSelection = QColor("#434c5e");

  c.accentPrimary = QColor("#88c0d0");
  c.accentSoft = QColor("#2e4450");
  c.accentGlow = QColor("#88c0d040");

  c.scrollTrack = QColor("#2e3440");
  c.scrollThumb = QColor("#4c566a");
  c.scrollThumbHover = QColor("#5e6a82");

  c.tabBg = QColor("#272c36");
  c.tabActiveBg = QColor("#2e3440");
  c.tabActiveBorder = QColor("#88c0d0");
  c.tabHoverBg = QColor("#313844");
  c.tabFg = QColor("#4c566a");
  c.tabActiveFg = QColor("#eceff4");

  c.treeSelectedBg = QColor("#3b4252");
  c.treeHoverBg = QColor("#353c4a");
  c.treeIndentGuide = QColor("#3b4252");

  c.termBg = QColor("#2e3440");
  c.termFg = QColor("#eceff4");
  c.termCursor = QColor("#d8dee9");
  c.termSelection = QColor("#434c5e");

  c.ansiBlack = QColor("#3b4252");
  c.ansiRed = QColor("#bf616a");
  c.ansiGreen = QColor("#a3be8c");
  c.ansiYellow = QColor("#ebcb8b");
  c.ansiBlue = QColor("#81a1c1");
  c.ansiMagenta = QColor("#b48ead");
  c.ansiCyan = QColor("#88c0d0");
  c.ansiWhite = QColor("#e5e9f0");

  c.ansiBrightBlack = QColor("#4c566a");
  c.ansiBrightRed = QColor("#d08770");
  c.ansiBrightGreen = QColor("#b4d0a0");
  c.ansiBrightYellow = QColor("#f0d8a0");
  c.ansiBrightBlue = QColor("#9bb5d0");
  c.ansiBrightMagenta = QColor("#c8a0c0");
  c.ansiBrightCyan = QColor("#a0d0dd");
  c.ansiBrightWhite = QColor("#eceff4");

  c.statusSuccess = QColor("#a3be8c");
  c.statusWarning = QColor("#ebcb8b");
  c.statusError = QColor("#bf616a");
  c.statusInfo = QColor("#88c0d0");

  t.ui.borderRadius = 6;
  t.ui.glowIntensity = 0.1;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition solarizedDark() {
  ThemeDefinition t;
  t.name = "Solarized Dark";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#002b36");
  c.editorFg = QColor("#839496");
  c.editorGutter = QColor("#002b36");
  c.editorGutterFg = QColor("#586e75");
  c.editorLineHighlight = QColor("#073642");
  c.editorSelection = QColor("#073642");
  c.editorSelectionFg = QColor("#93a1a1");
  c.editorCursor = QColor("#839496");
  c.editorFindMatch = QColor("#b5890040");
  c.editorFindMatchActive = QColor("#b5890080");
  c.editorBracketMatch = QColor("#268bd240");
  c.editorIndentGuide = QColor("#073642");
  c.editorWhitespace = QColor("#0a4050");

  c.syntaxKeyword = QColor("#859900");
  c.syntaxKeyword2 = QColor("#cb4b16");
  c.syntaxKeyword3 = QColor("#859900");
  c.syntaxString = QColor("#2aa198");
  c.syntaxComment = QColor("#586e75");
  c.syntaxFunction = QColor("#268bd2");
  c.syntaxClass = QColor("#b58900");
  c.syntaxNumber = QColor("#d33682");
  c.syntaxOperator = QColor("#859900");
  c.syntaxType = QColor("#b58900");
  c.syntaxConstant = QColor("#cb4b16");
  c.syntaxTag = QColor("#268bd2");
  c.syntaxAttribute = QColor("#93a1a1");
  c.syntaxRegex = QColor("#dc322f");
  c.syntaxEscape = QColor("#cb4b16");

  c.surfaceBase = QColor("#002b36");
  c.surfaceRaised = QColor("#073642");
  c.surfaceOverlay = QColor("#0a4050");
  c.surfaceSunken = QColor("#00212b");
  c.surfacePopover = QColor("#073642");

  c.borderDefault = QColor("#586e75");
  c.borderSubtle = QColor("#073642");
  c.borderStrong = QColor("#657b83");
  c.borderFocus = QColor("#268bd2");

  c.textPrimary = QColor("#839496");
  c.textSecondary = QColor("#657b83");
  c.textMuted = QColor("#586e75");
  c.textDisabled = QColor("#405060");
  c.textInverse = QColor("#002b36");
  c.textLink = QColor("#268bd2");

  c.btnPrimaryBg = QColor("#268bd2");
  c.btnPrimaryFg = QColor("#fdf6e3");
  c.btnPrimaryHover = QColor("#3da0e0");
  c.btnPrimaryActive = QColor("#1a7abb");
  c.btnSecondaryBg = QColor("#073642");
  c.btnSecondaryFg = QColor("#839496");
  c.btnSecondaryHover = QColor("#0a4050");
  c.btnSecondaryActive = QColor("#063040");
  c.btnDangerBg = QColor("#dc322f");
  c.btnDangerFg = QColor("#fdf6e3");
  c.btnDangerHover = QColor("#e54d4a");
  c.btnDangerActive = QColor("#c02825");
  c.btnGhostHover = QColor("#063040");
  c.btnGhostActive = QColor("#073642");

  c.inputBg = QColor("#073642");
  c.inputFg = QColor("#839496");
  c.inputBorder = QColor("#586e75");
  c.inputBorderFocus = QColor("#268bd2");
  c.inputPlaceholder = QColor("#586e75");
  c.inputSelection = QColor("#073642");

  c.accentPrimary = QColor("#268bd2");
  c.accentSoft = QColor("#0a3050");
  c.accentGlow = QColor("#268bd240");

  c.scrollTrack = QColor("#002b36");
  c.scrollThumb = QColor("#073642");
  c.scrollThumbHover = QColor("#586e75");

  c.tabBg = QColor("#00212b");
  c.tabActiveBg = QColor("#002b36");
  c.tabActiveBorder = QColor("#268bd2");
  c.tabHoverBg = QColor("#02303c");
  c.tabFg = QColor("#586e75");
  c.tabActiveFg = QColor("#839496");

  c.treeSelectedBg = QColor("#073642");
  c.treeHoverBg = QColor("#063040");
  c.treeIndentGuide = QColor("#073642");

  c.termBg = QColor("#002b36");
  c.termFg = QColor("#839496");
  c.termCursor = QColor("#839496");
  c.termSelection = QColor("#073642");

  c.ansiBlack = QColor("#073642");
  c.ansiRed = QColor("#dc322f");
  c.ansiGreen = QColor("#859900");
  c.ansiYellow = QColor("#b58900");
  c.ansiBlue = QColor("#268bd2");
  c.ansiMagenta = QColor("#d33682");
  c.ansiCyan = QColor("#2aa198");
  c.ansiWhite = QColor("#eee8d5");

  c.ansiBrightBlack = QColor("#586e75");
  c.ansiBrightRed = QColor("#cb4b16");
  c.ansiBrightGreen = QColor("#93a34a");
  c.ansiBrightYellow = QColor("#c8a000");
  c.ansiBrightBlue = QColor("#3da0e0");
  c.ansiBrightMagenta = QColor("#6c71c4");
  c.ansiBrightCyan = QColor("#40b8b0");
  c.ansiBrightWhite = QColor("#fdf6e3");

  c.statusSuccess = QColor("#859900");
  c.statusWarning = QColor("#b58900");
  c.statusError = QColor("#dc322f");
  c.statusInfo = QColor("#268bd2");

  t.ui.borderRadius = 4;
  t.ui.glowIntensity = 0.1;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition cyberpunk() {
  ThemeDefinition t;
  t.name = "Cyberpunk";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#0a0015");
  c.editorFg = QColor("#e0d0f0");
  c.editorGutter = QColor("#0a0015");
  c.editorGutterFg = QColor("#4a3f5c");
  c.editorLineHighlight = QColor("#150025");
  c.editorSelection = QColor("#2a1040");
  c.editorSelectionFg = QColor("#f0e0ff");
  c.editorCursor = QColor("#ff2d6f");
  c.editorFindMatch = QColor("#ff2d6f40");
  c.editorFindMatchActive = QColor("#ff2d6f80");
  c.editorBracketMatch = QColor("#00d4ff40");
  c.editorIndentGuide = QColor("#1a0a2a");
  c.editorWhitespace = QColor("#2a1540");

  c.syntaxKeyword = QColor("#ff2d6f");
  c.syntaxKeyword2 = QColor("#ff9500");
  c.syntaxKeyword3 = QColor("#ff2d6f");
  c.syntaxString = QColor("#00d4ff");
  c.syntaxComment = QColor("#4a3f5c");
  c.syntaxFunction = QColor("#f7d731");
  c.syntaxClass = QColor("#00d4ff");
  c.syntaxNumber = QColor("#ff9500");
  c.syntaxOperator = QColor("#ff2d6f");
  c.syntaxType = QColor("#00d4ff");
  c.syntaxConstant = QColor("#ff9500");
  c.syntaxTag = QColor("#ff2d6f");
  c.syntaxAttribute = QColor("#00d4ff");
  c.syntaxRegex = QColor("#f7d731");
  c.syntaxEscape = QColor("#ff9500");

  c.surfaceBase = QColor("#0a0015");
  c.surfaceRaised = QColor("#150025");
  c.surfaceOverlay = QColor("#200535");
  c.surfaceSunken = QColor("#06000e");
  c.surfacePopover = QColor("#1a0030");

  c.borderDefault = QColor("#3a1a50");
  c.borderSubtle = QColor("#200535");
  c.borderStrong = QColor("#5a2a70");
  c.borderFocus = QColor("#ff2d6f");

  c.textPrimary = QColor("#e0d0f0");
  c.textSecondary = QColor("#9080a0");
  c.textMuted = QColor("#4a3f5c");
  c.textDisabled = QColor("#302040");
  c.textInverse = QColor("#0a0015");
  c.textLink = QColor("#00d4ff");

  c.btnPrimaryBg = QColor("#ff2d6f");
  c.btnPrimaryFg = QColor("#ffffff");
  c.btnPrimaryHover = QColor("#ff5088");
  c.btnPrimaryActive = QColor("#d91e5a");
  c.btnSecondaryBg = QColor("#200535");
  c.btnSecondaryFg = QColor("#e0d0f0");
  c.btnSecondaryHover = QColor("#2a0a40");
  c.btnSecondaryActive = QColor("#150025");
  c.btnDangerBg = QColor("#ff2d6f");
  c.btnDangerFg = QColor("#ffffff");
  c.btnDangerHover = QColor("#ff5088");
  c.btnDangerActive = QColor("#d91e5a");
  c.btnGhostHover = QColor("#150025");
  c.btnGhostActive = QColor("#200535");

  c.inputBg = QColor("#150025");
  c.inputFg = QColor("#e0d0f0");
  c.inputBorder = QColor("#3a1a50");
  c.inputBorderFocus = QColor("#ff2d6f");
  c.inputPlaceholder = QColor("#4a3f5c");
  c.inputSelection = QColor("#2a1040");

  c.accentPrimary = QColor("#ff2d6f");
  c.accentSoft = QColor("#3a0a25");
  c.accentGlow = QColor("#ff2d6f40");

  c.scrollTrack = QColor("#0a0015");
  c.scrollThumb = QColor("#3a1a50");
  c.scrollThumbHover = QColor("#5a2a70");

  c.tabBg = QColor("#06000e");
  c.tabActiveBg = QColor("#0a0015");
  c.tabActiveBorder = QColor("#ff2d6f");
  c.tabHoverBg = QColor("#0e0018");
  c.tabFg = QColor("#4a3f5c");
  c.tabActiveFg = QColor("#e0d0f0");

  c.treeSelectedBg = QColor("#200535");
  c.treeHoverBg = QColor("#150025");
  c.treeIndentGuide = QColor("#1a0a2a");

  c.termBg = QColor("#0a0015");
  c.termFg = QColor("#e0d0f0");
  c.termCursor = QColor("#ff2d6f");
  c.termSelection = QColor("#2a1040");

  c.ansiBlack = QColor("#0a0015");
  c.ansiRed = QColor("#ff2d6f");
  c.ansiGreen = QColor("#00ff88");
  c.ansiYellow = QColor("#f7d731");
  c.ansiBlue = QColor("#00d4ff");
  c.ansiMagenta = QColor("#ff2d6f");
  c.ansiCyan = QColor("#00d4ff");
  c.ansiWhite = QColor("#e0d0f0");

  c.ansiBrightBlack = QColor("#4a3f5c");
  c.ansiBrightRed = QColor("#ff5088");
  c.ansiBrightGreen = QColor("#33ffa0");
  c.ansiBrightYellow = QColor("#f9e26a");
  c.ansiBrightBlue = QColor("#33ddff");
  c.ansiBrightMagenta = QColor("#ff5088");
  c.ansiBrightCyan = QColor("#33ddff");
  c.ansiBrightWhite = QColor("#ffffff");

  c.statusSuccess = QColor("#00ff88");
  c.statusWarning = QColor("#f7d731");
  c.statusError = QColor("#ff2d6f");
  c.statusInfo = QColor("#00d4ff");

  t.ui.borderRadius = 4;
  t.ui.glowIntensity = 0.5;
  t.ui.animationSpeed = "fancy";
  t.ui.scanlineEffect = true;

  return t;
}

ThemeDefinition matrix() {
  ThemeDefinition t;
  t.name = "Matrix";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#000000");
  c.editorFg = QColor("#00cc00");
  c.editorGutter = QColor("#000000");
  c.editorGutterFg = QColor("#005500");
  c.editorLineHighlight = QColor("#0a0a0a");
  c.editorSelection = QColor("#003300");
  c.editorSelectionFg = QColor("#00ff00");
  c.editorCursor = QColor("#00ff00");
  c.editorFindMatch = QColor("#00ff0040");
  c.editorFindMatchActive = QColor("#00ff0080");
  c.editorBracketMatch = QColor("#00ff0040");
  c.editorIndentGuide = QColor("#0a1a0a");
  c.editorWhitespace = QColor("#0a2a0a");

  c.syntaxKeyword = QColor("#00ff41");
  c.syntaxKeyword2 = QColor("#00e5ff");
  c.syntaxKeyword3 = QColor("#c6ff00");
  c.syntaxString = QColor("#7dff73");
  c.syntaxComment = QColor("#0b5f20");
  c.syntaxFunction = QColor("#35ffd2");
  c.syntaxClass = QColor("#00ff9c");
  c.syntaxNumber = QColor("#d8ff78");
  c.syntaxOperator = QColor("#00ff41");
  c.syntaxType = QColor("#00ff9c");
  c.syntaxConstant = QColor("#d8ff78");
  c.syntaxTag = QColor("#00ff41");
  c.syntaxAttribute = QColor("#c6ff00");
  c.syntaxRegex = QColor("#7dff73");
  c.syntaxEscape = QColor("#35ffd2");

  c.surfaceBase = QColor("#000000");
  c.surfaceRaised = QColor("#0a0a0a");
  c.surfaceOverlay = QColor("#111111");
  c.surfaceSunken = QColor("#000000");
  c.surfacePopover = QColor("#0d0d0d");

  c.borderDefault = QColor("#003300");
  c.borderSubtle = QColor("#001a00");
  c.borderStrong = QColor("#005500");
  c.borderFocus = QColor("#00ff00");

  c.textPrimary = QColor("#00cc00");
  c.textSecondary = QColor("#008800");
  c.textMuted = QColor("#005500");
  c.textDisabled = QColor("#003300");
  c.textInverse = QColor("#000000");
  c.textLink = QColor("#00ff00");

  c.btnPrimaryBg = QColor("#00ff00");
  c.btnPrimaryFg = QColor("#000000");
  c.btnPrimaryHover = QColor("#33ff33");
  c.btnPrimaryActive = QColor("#00cc00");
  c.btnSecondaryBg = QColor("#003300");
  c.btnSecondaryFg = QColor("#00cc00");
  c.btnSecondaryHover = QColor("#004400");
  c.btnSecondaryActive = QColor("#002200");
  c.btnDangerBg = QColor("#00ff00");
  c.btnDangerFg = QColor("#000000");
  c.btnDangerHover = QColor("#33ff33");
  c.btnDangerActive = QColor("#00cc00");
  c.btnGhostHover = QColor("#0a0a0a");
  c.btnGhostActive = QColor("#111111");

  c.inputBg = QColor("#0a0a0a");
  c.inputFg = QColor("#00cc00");
  c.inputBorder = QColor("#003300");
  c.inputBorderFocus = QColor("#00ff00");
  c.inputPlaceholder = QColor("#005500");
  c.inputSelection = QColor("#003300");

  c.accentPrimary = QColor("#00ff00");
  c.accentSoft = QColor("#003300");
  c.accentGlow = QColor("#00ff0040");

  c.scrollTrack = QColor("#000000");
  c.scrollThumb = QColor("#003300");
  c.scrollThumbHover = QColor("#005500");

  c.tabBg = QColor("#000000");
  c.tabActiveBg = QColor("#0a0a0a");
  c.tabActiveBorder = QColor("#00ff00");
  c.tabHoverBg = QColor("#050505");
  c.tabFg = QColor("#005500");
  c.tabActiveFg = QColor("#00cc00");

  c.treeSelectedBg = QColor("#001a00");
  c.treeHoverBg = QColor("#050505");
  c.treeIndentGuide = QColor("#0a1a0a");

  c.termBg = QColor("#000000");
  c.termFg = QColor("#00cc00");
  c.termCursor = QColor("#00ff00");
  c.termSelection = QColor("#003300");

  c.ansiBlack = QColor("#000000");
  c.ansiRed = QColor("#00aa00");
  c.ansiGreen = QColor("#00ff00");
  c.ansiYellow = QColor("#00dd00");
  c.ansiBlue = QColor("#00aa00");
  c.ansiMagenta = QColor("#00cc00");
  c.ansiCyan = QColor("#00ffaa");
  c.ansiWhite = QColor("#00cc00");

  c.ansiBrightBlack = QColor("#005500");
  c.ansiBrightRed = QColor("#00cc00");
  c.ansiBrightGreen = QColor("#33ff33");
  c.ansiBrightYellow = QColor("#66ff66");
  c.ansiBrightBlue = QColor("#00cc00");
  c.ansiBrightMagenta = QColor("#00ee00");
  c.ansiBrightCyan = QColor("#33ffbb");
  c.ansiBrightWhite = QColor("#00ff00");

  c.statusSuccess = QColor("#00ff00");
  c.statusWarning = QColor("#00dd00");
  c.statusError = QColor("#00aa00");
  c.statusInfo = QColor("#00ffaa");

  t.ui.borderRadius = 0;
  t.ui.glowIntensity = 0.6;
  t.ui.animationSpeed = "fancy";
  t.ui.scanlineEffect = true;

  return t;
}

ThemeDefinition ghost() {
  ThemeDefinition t;
  t.name = "Ghost";
  t.author = "Lightpad";
  t.type = "dark";

  auto &c = t.colors;

  c.editorBg = QColor("#1a1a1a");
  c.editorFg = QColor("#cccccc");
  c.editorGutter = QColor("#1a1a1a");
  c.editorGutterFg = QColor("#555555");
  c.editorLineHighlight = QColor("#222222");
  c.editorSelection = QColor("#333333");
  c.editorSelectionFg = QColor("#eeeeee");
  c.editorCursor = QColor("#ffffff");
  c.editorFindMatch = QColor("#ffffff30");
  c.editorFindMatchActive = QColor("#ffffff60");
  c.editorBracketMatch = QColor("#ffffff30");
  c.editorIndentGuide = QColor("#252525");
  c.editorWhitespace = QColor("#333333");

  c.syntaxKeyword = QColor("#ffffff");
  c.syntaxKeyword2 = QColor("#dddddd");
  c.syntaxKeyword3 = QColor("#bbbbbb");
  c.syntaxString = QColor("#aaaaaa");
  c.syntaxComment = QColor("#555555");
  c.syntaxFunction = QColor("#eeeeee");
  c.syntaxClass = QColor("#ffffff");
  c.syntaxNumber = QColor("#999999");
  c.syntaxOperator = QColor("#cccccc");
  c.syntaxType = QColor("#dddddd");
  c.syntaxConstant = QColor("#bbbbbb");
  c.syntaxTag = QColor("#ffffff");
  c.syntaxAttribute = QColor("#cccccc");
  c.syntaxRegex = QColor("#999999");
  c.syntaxEscape = QColor("#bbbbbb");

  c.surfaceBase = QColor("#1a1a1a");
  c.surfaceRaised = QColor("#222222");
  c.surfaceOverlay = QColor("#2a2a2a");
  c.surfaceSunken = QColor("#141414");
  c.surfacePopover = QColor("#252525");

  c.borderDefault = QColor("#333333");
  c.borderSubtle = QColor("#282828");
  c.borderStrong = QColor("#444444");
  c.borderFocus = QColor("#ffffff");

  c.textPrimary = QColor("#cccccc");
  c.textSecondary = QColor("#888888");
  c.textMuted = QColor("#555555");
  c.textDisabled = QColor("#3a3a3a");
  c.textInverse = QColor("#1a1a1a");
  c.textLink = QColor("#ffffff");

  c.btnPrimaryBg = QColor("#ffffff");
  c.btnPrimaryFg = QColor("#1a1a1a");
  c.btnPrimaryHover = QColor("#e8e8e8");
  c.btnPrimaryActive = QColor("#d0d0d0");
  c.btnSecondaryBg = QColor("#2a2a2a");
  c.btnSecondaryFg = QColor("#cccccc");
  c.btnSecondaryHover = QColor("#333333");
  c.btnSecondaryActive = QColor("#222222");
  c.btnDangerBg = QColor("#cccccc");
  c.btnDangerFg = QColor("#1a1a1a");
  c.btnDangerHover = QColor("#dddddd");
  c.btnDangerActive = QColor("#aaaaaa");
  c.btnGhostHover = QColor("#222222");
  c.btnGhostActive = QColor("#2a2a2a");

  c.inputBg = QColor("#222222");
  c.inputFg = QColor("#cccccc");
  c.inputBorder = QColor("#333333");
  c.inputBorderFocus = QColor("#ffffff");
  c.inputPlaceholder = QColor("#555555");
  c.inputSelection = QColor("#333333");

  c.accentPrimary = QColor("#ffffff");
  c.accentSoft = QColor("#333333");
  c.accentGlow = QColor("#ffffff20");

  c.scrollTrack = QColor("#1a1a1a");
  c.scrollThumb = QColor("#333333");
  c.scrollThumbHover = QColor("#444444");

  c.tabBg = QColor("#141414");
  c.tabActiveBg = QColor("#1a1a1a");
  c.tabActiveBorder = QColor("#ffffff");
  c.tabHoverBg = QColor("#1e1e1e");
  c.tabFg = QColor("#555555");
  c.tabActiveFg = QColor("#cccccc");

  c.treeSelectedBg = QColor("#282828");
  c.treeHoverBg = QColor("#1e1e1e");
  c.treeIndentGuide = QColor("#252525");

  c.termBg = QColor("#1a1a1a");
  c.termFg = QColor("#cccccc");
  c.termCursor = QColor("#ffffff");
  c.termSelection = QColor("#333333");

  c.ansiBlack = QColor("#1a1a1a");
  c.ansiRed = QColor("#aa8888");
  c.ansiGreen = QColor("#88aa88");
  c.ansiYellow = QColor("#aaaa88");
  c.ansiBlue = QColor("#8888aa");
  c.ansiMagenta = QColor("#aa88aa");
  c.ansiCyan = QColor("#88aaaa");
  c.ansiWhite = QColor("#cccccc");

  c.ansiBrightBlack = QColor("#555555");
  c.ansiBrightRed = QColor("#ccaaaa");
  c.ansiBrightGreen = QColor("#aaccaa");
  c.ansiBrightYellow = QColor("#ccccaa");
  c.ansiBrightBlue = QColor("#aaaacc");
  c.ansiBrightMagenta = QColor("#ccaacc");
  c.ansiBrightCyan = QColor("#aacccc");
  c.ansiBrightWhite = QColor("#eeeeee");

  c.statusSuccess = QColor("#88aa88");
  c.statusWarning = QColor("#aaaa88");
  c.statusError = QColor("#aa8888");
  c.statusInfo = QColor("#8888aa");

  t.ui.borderRadius = 4;
  t.ui.glowIntensity = 0.0;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;

  return t;
}

ThemeDefinition daylight() {
  Theme classic;
  classic.backgroundColor = QColor("#f7f8fa");
  classic.foregroundColor = QColor("#1f2937");
  classic.highlightColor = QColor("#eef2f7");
  classic.lineNumberAreaColor = QColor("#eef1f5");
  classic.keywordFormat_0 = QColor("#0f66d0");
  classic.keywordFormat_1 = QColor("#7c3aed");
  classic.keywordFormat_2 = QColor("#047857");
  classic.searchFormat = QColor("#fde68a");
  classic.singleLineCommentFormat = QColor("#6b7280");
  classic.functionFormat = QColor("#2563eb");
  classic.quotationFormat = QColor("#047857");
  classic.classFormat = QColor("#0f766e");
  classic.numberFormat = QColor("#b45309");
  classic.surfaceColor = QColor("#ffffff");
  classic.surfaceAltColor = QColor("#eef2f7");
  classic.borderColor = QColor("#d1d5db");
  classic.hoverColor = QColor("#e8edf4");
  classic.pressedColor = QColor("#dce4ee");
  classic.accentColor = QColor("#2563eb");
  classic.accentSoftColor = QColor("#dbeafe");
  classic.successColor = QColor("#15803d");
  classic.warningColor = QColor("#b45309");
  classic.errorColor = QColor("#dc2626");

  ThemeDefinition t = ThemeDefinition::fromClassicTheme(classic, "Daylight");
  t.author = "Lightpad";
  t.type = "light";
  t.colors.termBg = QColor("#ffffff");
  t.colors.termFg = QColor("#1f2937");
  t.colors.termCursor = QColor("#2563eb");
  t.colors.termSelection = QColor("#dbeafe");
  t.ui.borderRadius = 6;
  t.ui.glowIntensity = 0.0;
  t.ui.chromeOpacity = 1.0;
  t.ui.animationSpeed = "subtle";
  t.ui.scanlineEffect = false;
  t.ui.panelBorders = true;
  return t;
}

} // namespace ThemePresets
