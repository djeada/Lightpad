#include "themegallerydialog.h"
#include "../../theme/themeengine.h"

#include <QColorDialog>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

class ThemePreviewSwatch : public QWidget {
public:
  explicit ThemePreviewSwatch(QWidget *parent = nullptr) : QWidget(parent) {
    setMinimumSize(380, 240);
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
    const qreal r = m_def.ui.borderRadius;
    QRectF full = rect().adjusted(1, 1, -1, -1);

    p.setPen(QPen(c.borderDefault, 1));
    p.setBrush(c.surfaceBase);
    p.drawRoundedRect(full, r, r);

    QRectF titlebar(full.left() + 8, full.top() + 8, full.width() - 16, 22);
    p.setBrush(c.surfaceRaised);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(titlebar, r / 2, r / 2);

    p.setPen(c.textPrimary);
    p.setFont(QFont("Monospace", 9, QFont::Bold));
    p.drawText(titlebar.adjusted(8, 0, 0, 0), Qt::AlignVCenter,
               "▶ " + m_def.name);

    qreal dotX = titlebar.right() - 14;
    qreal dotY = titlebar.center().y();
    for (const QColor &dot :
         {c.statusError, c.statusWarning, c.statusSuccess}) {
      p.setBrush(dot);
      p.setPen(Qt::NoPen);
      p.drawEllipse(QPointF(dotX, dotY), 3.5, 3.5);
      dotX -= 10;
    }

    QRectF body(full.left() + 8, titlebar.bottom() + 6, full.width() - 16,
                full.bottom() - titlebar.bottom() - 14);
    p.setBrush(c.editorBg);
    p.setPen(QPen(c.borderSubtle, 1));
    p.drawRoundedRect(body, r / 2, r / 2);

    p.setFont(QFont("Monospace", 9));
    qreal lh = 16;
    qreal y = body.top() + 10;
    qreal x = body.left() + 10;

    struct Tok {
      QString text;
      QColor color;
    };

    auto line = [&](std::initializer_list<Tok> toks) {
      qreal cx = x;
      for (const Tok &t : toks) {
        p.setPen(t.color);
        p.drawText(QPointF(cx, y), t.text);
        cx += p.fontMetrics().horizontalAdvance(t.text);
      }
      y += lh;
    };

    line({{"// hacker mode engaged", c.syntaxComment}});
    line({{"def ", c.syntaxKeyword},
          {"compile", c.syntaxFunction},
          {"(", c.textPrimary},
          {"path", c.textPrimary},
          {": ", c.textPrimary},
          {"str", c.syntaxType},
          {"):", c.textPrimary}});
    line({{"    ", c.textPrimary},
          {"return ", c.syntaxKeyword},
          {"\"", c.syntaxString},
          {"lightpad", c.syntaxString},
          {"\" + ", c.syntaxString},
          {"path", c.textPrimary}});
    line({{"    ", c.textPrimary},
          {"count ", c.textPrimary},
          {"= ", c.syntaxOperator},
          {"42", c.syntaxNumber}});

    QRectF accentBar(body.left() + 10, y + 6, body.width() - 20, 3);
    p.setBrush(c.accentPrimary);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(accentBar, 1.5, 1.5);

    qreal py = accentBar.bottom() + 10;
    struct Sw {
      QString label;
      QColor color;
    };
    QList<Sw> swatches = {{"bg", c.surfaceBase},     {"fg", c.textPrimary},
                          {"acc", c.accentPrimary},  {"ok", c.statusSuccess},
                          {"warn", c.statusWarning}, {"err", c.statusError}};
    qreal sw =
        (body.width() - 20 - (swatches.size() - 1) * 4) / swatches.size();
    qreal sx = body.left() + 10;
    p.setFont(QFont("Monospace", 7));
    for (const Sw &s : swatches) {
      QRectF r2(sx, py, sw, 18);
      p.setBrush(s.color);
      p.setPen(QPen(c.borderSubtle, 1));
      p.drawRoundedRect(r2, 2, 2);
      p.setPen(c.textMuted);
      p.drawText(r2, Qt::AlignCenter, s.label);
      sx += sw + 4;
    }
  }

private:
  ThemeDefinition m_def;
};

ThemeGalleryDialog::ThemeGalleryDialog(QWidget *parent) : StyledDialog(parent) {
  setWindowTitle(tr("Theme Gallery"));
  resize(820, 520);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 16, 16, 16);
  root->setSpacing(12);

  auto *title = new QLabel(tr("◆ Theme Gallery"), this);
  styleTitleLabel(title);
  root->addWidget(title);

  auto *subtitle =
      new QLabel(tr("Pick a preset to transform the entire editor look"), this);
  styleSubduedLabel(subtitle);
  root->addWidget(subtitle);

  auto *split = new QSplitter(Qt::Horizontal, this);
  split->setChildrenCollapsible(false);

  m_list = new QListWidget(this);
  m_list->setMinimumWidth(220);
  m_list->setIconSize(QSize(16, 16));
  split->addWidget(m_list);

  auto *rightWrap = new QWidget(this);
  auto *rightLay = new QVBoxLayout(rightWrap);
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->setSpacing(8);

  m_preview = new ThemePreviewSwatch(rightWrap);
  rightLay->addWidget(m_preview, 1);

  auto *meta = new QWidget(rightWrap);
  auto *metaLay = new QFormLayout(meta);
  metaLay->setContentsMargins(4, 4, 4, 4);
  metaLay->setLabelAlignment(Qt::AlignRight);
  m_nameEdit = new QLineEdit(meta);
  m_authorEdit = new QLineEdit(meta);
  m_typeLabel = new QLabel(meta);
  metaLay->addRow(tr("Name:"), m_nameEdit);
  metaLay->addRow(tr("Author:"), m_authorEdit);
  metaLay->addRow(tr("Type:"), m_typeLabel);
  rightLay->addWidget(meta);

  split->addWidget(rightWrap);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  root->addWidget(split, 1);

  auto *btnRow = new QHBoxLayout();
  m_importBtn = new QPushButton(tr("Import…"), this);
  m_exportBtn = new QPushButton(tr("Export…"), this);
  m_generateBtn = new QPushButton(tr("Generate Variant…"), this);
  m_saveBtn = new QPushButton(tr("Save Custom Theme"), this);
  m_deleteBtn = new QPushButton(tr("Delete Custom Theme"), this);
  m_applyBtn = new QPushButton(tr("Apply Theme"), this);
  m_closeBtn = new QPushButton(tr("Close"), this);
  styleSecondaryButton(m_generateBtn);
  styleSecondaryButton(m_saveBtn);
  styleDangerButton(m_deleteBtn);
  stylePrimaryButton(m_applyBtn);
  btnRow->addWidget(m_importBtn);
  btnRow->addWidget(m_exportBtn);
  btnRow->addWidget(m_generateBtn);
  btnRow->addWidget(m_saveBtn);
  btnRow->addWidget(m_deleteBtn);
  m_deleteBtn->hide();
  btnRow->addStretch(1);
  btnRow->addWidget(m_closeBtn);
  btnRow->addWidget(m_applyBtn);
  root->addLayout(btnRow);

  connect(m_list, &QListWidget::currentItemChanged, this,
          &ThemeGalleryDialog::onSelectionChanged);
  connect(m_list, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onApplyClicked(); });
  connect(m_applyBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onApplyClicked);
  connect(m_importBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onImportClicked);
  connect(m_exportBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onExportClicked);
  connect(m_generateBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onGenerateClicked);
  connect(m_saveBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onSaveClicked);
  connect(m_deleteBtn, &QPushButton::clicked, this,
          &ThemeGalleryDialog::onDeleteClicked);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_nameEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            m_workingTheme.name = text.trimmed();
            m_preview->setDefinition(m_workingTheme);
          });
  connect(
      m_authorEdit, &QLineEdit::textChanged, this,
      [this](const QString &text) { m_workingTheme.author = text.trimmed(); });

  populateThemes();
  StyledDialog::applyTheme(ThemeEngine::instance().activeTheme());
}

void ThemeGalleryDialog::populateThemes() {
  m_list->clear();
  QStringList names = ThemeEngine::instance().availableThemes();
  names.sort();
  const QString preferred = m_workingTheme.name;
  QString active = ThemeEngine::instance().activeTheme().name;
  for (const QString &n : names) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(n);
    QPixmap px(16, 16);
    px.fill(d.colors.accentPrimary);
    auto *item = new QListWidgetItem(QIcon(px), n);
    m_list->addItem(item);
    if ((!preferred.isEmpty() && n == preferred) ||
        (preferred.isEmpty() && n == active)) {
      m_list->setCurrentItem(item);
    }
  }
  if (!m_list->currentItem() && m_list->count() > 0)
    m_list->setCurrentRow(0);
}

void ThemeGalleryDialog::onSelectionChanged() {
  auto *item = m_list->currentItem();
  if (!item)
    return;
  ThemeDefinition d = ThemeEngine::instance().themeByName(item->text());
  updatePreview(d);
}

void ThemeGalleryDialog::updatePreview(const ThemeDefinition &def) {
  m_workingTheme = def;
  m_preview->setDefinition(m_workingTheme);
  m_nameEdit->setText(m_workingTheme.name);
  m_authorEdit->setText(m_workingTheme.author);
  m_typeLabel->setText(m_workingTheme.type);
  if (m_deleteBtn)
    m_deleteBtn->setVisible(
        !m_workingTheme.name.isEmpty() &&
        ThemeEngine::instance().hasTheme(m_workingTheme.name) &&
        !ThemeEngine::instance().isBuiltinTheme(m_workingTheme.name));
}

ThemeDefinition ThemeGalleryDialog::currentEditedTheme() const {
  ThemeDefinition current = m_workingTheme;
  current.name = m_nameEdit->text().trimmed();
  current.author = m_authorEdit->text().trimmed();
  current.normalize();
  return current;
}

void ThemeGalleryDialog::onApplyClicked() {
  ThemeDefinition current = currentEditedTheme();
  if (current.name.isEmpty()) {
    QMessageBox::warning(this, tr("Missing name"),
                         tr("Set a theme name before applying it."));
    return;
  }
  emit themeSelected(current);
  accept();
}

void ThemeGalleryDialog::onImportClicked() {
  QString path = QFileDialog::getOpenFileName(this, tr("Import Theme"),
                                              QString(), tr("JSON (*.json)"));
  if (path.isEmpty())
    return;
  if (!ThemeEngine::instance().importTheme(path)) {
    QMessageBox::warning(this, tr("Import failed"),
                         tr("Could not parse theme file."));
    return;
  }
  populateThemes();
}

void ThemeGalleryDialog::onExportClicked() {
  ThemeDefinition current = currentEditedTheme();
  if (current.name.isEmpty())
    return;
  QString path = QFileDialog::getSaveFileName(
      this, tr("Export Theme"), current.name + ".json", tr("JSON (*.json)"));
  if (path.isEmpty())
    return;
  QJsonObject json;
  current.write(json);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
      file.write(QJsonDocument(json).toJson(QJsonDocument::Indented)) < 0) {
    QMessageBox::warning(this, tr("Export failed"),
                         tr("Could not write theme file."));
  }
}

void ThemeGalleryDialog::onGenerateClicked() {
  ThemeDefinition base = currentEditedTheme();
  if (base.name.isEmpty()) {
    auto *item = m_list->currentItem();
    if (!item)
      return;
    base = ThemeEngine::instance().themeByName(item->text());
  }

  const QColor background = QColorDialog::getColor(
      base.colors.surfaceBase, this, tr("Choose Background Color"));
  if (!background.isValid())
    return;

  const QColor foreground = QColorDialog::getColor(
      base.colors.textPrimary, this, tr("Choose Foreground Color"));
  if (!foreground.isValid())
    return;

  const QColor accent = QColorDialog::getColor(base.colors.accentPrimary, this,
                                               tr("Choose Accent Color"));
  if (!accent.isValid())
    return;

  ThemeDefinition generated = ThemeDefinition::generateFromSeed(
      base,
      base.name.isEmpty() ? tr("Generated Theme") : base.name + tr(" Variant"),
      base.author, background, foreground, accent);
  generated.type = "custom";
  updatePreview(generated);
}

void ThemeGalleryDialog::onSaveClicked() {
  ThemeDefinition current = currentEditedTheme();
  const QString requestedName = current.name;
  if (current.name.isEmpty()) {
    QMessageBox::warning(this, tr("Missing name"),
                         tr("Set a theme name before saving it."));
    return;
  }

  current.type = "custom";
  current = ThemeEngine::instance().saveUserTheme(current);
  ThemeEngine::instance().loadUserThemes();
  updatePreview(current);
  populateThemes();
  if (!requestedName.isEmpty() && requestedName != current.name) {
    QMessageBox::information(
        this, tr("Saved as custom copy"),
        tr("Built-in themes are protected. Your theme was saved as \"%1\".")
            .arg(current.name));
  }
}

void ThemeGalleryDialog::onDeleteClicked() {
  auto *item = m_list->currentItem();
  if (!item)
    return;

  const QString name = item->text().trimmed();
  if (name.isEmpty() || ThemeEngine::instance().isBuiltinTheme(name)) {
    QMessageBox::information(this, tr("Built-in theme"),
                             tr("Built-in themes cannot be deleted. Only "
                                "custom themes can be removed."));
    return;
  }

  const auto answer = QMessageBox::warning(
      this, tr("Delete custom theme"),
      tr("Delete the custom theme \"%1\"? This cannot be undone.").arg(name),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (answer != QMessageBox::Yes)
    return;

  const bool wasActive = ThemeEngine::instance().activeTheme().name == name;
  if (!ThemeEngine::instance().deleteUserTheme(name)) {
    QMessageBox::warning(this, tr("Delete failed"),
                         tr("Could not delete the selected custom theme."));
    return;
  }

  populateThemes();
  if (wasActive)
    emit themeSelected(ThemeEngine::instance().activeTheme());
}

void ThemeGalleryDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  m_preview->update();
}
