#ifndef THEMEGALLERYDIALOG_H
#define THEMEGALLERYDIALOG_H

#include "../../theme/themedefinition.h"
#include "styleddialog.h"

class QListWidget;
class QListWidgetItem;
class QLabel;

class ThemePreviewSwatch;

class ThemeGalleryDialog : public StyledDialog {
  Q_OBJECT
public:
  explicit ThemeGalleryDialog(QWidget *parent = nullptr);

  void applyTheme(const Theme &theme) override;

signals:
  void themeSelected(const ThemeDefinition &theme);

private slots:
  void onSelectionChanged();
  void onApplyClicked();
  void onImportClicked();
  void onExportClicked();

private:
  void populateThemes();
  void updatePreview(const ThemeDefinition &def);

  QListWidget *m_list = nullptr;
  ThemePreviewSwatch *m_preview = nullptr;
  QLabel *m_nameLabel = nullptr;
  QLabel *m_authorLabel = nullptr;
  QLabel *m_typeLabel = nullptr;
  QPushButton *m_applyBtn = nullptr;
  QPushButton *m_importBtn = nullptr;
  QPushButton *m_exportBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;
};

#endif
