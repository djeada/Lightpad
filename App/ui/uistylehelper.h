#ifndef UISTYLEHELPER_H
#define UISTYLEHELPER_H

#include "../settings/theme.h"
#include <QString>

/**
 * @brief Provides unified stylesheet generation from Theme tokens.
 *
 * This class centralizes UI styling to ensure consistent appearance across
 * all dialogs, panels, and widgets in the application.
 */
class UIStyleHelper {
public:
  /**
   * @brief Generate stylesheet for popup dialogs (command palette, quick open,
   * etc.)
   */
  static QString popupDialogStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for search/input boxes in popup dialogs
   */
  static QString searchBoxStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for result lists in popup dialogs
   */
  static QString resultListStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for panel headers (breadcrumb, problems panel
   * header, etc.)
   */
  static QString panelHeaderStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for tree widgets
   */
  static QString treeWidgetStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for labels with subdued/secondary text
   */
  static QString subduedLabelStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for title labels (bold, prominent)
   */
  static QString titleLabelStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for comboboxes
   */
  static QString comboBoxStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for checkboxes
   */
  static QString checkBoxStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for form dialogs (git dialogs, etc.)
   */
  static QString formDialogStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for group boxes
   */
  static QString groupBoxStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for line edits/text inputs
   */
  static QString lineEditStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for primary buttons
   */
  static QString primaryButtonStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for secondary/default buttons
   */
  static QString secondaryButtonStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for breadcrumb segment buttons
   */
  static QString breadcrumbButtonStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for breadcrumb segment buttons (last/active)
   */
  static QString breadcrumbActiveButtonStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for breadcrumb separators
   */
  static QString breadcrumbSeparatorStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for info labels (small, subdued)
   */
  static QString infoLabelStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for success info labels
   */
  static QString successInfoLabelStyle(const Theme &theme);

  /**
   * @brief Generate stylesheet for error info labels
   */
  static QString errorInfoLabelStyle(const Theme &theme);
};

#endif // UISTYLEHELPER_H
