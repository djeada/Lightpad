#ifndef THEMEPALETTE_H
#define THEMEPALETTE_H

#include "colorcontrast.h"
#include "themedefinition.h"
#include <QPalette>

namespace ThemePalette {

inline QPalette build(const ThemeDefinition &theme) {
  using ColorContrast::flatten;
  const ThemeColors &c = theme.colors;
  const QColor base = flatten(c.surfaceBase, QColor(Qt::black));
  const QColor highlight = flatten(c.accentSoft, base);

  QPalette palette;
  palette.setColor(QPalette::Window, base);
  palette.setColor(QPalette::WindowText, c.textPrimary);
  palette.setColor(QPalette::Base, flatten(c.surfaceBase, base));
  palette.setColor(QPalette::AlternateBase, flatten(c.surfaceOverlay, base));
  palette.setColor(QPalette::Text, c.textPrimary);
  palette.setColor(QPalette::Button, flatten(c.btnSecondaryBg, base));
  palette.setColor(QPalette::ButtonText, c.btnSecondaryFg);
  palette.setColor(QPalette::BrightText,
                   ColorContrast::ensure(c.statusError, base));
  palette.setColor(QPalette::ToolTipBase, flatten(c.surfacePopover, base));
  palette.setColor(QPalette::ToolTipText, c.textPrimary);
  palette.setColor(QPalette::PlaceholderText, c.inputPlaceholder);
  palette.setColor(QPalette::Highlight, highlight);
  palette.setColor(
      QPalette::HighlightedText,
      ColorContrast::bestOf(highlight, {c.textPrimary, c.textInverse}));
  palette.setColor(QPalette::Link, c.textLink);
  palette.setColor(QPalette::LinkVisited, c.syntaxKeyword2);
  palette.setColor(QPalette::Light, flatten(c.surfaceOverlay, base));
  palette.setColor(QPalette::Midlight, flatten(c.borderSubtle, base));
  palette.setColor(QPalette::Mid, flatten(c.borderDefault, base));
  palette.setColor(QPalette::Dark, flatten(c.surfaceSunken, base));
  palette.setColor(QPalette::Shadow, flatten(c.surfaceSunken, base));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  palette.setColor(QPalette::Accent, c.accentPrimary);
#endif

  for (QPalette::ColorRole role :
       {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    palette.setColor(QPalette::Disabled, role, c.textDisabled);
  }
  palette.setColor(QPalette::Inactive, QPalette::Highlight, highlight);
  palette.setColor(QPalette::Inactive, QPalette::HighlightedText,
                   palette.color(QPalette::Active, QPalette::HighlightedText));
  return palette;
}

} // namespace ThemePalette

#endif
