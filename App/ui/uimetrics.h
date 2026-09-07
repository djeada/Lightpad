#ifndef UIMETRICS_H
#define UIMETRICS_H

#include <QString>

// Shared layout constants for application chrome.
//
// Panels used to pick their own paddings, control heights and corner radii,
// which is what made the docks read as a stack of nested outlined boxes. Every
// panel should size its chrome from this scale so toolbars, tab bars and trees
// line up with each other, and so a change here moves the whole UI at once.
//
// Colours are not part of this: those come from the semantic roles in
// ThemeColors. This file is only about space and size.
namespace UiMetrics {

// Spacing scale. Prefer these over ad-hoc pixel values; anything between two
// steps is almost always a sign the layout wants a different step.
constexpr int SpaceXs = 2;
constexpr int SpaceSm = 4;
constexpr int SpaceMd = 8;
constexpr int SpaceLg = 12;
constexpr int SpaceXl = 16;

// Panel chrome.
constexpr int PanelMargin = SpaceMd;
constexpr int PanelSpacing = SpaceMd;

// Compact IDE control sizing. Toolbar buttons, combo boxes and inputs in a
// dock share one height so a control strip reads as a single row.
constexpr int ControlHeight = 26;
constexpr int ControlPaddingH = SpaceMd;
constexpr int ToolbarHeight = 32;
constexpr int ToolbarSpacing = SpaceMd;
// Gap between grouped actions inside a toolbar group.
constexpr int ToolbarGroupSpacing = SpaceSm;

// Tabs.
constexpr int TabHeight = 26;
constexpr int TabPaddingH = SpaceLg;
constexpr int TabIndicatorThickness = 2;

// Trees and lists. One row height across explorer, debugger, tests and
// problems keeps the panels visually consistent when docked side by side.
constexpr int TreeRowHeight = 22;
constexpr int TreeIndent = 14;
// Horizontal room a row's chrome takes from its item rect. Delegates that
// inset their text must add this back in sizeHint(), or ResizeToContents
// columns come out too narrow and elide their own content.
constexpr int TreeRowInset = 3;
constexpr int TreeTextInsetLeft = 10;
constexpr int TreeTextInsetRight = 8;
constexpr int TreeRowPadding =
    2 * TreeRowInset + TreeTextInsetLeft + TreeTextInsetRight;

// Corner radius for buttons, inputs and pills.
constexpr int RadiusSm = 3;
constexpr int RadiusMd = 6;

// Minimum usable height for a dock panel before its content stops making
// sense; docks should not be shrinkable below this.
constexpr int DockMinHeight = 120;

} // namespace UiMetrics

#endif
