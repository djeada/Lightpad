#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

#include "../../theme/themedefinition.h"

class MainWindow;

class Preferences : public QDialog {
  Q_OBJECT

public:
  Preferences(MainWindow *parent);
  ~Preferences();

private:
  void buildUi();
  void loadCurrentSettings();
  void connectSignals();
  void persistAll();
  void persistCurrentTheme(const QString &activeThemeName);
  void refreshThemeList(const QString &selectedName = QString());
  void syncThemeControls(const ThemeDefinition &theme);
  void refreshThemeSwatches();
  void applyEditedTheme(const QString &activeThemeName);

  QWidget *createSectionHeader(const QString &title);
  QWidget *createSeparator();
  QToolButton *createColorSwatch(const QColor &color);
  QWidget *buildThemeSection();

  void onFontFamilyChanged(const QFont &font);
  void onFontSizeChanged(int size);
  void onColorSwatchClicked(QToolButton *button, const QString &role);
  void onThemePresetChanged(int row);
  void onScanlinesToggled(bool enabled);
  void onGlowPresetChanged(int index);
  void onPanelBordersToggled(bool enabled);
  void onTransparencyPresetChanged(int index);
  void onGenerateThemeClicked();
  void onSaveThemeClicked();
  void onDeleteThemeClicked();
  void onThemeMetadataChanged();
  void applyThemeEffects();

  MainWindow *m_mainWindow;

  QFontComboBox *m_fontFamilyCombo;
  QSpinBox *m_fontSizeSpinner;
  QLabel *m_fontPreview;

  QComboBox *m_tabWidthCombo;
  QCheckBox *m_autoIndentCheck;
  QCheckBox *m_autoSaveCheck;
  QCheckBox *m_trimWhitespaceCheck;
  QCheckBox *m_finalNewlineCheck;

  QCheckBox *m_lineNumbersCheck;
  QCheckBox *m_currentLineCheck;
  QCheckBox *m_bracketMatchCheck;

  QListWidget *m_themeList;
  QWidget *m_themePreview;
  QLineEdit *m_themeNameEdit = nullptr;
  QLineEdit *m_themeAuthorEdit = nullptr;
  QComboBox *m_themeBaseCombo = nullptr;
  QPushButton *m_generateThemeButton = nullptr;
  QPushButton *m_saveThemeButton = nullptr;
  QPushButton *m_deleteThemeButton = nullptr;
  QCheckBox *m_scanlinesCheck = nullptr;
  QComboBox *m_glowCombo = nullptr;
  QCheckBox *m_panelBordersCheck = nullptr;
  QComboBox *m_transparencyCombo = nullptr;

  QMap<QString, QToolButton *> m_colorSwatches;
  ThemeDefinition m_editTheme;
  bool m_updatingThemeUi = false;
};

#endif
