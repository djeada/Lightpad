#ifndef UIMETRICS_H
#define UIMETRICS_H

#include <QString>

namespace UiMetrics {

constexpr int SpaceXs = 2;
constexpr int SpaceSm = 4;
constexpr int SpaceMd = 8;
constexpr int SpaceLg = 12;
constexpr int SpaceXl = 16;

constexpr int PanelMargin = SpaceMd;
constexpr int PanelSpacing = SpaceMd;

constexpr int ControlHeight = 26;
constexpr int ControlPaddingH = SpaceMd;
constexpr int ToolbarHeight = 32;
constexpr int ToolbarSpacing = SpaceMd;

constexpr int ToolbarGroupSpacing = SpaceSm;

constexpr int TabHeight = 26;
constexpr int TabPaddingH = SpaceLg;
constexpr int TabIndicatorThickness = 2;

constexpr int TreeRowHeight = 22;
constexpr int TreeIndent = 14;

constexpr int TreeRowInset = 3;
constexpr int TreeTextInsetLeft = 10;
constexpr int TreeTextInsetRight = 8;
constexpr int TreeRowPadding =
    2 * TreeRowInset + TreeTextInsetLeft + TreeTextInsetRight;

constexpr int RadiusSm = 3;
constexpr int RadiusMd = 6;

constexpr int DockMinHeight = 120;

} // namespace UiMetrics

#endif
