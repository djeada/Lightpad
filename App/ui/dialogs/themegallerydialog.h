#ifndef THEMEGALLERYDIALOG_H
#define THEMEGALLERYDIALOG_H

#include "../../theme/themedefinition.h"
#include "styleddialog.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QLineEdit;

class ThemePreviewSwatch;

class ThemeGalleryDialog : public StyledDialog {
  Q_OBJECT
public:
  explicit ThemeGalleryDialog(QWidget *parent = nullptr);
  using StyledDialog::applyTheme;

  void applyTheme(const Theme &theme) override;

signals:
  void themeSelected(const ThemeDefinition &theme);

private slots:
  void onSelectionChanged();
  void onApplyClicked();
  void onImportClicked();
  void onExportClicked();
  void onGenerateClicked();
  void onSaveClicked();
  void onDeleteClicked();

private:
  void populateThemes();
  void updatePreview(const ThemeDefinition &def);
  ThemeDefinition currentEditedTheme() const;

  QListWidget *m_list = nullptr;
  ThemePreviewSwatch *m_preview = nullptr;
  QLineEdit *m_nameEdit = nullptr;
  QLineEdit *m_authorEdit = nullptr;
  QLabel *m_typeLabel = nullptr;
  QPushButton *m_applyBtn = nullptr;
  QPushButton *m_importBtn = nullptr;
  QPushButton *m_exportBtn = nullptr;
  QPushButton *m_generateBtn = nullptr;
  QPushButton *m_saveBtn = nullptr;
  QPushButton *m_deleteBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;
  ThemeDefinition m_workingTheme;
};

#endif
