#include "preferences.h"
#include "../../settings/settingsmanager.h"
#include "../../theme/themedefinition.h"
#include "../../theme/themeengine.h"
#include "../mainwindow.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
class ThemePreviewCard : public QWidget {
public:
  explicit ThemePreviewCard(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumHeight(190);
  }

  void setDefinition(const ThemeDefinition &def) {
    m_def = def;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const ThemeColors &c = m_def.colors;
    qreal r = m_def.ui.borderRadius;
    QRectF full = rect().adjusted(1, 1, -1, -1);

    p.setPen(QPen(c.accentPrimary, 1));
    p.setBrush(c.surfaceBase);
    p.drawRoundedRect(full, r, r);

    QRectF tb(full.left() + 6, full.top() + 6, full.width() - 12, 22);
    p.setBrush(c.surfaceRaised);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(tb, r / 2, r / 2);
    p.setPen(c.textPrimary);
    p.setFont(QFont("Monospace", 9, QFont::Bold));
    p.drawText(tb.adjusted(8, 0, 0, 0), Qt::AlignVCenter,
               "▶ " + m_def.name.toUpper());
    qreal dx = tb.right() - 10;
    for (const QColor &d : {c.statusError, c.statusWarning, c.statusSuccess}) {
      p.setBrush(d);
      p.setPen(Qt::NoPen);
      p.drawEllipse(QPointF(dx, tb.center().y()), 3.0, 3.0);
      dx -= 9;
    }

    QRectF body(full.left() + 6, tb.bottom() + 4, full.width() - 12,
                full.bottom() - tb.bottom() - 10);
    p.setBrush(c.editorBg);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(body, r / 2, r / 2);

    p.setFont(QFont("Monospace", 9));
    qreal y = body.top() + 14;
    qreal x = body.left() + 10;
    qreal lh = 15;
    struct Tok {
      QString t;
      QColor c;
    };
    auto line = [&](std::initializer_list<Tok> toks) {
      qreal cx = x;
      for (const Tok &tok : toks) {
        p.setPen(tok.c);
        p.drawText(QPointF(cx, y), tok.t);
        cx += p.fontMetrics().horizontalAdvance(tok.t);
      }
      y += lh;
    };
    line({{"// lightpad > hacker mode", c.syntaxComment}});
    line({{"def ", c.syntaxKeyword},
          {"compile", c.syntaxFunction},
          {"(", c.textPrimary},
          {"path", c.textPrimary},
          {": ", c.textPrimary},
          {"str", c.syntaxType},
          {"):", c.textPrimary}});
    line({{"    ", c.textPrimary},
          {"return ", c.syntaxKeyword},
          {"\"lightpad/\" + ", c.syntaxString},
          {"path", c.textPrimary}});

    QRectF bar(body.left() + 10, y + 2, body.width() - 20, 2);
    p.setBrush(c.accentPrimary);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bar, 1, 1);

    y = bar.bottom() + 8;
    QList<QPair<QString, QColor>> sw = {
        {"bg", c.surfaceBase},     {"fg", c.textPrimary},
        {"acc", c.accentPrimary},  {"ok", c.statusSuccess},
        {"warn", c.statusWarning}, {"err", c.statusError}};
    qreal cellW = (body.width() - 20 - (sw.size() - 1) * 3) / sw.size();
    qreal cx = body.left() + 10;
    p.setFont(QFont("Monospace", 7));
    for (const auto &s : sw) {
      QRectF rc(cx, y, cellW, 16);
      p.setBrush(s.second);
      p.setPen(QPen(c.borderSubtle, 1));
      p.drawRoundedRect(rc, 2, 2);
      p.setPen(c.textMuted);
      p.drawText(rc, Qt::AlignCenter, s.first);
      cx += cellW + 3;
    }
  }

private:
  ThemeDefinition m_def;
};
} // namespace

static const QString kSwatchStyle =
    "QToolButton { border: 1px solid %1; border-radius: 3px; min-width: 28px; "
    "max-width: 28px; min-height: 28px; max-height: 28px; }";

static const QString kSectionHeaderStyle =
    "font-family: Monospace; font-weight: 700; font-size: 13px; "
    "padding-top: 6px; letter-spacing: 1px;";

Preferences::Preferences(MainWindow *parent)
    : QDialog(nullptr), m_mainWindow(parent) {
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle("Preferences");
  setMinimumSize(480, 560);
  resize(520, 640);

  buildUi();
  loadCurrentSettings();
  connectSignals();
  show();
}

Preferences::~Preferences() = default;

QWidget *Preferences::createSectionHeader(const QString &title) {
  auto *label = new QLabel(title, this);
  label->setStyleSheet(kSectionHeaderStyle);
  return label;
}

QWidget *Preferences::createSeparator() {
  auto *line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  Theme theme = m_mainWindow->getTheme();
  line->setStyleSheet(QString("color: %1;").arg(theme.borderColor.name()));
  return line;
}

QToolButton *Preferences::createColorSwatch(const QColor &color) {
  auto *btn = new QToolButton(this);
  Theme theme = m_mainWindow->getTheme();
  btn->setStyleSheet(
      kSwatchStyle.arg(theme.borderColor.name()) +
      QString("QToolButton { background-color: %1; }").arg(color.name()));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setToolTip(color.name());
  return btn;
}

void Preferences::buildUi() {
  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 12);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *scrollWidget = new QWidget();
  auto *mainLayout = new QVBoxLayout(scrollWidget);
  mainLayout->setContentsMargins(20, 16, 20, 16);
  mainLayout->setSpacing(8);

  mainLayout->addWidget(createSectionHeader("◆ THEME"));
  mainLayout->addWidget(createSeparator());
  mainLayout->addWidget(buildThemeSection());
  {
    auto *fxGrid = new QGridLayout();
    fxGrid->setHorizontalSpacing(12);
    fxGrid->setVerticalSpacing(8);

    auto *glowLabel = new QLabel("Glow", this);
    m_glowCombo = new QComboBox(this);
    m_glowCombo->addItem("Off", 0.0);
    m_glowCombo->addItem("Low", 0.12);
    m_glowCombo->addItem("Medium", 0.28);
    m_glowCombo->addItem("High", 0.5);
    connect(m_glowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Preferences::onGlowPresetChanged);
    fxGrid->addWidget(glowLabel, 0, 0);
    fxGrid->addWidget(m_glowCombo, 0, 1);

    auto *transparencyLabel = new QLabel("Transparency", this);
    m_transparencyCombo = new QComboBox(this);
    m_transparencyCombo->addItem("Off", 1.0);
    m_transparencyCombo->addItem("Low", 0.94);
    m_transparencyCombo->addItem("Medium", 0.86);
    connect(m_transparencyCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Preferences::onTransparencyPresetChanged);
    fxGrid->addWidget(transparencyLabel, 0, 2);
    fxGrid->addWidget(m_transparencyCombo, 0, 3);

    m_scanlinesCheck = new QCheckBox("CRT scanline overlay", this);
    m_scanlinesCheck->setChecked(
        ThemeEngine::instance().activeTheme().ui.scanlineEffect);
    connect(m_scanlinesCheck, &QCheckBox::toggled, this,
            &Preferences::onScanlinesToggled);
    fxGrid->addWidget(m_scanlinesCheck, 1, 0, 1, 2);

    m_panelBordersCheck = new QCheckBox("Subtle panel borders", this);
    m_panelBordersCheck->setChecked(
        ThemeEngine::instance().activeTheme().ui.panelBorders);
    connect(m_panelBordersCheck, &QCheckBox::toggled, this,
            &Preferences::onPanelBordersToggled);
    fxGrid->addWidget(m_panelBordersCheck, 1, 2, 1, 2);
    fxGrid->setColumnStretch(4, 1);
    mainLayout->addLayout(fxGrid);
  }
  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ FONT"));
  mainLayout->addWidget(createSeparator());

  auto *fontRow = new QHBoxLayout();
  fontRow->setSpacing(10);

  auto *familyLabel = new QLabel("Family", this);
  m_fontFamilyCombo = new QFontComboBox(this);
  m_fontFamilyCombo->setFontFilters(QFontComboBox::MonospacedFonts);
  m_fontFamilyCombo->setMinimumWidth(200);

  auto *sizeLabel = new QLabel("Size", this);
  m_fontSizeSpinner = new QSpinBox(this);
  m_fontSizeSpinner->setRange(6, 72);
  m_fontSizeSpinner->setSuffix(" pt");

  fontRow->addWidget(familyLabel);
  fontRow->addWidget(m_fontFamilyCombo, 1);
  fontRow->addSpacing(12);
  fontRow->addWidget(sizeLabel);
  fontRow->addWidget(m_fontSizeSpinner);
  mainLayout->addLayout(fontRow);

  m_fontPreview = new QLabel(this);
  m_fontPreview->setWordWrap(true);
  Theme theme = m_mainWindow->getTheme();
  m_fontPreview->setStyleSheet(
      QString(
          "QLabel { border: 1px solid %1; border-radius: 4px; padding: 10px; "
          "background-color: %2; }")
          .arg(theme.borderColor.name(), theme.backgroundColor.name()));
  m_fontPreview->setMinimumHeight(56);
  m_fontPreview->setText("The quick brown fox jumps over the lazy dog.\n"
                         "0123456789 (){}[] <> :: -> => != == += -= */");
  mainLayout->addWidget(m_fontPreview);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ EDITOR"));
  mainLayout->addWidget(createSeparator());

  auto *tabRow = new QHBoxLayout();
  auto *tabLabel = new QLabel("Tab width", this);
  m_tabWidthCombo = new QComboBox(this);
  m_tabWidthCombo->addItems({"2", "4", "8"});
  m_tabWidthCombo->setFixedWidth(70);
  tabRow->addWidget(tabLabel);
  tabRow->addWidget(m_tabWidthCombo);
  tabRow->addStretch();
  mainLayout->addLayout(tabRow);

  m_autoIndentCheck = new QCheckBox("Enable automatic indentation", this);
  mainLayout->addWidget(m_autoIndentCheck);

  m_autoSaveCheck = new QCheckBox("Auto-save modified files", this);
  mainLayout->addWidget(m_autoSaveCheck);

  m_trimWhitespaceCheck =
      new QCheckBox("Trim trailing whitespace on save", this);
  mainLayout->addWidget(m_trimWhitespaceCheck);

  m_finalNewlineCheck = new QCheckBox("Insert final newline on save", this);
  mainLayout->addWidget(m_finalNewlineCheck);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ DISPLAY"));
  mainLayout->addWidget(createSeparator());

  m_lineNumbersCheck = new QCheckBox("Show line numbers", this);
  mainLayout->addWidget(m_lineNumbersCheck);

  m_currentLineCheck = new QCheckBox("Highlight current line", this);
  mainLayout->addWidget(m_currentLineCheck);

  m_bracketMatchCheck = new QCheckBox("Highlight matching brackets", this);
  mainLayout->addWidget(m_bracketMatchCheck);

  mainLayout->addSpacing(8);

  mainLayout->addWidget(createSectionHeader("▶ SYNTAX COLORS"));
  mainLayout->addWidget(createSeparator());

  auto *colorGrid = new QGridLayout();
  colorGrid->setHorizontalSpacing(16);
  colorGrid->setVerticalSpacing(8);

  struct ColorEntry {
    QString role;
    QString label;
  };

  QList<ColorEntry> entries = {
      {"backgroundColor", "Background"},
      {"foregroundColor", "Foreground"},
      {"keywordFormat_0", "Keywords 1"},
      {"keywordFormat_1", "Keywords 2"},
      {"keywordFormat_2", "Keywords 3"},
      {"singleLineCommentFormat", "Comments"},
      {"functionFormat", "Functions"},
      {"quotationFormat", "Strings"},
      {"classFormat", "Classes"},
      {"numberFormat", "Numbers"},
  };

  int row = 0, col = 0;
  for (const auto &entry : entries) {
    auto *swatchBtn = createColorSwatch(QColor("#000"));
    m_colorSwatches[entry.role] = swatchBtn;

    auto *lbl = new QLabel(entry.label, this);
    lbl->setFixedWidth(80);

    auto *cellLayout = new QHBoxLayout();
    cellLayout->setSpacing(6);
    cellLayout->addWidget(lbl);
    cellLayout->addWidget(swatchBtn);
    cellLayout->addStretch();

    colorGrid->addLayout(cellLayout, row, col);
    col++;
    if (col >= 2) {
      col = 0;
      row++;
    }
  }
  mainLayout->addLayout(colorGrid);

  mainLayout->addStretch();

  scrollArea->setWidget(scrollWidget);
  outerLayout->addWidget(scrollArea);

  auto *buttonRow = new QHBoxLayout();
  buttonRow->setContentsMargins(20, 0, 20, 0);
  buttonRow->addStretch();
  auto *closeBtn = new QToolButton(this);
  closeBtn->setText("Close");
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      QString("QToolButton { border: 1px solid %1; border-radius: 4px; "
              "padding: 4px 16px; }"
              "QToolButton:hover { background-color: %2; }")
          .arg(theme.borderColor.name(), theme.surfaceAltColor.name()));
  connect(closeBtn, &QToolButton::clicked, this, &QDialog::close);
  buttonRow->addWidget(closeBtn);
  outerLayout->addLayout(buttonRow);
}

void Preferences::loadCurrentSettings() {
  if (!m_mainWindow)
    return;

  auto textSettings = m_mainWindow->getSettings();
  auto theme = m_mainWindow->getTheme();
  auto &sm = SettingsManager::instance();

  m_fontFamilyCombo->setCurrentFont(textSettings.mainFont);
  m_fontSizeSpinner->setValue(textSettings.mainFont.pointSize());
  QFont previewFont = textSettings.mainFont;
  previewFont.setPointSize(textSettings.mainFont.pointSize());
  m_fontPreview->setFont(previewFont);

  int tabWidth = textSettings.tabWidth;
  int idx = m_tabWidthCombo->findText(QString::number(tabWidth));
  if (idx >= 0)
    m_tabWidthCombo->setCurrentIndex(idx);

  m_autoIndentCheck->setChecked(textSettings.autoIndent);
  m_autoSaveCheck->setChecked(sm.getValue("autoSaveFiles", true).toBool());
  m_trimWhitespaceCheck->setChecked(
      sm.getValue("trimTrailingWhitespace", false).toBool());
  m_finalNewlineCheck->setChecked(
      sm.getValue("insertFinalNewline", false).toBool());

  m_lineNumbersCheck->setChecked(textSettings.showLineNumberArea);
  m_currentLineCheck->setChecked(textSettings.lineHighlighted);
  m_bracketMatchCheck->setChecked(textSettings.matchingBracketsHighlighted);

  const ThemeDefinition &activeTheme = ThemeEngine::instance().activeTheme();
  if (m_glowCombo) {
    const qreal glow = activeTheme.ui.glowIntensity;
    int best = 0;
    qreal bestDistance = 10.0;
    for (int i = 0; i < m_glowCombo->count(); ++i) {
      qreal distance = qAbs(m_glowCombo->itemData(i).toDouble() - glow);
      if (distance < bestDistance) {
        best = i;
        bestDistance = distance;
      }
    }
    m_glowCombo->setCurrentIndex(best);
  }
  if (m_transparencyCombo) {
    const qreal opacity = activeTheme.ui.chromeOpacity;
    int best = 0;
    qreal bestDistance = 10.0;
    for (int i = 0; i < m_transparencyCombo->count(); ++i) {
      qreal distance =
          qAbs(m_transparencyCombo->itemData(i).toDouble() - opacity);
      if (distance < bestDistance) {
        best = i;
        bestDistance = distance;
      }
    }
    m_transparencyCombo->setCurrentIndex(best);
  }
  if (m_scanlinesCheck)
    m_scanlinesCheck->setChecked(activeTheme.ui.scanlineEffect);
  if (m_panelBordersCheck)
    m_panelBordersCheck->setChecked(activeTheme.ui.panelBorders);

  auto updateSwatch = [&](const QString &role, const QColor &color) {
    if (m_colorSwatches.contains(role)) {
      auto *btn = m_colorSwatches[role];
      btn->setStyleSheet(
          kSwatchStyle.arg(theme.borderColor.name()) +
          QString("QToolButton { background-color: %1; }").arg(color.name()));
      btn->setToolTip(color.name());
    }
  };

  updateSwatch("backgroundColor", theme.backgroundColor);
  updateSwatch("foregroundColor", theme.foregroundColor);
  updateSwatch("keywordFormat_0", theme.keywordFormat_0);
  updateSwatch("keywordFormat_1", theme.keywordFormat_1);
  updateSwatch("keywordFormat_2", theme.keywordFormat_2);
  updateSwatch("singleLineCommentFormat", theme.singleLineCommentFormat);
  updateSwatch("functionFormat", theme.functionFormat);
  updateSwatch("quotationFormat", theme.quotationFormat);
  updateSwatch("classFormat", theme.classFormat);
  updateSwatch("numberFormat", theme.numberFormat);
}

void Preferences::connectSignals() {
  if (!m_mainWindow)
    return;

  connect(m_fontFamilyCombo, &QFontComboBox::currentFontChanged, this,
          &Preferences::onFontFamilyChanged);

  connect(m_fontSizeSpinner, &QSpinBox::valueChanged, this,
          &Preferences::onFontSizeChanged);

  connect(m_tabWidthCombo, &QComboBox::currentTextChanged, this,
          [this](const QString &text) {
            bool ok;
            int width = text.toInt(&ok);
            if (ok && m_mainWindow) {
              m_mainWindow->setTabWidth(width);
              persistAll();
            }
          });

  connect(m_autoIndentCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      SettingsManager::instance().setValue("autoIndent", checked);
      persistAll();
    }
  });

  connect(m_autoSaveCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow)
      m_mainWindow->setAutoSaveEnabled(checked);
  });

  connect(
      m_trimWhitespaceCheck, &QCheckBox::toggled, this, [this](bool checked) {
        SettingsManager::instance().setValue("trimTrailingWhitespace", checked);
        SettingsManager::instance().saveSettings();
      });

  connect(m_finalNewlineCheck, &QCheckBox::toggled, this, [this](bool checked) {
    SettingsManager::instance().setValue("insertFinalNewline", checked);
    SettingsManager::instance().saveSettings();
  });

  connect(m_lineNumbersCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->showLineNumbers(checked);
      persistAll();
    }
  });

  connect(m_currentLineCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->highlihtCurrentLine(checked);
      persistAll();
    }
  });

  connect(m_bracketMatchCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (m_mainWindow) {
      m_mainWindow->highlihtMatchingBracket(checked);
      persistAll();
    }
  });

  for (auto it = m_colorSwatches.begin(); it != m_colorSwatches.end(); ++it) {
    const QString role = it.key();
    QToolButton *btn = it.value();
    connect(btn, &QToolButton::clicked, this,
            [this, btn, role]() { onColorSwatchClicked(btn, role); });
  }
}

void Preferences::onFontFamilyChanged(const QFont &font) {
  if (!m_mainWindow)
    return;

  QFont newFont = m_mainWindow->getFont();
  newFont.setFamily(font.family());
  m_mainWindow->setFont(newFont);
  m_fontPreview->setFont(newFont);
  persistAll();
}

void Preferences::onFontSizeChanged(int size) {
  if (!m_mainWindow)
    return;

  QFont newFont = m_mainWindow->getFont();
  newFont.setPointSize(size);
  m_mainWindow->setFont(newFont);
  m_fontPreview->setFont(newFont);
  persistAll();
}

void Preferences::onColorSwatchClicked(QToolButton *button,
                                       const QString &role) {
  QColor current;
  current.setNamedColor(button->toolTip());
  QColor color = QColorDialog::getColor(current, this, "Select Color");
  if (!color.isValid() || !m_mainWindow)
    return;

  button->setStyleSheet(
      kSwatchStyle.arg(m_mainWindow->getTheme().borderColor.name()) +
      QString("QToolButton { background-color: %1; }").arg(color.name()));
  button->setToolTip(color.name());

  Theme theme = m_mainWindow->getTheme();

  if (role == "backgroundColor")
    theme.backgroundColor = color;
  else if (role == "foregroundColor")
    theme.foregroundColor = color;
  else if (role == "keywordFormat_0")
    theme.keywordFormat_0 = color;
  else if (role == "keywordFormat_1")
    theme.keywordFormat_1 = color;
  else if (role == "keywordFormat_2")
    theme.keywordFormat_2 = color;
  else if (role == "singleLineCommentFormat")
    theme.singleLineCommentFormat = color;
  else if (role == "functionFormat")
    theme.functionFormat = color;
  else if (role == "quotationFormat")
    theme.quotationFormat = color;
  else if (role == "classFormat")
    theme.classFormat = color;
  else if (role == "numberFormat")
    theme.numberFormat = color;

  m_mainWindow->setTheme(theme);
  persistCurrentTheme(QString());
  persistAll();
}

void Preferences::persistAll() {
  if (!m_mainWindow)
    return;

  auto &sm = SettingsManager::instance();
  QFont currentFont = m_mainWindow->getFont();

  sm.setValue("fontFamily", currentFont.family());
  sm.setValue("fontSize", currentFont.pointSize());
  sm.setValue("fontWeight", currentFont.weight());
  sm.setValue("fontItalic", currentFont.italic());

  sm.saveSettings();
}

void Preferences::persistCurrentTheme(const QString &activeThemeName) {
  if (!m_mainWindow)
    return;

  auto &sm = SettingsManager::instance();
  QJsonObject themeJson;
  m_mainWindow->getTheme().write(themeJson);
  sm.setValue("theme", themeJson);
  sm.setValue("activeThemeName", activeThemeName);
  sm.saveSettings();
}

QWidget *Preferences::buildThemeSection() {
  auto *wrap = new QWidget(this);
  auto *row = new QHBoxLayout(wrap);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(10);

  m_themeList = new QListWidget(wrap);
  m_themeList->setMinimumWidth(180);
  m_themeList->setMaximumWidth(220);
  m_themeList->setMinimumHeight(190);

  QStringList names = ThemeEngine::instance().availableThemes();
  names.sort();
  QString active = ThemeEngine::instance().activeTheme().name;
  for (const QString &n : names) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(n);
    QPixmap px(14, 14);
    px.fill(d.colors.accentPrimary);
    auto *item = new QListWidgetItem(QIcon(px), n);
    m_themeList->addItem(item);
    if (n == active)
      m_themeList->setCurrentItem(item);
  }
  if (!m_themeList->currentItem() && m_themeList->count() > 0)
    m_themeList->setCurrentRow(0);
  row->addWidget(m_themeList);

  m_themePreview = new ThemePreviewCard(wrap);
  row->addWidget(m_themePreview, 1);

  connect(m_themeList, &QListWidget::currentRowChanged, this,
          &Preferences::onThemePresetChanged);

  if (auto *cur = m_themeList->currentItem()) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(cur->text());
    static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(d);
  }
  return wrap;
}

void Preferences::onScanlinesToggled(bool enabled) {
  ThemeDefinition d = ThemeEngine::instance().activeTheme();
  d.ui.scanlineEffect = enabled;
  ThemeEngine::instance().setActiveTheme(d);
  applyThemeEffects();
}

void Preferences::onGlowPresetChanged(int index) {
  if (!m_glowCombo || index < 0)
    return;
  ThemeDefinition d = ThemeEngine::instance().activeTheme();
  d.ui.glowIntensity = m_glowCombo->itemData(index).toDouble();
  ThemeEngine::instance().setActiveTheme(d);
  applyThemeEffects();
}

void Preferences::onPanelBordersToggled(bool enabled) {
  ThemeDefinition d = ThemeEngine::instance().activeTheme();
  d.ui.panelBorders = enabled;
  ThemeEngine::instance().setActiveTheme(d);
  applyThemeEffects();
}

void Preferences::onTransparencyPresetChanged(int index) {
  if (!m_transparencyCombo || index < 0)
    return;
  ThemeDefinition d = ThemeEngine::instance().activeTheme();
  d.ui.chromeOpacity = m_transparencyCombo->itemData(index).toDouble();
  ThemeEngine::instance().setActiveTheme(d);
  applyThemeEffects();
}

void Preferences::applyThemeEffects() {
  if (!m_mainWindow)
    return;

  ThemeDefinition d = ThemeEngine::instance().activeTheme();
  Theme classic = d.toClassicTheme();
  m_mainWindow->setTheme(classic);
  ThemeEngine::instance().setActiveTheme(d);
  m_mainWindow->setScanlineEffectEnabled(d.ui.scanlineEffect);
  persistCurrentTheme(d.name);
  if (m_themePreview)
    static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(d);
}

void Preferences::onThemePresetChanged(int row) {
  auto *item = m_themeList->item(row);
  if (!item || !m_mainWindow)
    return;
  QString name = item->text();
  ThemeDefinition d = ThemeEngine::instance().themeByName(name);
  if (m_glowCombo) {
    int idx = m_glowCombo->findData(d.ui.glowIntensity);
    if (idx < 0)
      idx = d.ui.glowIntensity <= 0.01 ? 0 : (d.ui.glowIntensity < 0.2 ? 1 : 2);
    m_glowCombo->blockSignals(true);
    m_glowCombo->setCurrentIndex(idx);
    m_glowCombo->blockSignals(false);
  }
  if (m_transparencyCombo) {
    int idx = m_transparencyCombo->findData(d.ui.chromeOpacity);
    if (idx < 0)
      idx = d.ui.chromeOpacity > 0.98 ? 0 : (d.ui.chromeOpacity > 0.9 ? 1 : 2);
    m_transparencyCombo->blockSignals(true);
    m_transparencyCombo->setCurrentIndex(idx);
    m_transparencyCombo->blockSignals(false);
  }
  if (m_scanlinesCheck) {
    m_scanlinesCheck->blockSignals(true);
    m_scanlinesCheck->setChecked(d.ui.scanlineEffect);
    m_scanlinesCheck->blockSignals(false);
  }
  if (m_panelBordersCheck) {
    m_panelBordersCheck->blockSignals(true);
    m_panelBordersCheck->setChecked(d.ui.panelBorders);
    m_panelBordersCheck->blockSignals(false);
  }
  static_cast<ThemePreviewCard *>(m_themePreview)->setDefinition(d);

  ThemeEngine::instance().setActiveTheme(d);
  Theme classic = d.toClassicTheme();
  m_mainWindow->setTheme(classic);
  ThemeEngine::instance().setActiveTheme(d);
  m_mainWindow->setScanlineEffectEnabled(d.ui.scanlineEffect);
  persistCurrentTheme(name);
}
