#ifndef UISTYLEHELPER_H
#define UISTYLEHELPER_H

#include "../settings/theme.h"
#include <QString>

class UIStyleHelper {
public:
  static QString popupDialogStyle(const Theme &theme);

  static QString searchBoxStyle(const Theme &theme);

  static QString resultListStyle(const Theme &theme);

  static QString panelHeaderStyle(const Theme &theme);

  static QString treeWidgetStyle(const Theme &theme);

  static QString subduedLabelStyle(const Theme &theme);

  static QString titleLabelStyle(const Theme &theme);

  static QString comboBoxStyle(const Theme &theme);

  static QString checkBoxStyle(const Theme &theme);

  static QString formDialogStyle(const Theme &theme);

  static QString groupBoxStyle(const Theme &theme);

  static QString lineEditStyle(const Theme &theme);

  static QString primaryButtonStyle(const Theme &theme);

  static QString secondaryButtonStyle(const Theme &theme);

  static QString breadcrumbButtonStyle(const Theme &theme);

  static QString breadcrumbActiveButtonStyle(const Theme &theme);

  static QString breadcrumbSeparatorStyle(const Theme &theme);

  static QString infoLabelStyle(const Theme &theme);

  static QString successInfoLabelStyle(const Theme &theme);

  static QString errorInfoLabelStyle(const Theme &theme);
};

#endif
