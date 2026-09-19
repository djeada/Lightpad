#ifndef UISTYLEHELPER_H
#define UISTYLEHELPER_H

#include "../settings/theme.h"
#include <QColor>
#include <QString>
#include <QVector>

class ThemeDefinition;

class UIStyleHelper {
public:
  enum class Tone { Neutral, Accent, Success, Warning, Error, Info };

  static QString panelStyle(const Theme &theme, const QString &objectName);
  static QString panelStyle(const ThemeDefinition &theme,
                            const QString &objectName);
  static QString popupDialogStyle(const Theme &theme);
  static QString popupDialogStyle(const ThemeDefinition &theme);

  static QString searchBoxStyle(const Theme &theme);
  static QString searchBoxStyle(const ThemeDefinition &theme);

  static QString resultListStyle(const Theme &theme);
  static QString resultListStyle(const ThemeDefinition &theme);

  static QString panelHeaderStyle(const Theme &theme);
  static QString panelHeaderStyle(const Theme &theme,
                                  const QString &objectName);
  static QString panelHeaderStyle(const ThemeDefinition &theme);
  static QString panelHeaderStyle(const ThemeDefinition &theme,
                                  const QString &objectName);

  static QString treeWidgetStyle(const Theme &theme);
  static QString treeWidgetStyle(const ThemeDefinition &theme);

  static QString treeViewStyle(const Theme &theme);
  static QString treeViewStyle(const ThemeDefinition &theme);

  static QString contextMenuStyle(const Theme &theme);
  static QString contextMenuStyle(const ThemeDefinition &theme);

  static QString subduedLabelStyle(const Theme &theme);
  static QString subduedLabelStyle(const ThemeDefinition &theme);

  static QString titleLabelStyle(const Theme &theme);
  static QString titleLabelStyle(const ThemeDefinition &theme);

  static QString comboBoxStyle(const Theme &theme);
  static QString comboBoxStyle(const ThemeDefinition &theme);
  static QString comboArrowImageRule(const QColor &color);

  static QString checkBoxStyle(const Theme &theme);
  static QString checkBoxStyle(const ThemeDefinition &theme);

  static QString formDialogStyle(const Theme &theme);
  static QString formDialogStyle(const ThemeDefinition &theme);

  static QString groupBoxStyle(const Theme &theme);
  static QString groupBoxStyle(const ThemeDefinition &theme);

  static QString lineEditStyle(const Theme &theme);
  static QString lineEditStyle(const ThemeDefinition &theme);

  static QString primaryButtonStyle(const Theme &theme);
  static QString primaryButtonStyle(const ThemeDefinition &theme);

  static QString secondaryButtonStyle(const Theme &theme);
  static QString secondaryButtonStyle(const ThemeDefinition &theme);

  static QString breadcrumbButtonStyle(const Theme &theme);
  static QString breadcrumbButtonStyle(const ThemeDefinition &theme);

  static QString breadcrumbActiveButtonStyle(const Theme &theme);
  static QString breadcrumbActiveButtonStyle(const ThemeDefinition &theme);

  static QString breadcrumbSeparatorStyle(const Theme &theme);
  static QString breadcrumbSeparatorStyle(const ThemeDefinition &theme);

  static QString infoLabelStyle(const Theme &theme);
  static QString infoLabelStyle(const ThemeDefinition &theme);

  static QString successInfoLabelStyle(const Theme &theme);
  static QString successInfoLabelStyle(const ThemeDefinition &theme);

  static QString errorInfoLabelStyle(const Theme &theme);
  static QString errorInfoLabelStyle(const ThemeDefinition &theme);

  static QString tabWidgetStyle(const Theme &theme);
  static QString tabWidgetStyle(const ThemeDefinition &theme);

  static QString tableWidgetStyle(const Theme &theme);
  static QString tableWidgetStyle(const ThemeDefinition &theme);

  static QString plainTextEditStyle(const Theme &theme);
  static QString plainTextEditStyle(const ThemeDefinition &theme);

  static QString spinBoxStyle(const Theme &theme);
  static QString spinBoxStyle(const ThemeDefinition &theme);

  static QString dangerButtonStyle(const Theme &theme);
  static QString dangerButtonStyle(const ThemeDefinition &theme);

  static QString progressBarStyle(const Theme &theme);
  static QString progressBarStyle(const ThemeDefinition &theme);

  static QString toolBarStyle(const Theme &theme);
  static QString toolBarStyle(const ThemeDefinition &theme);

  static QString sectionLabelStyle(const Theme &theme);
  static QString sectionLabelStyle(const ThemeDefinition &theme);

  static QString emptyStateStyle(const Theme &theme);
  static QString emptyStateStyle(const ThemeDefinition &theme);

  static QString iconButtonStyle(const Theme &theme);
  static QString iconButtonStyle(const ThemeDefinition &theme);

  static QString toneLabelStyle(const Theme &theme, Tone tone);
  static QString toneLabelStyle(const ThemeDefinition &theme, Tone tone);

  static QString badgeStyle(const Theme &theme, Tone tone);
  static QString badgeStyle(const ThemeDefinition &theme, Tone tone);

  static QString bannerStyle(const Theme &theme, Tone tone);
  static QString bannerStyle(const ThemeDefinition &theme, Tone tone);

  static QString cardStyle(const Theme &theme,
                           const QString &objectName = QString());
  static QString cardStyle(const ThemeDefinition &theme,
                           const QString &objectName = QString());

  static QString headingStyle(const Theme &theme, int pixelSize = 13);
  static QString headingStyle(const ThemeDefinition &theme, int pixelSize = 13);

  static QString listWidgetStyle(const Theme &theme);
  static QString listWidgetStyle(const ThemeDefinition &theme);

  static QColor mutedTextColor(const Theme &theme);
  static QColor mutedTextColor(const ThemeDefinition &theme);
  static QColor secondaryTextColor(const Theme &theme);
  static QColor secondaryTextColor(const ThemeDefinition &theme);

  static QVector<QColor> seriesColors(const Theme &theme);
  static QVector<QColor> seriesColors(const ThemeDefinition &theme);

  static QColor toneColor(const Theme &theme, Tone tone);
  static QColor toneColor(const ThemeDefinition &theme, Tone tone);

  static QColor readableText(const Theme &theme, const QColor &background,
                             const QColor &preferred = QColor());
  static QColor readableText(const ThemeDefinition &theme,
                             const QColor &background,
                             const QColor &preferred = QColor());
};

#endif
