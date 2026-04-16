#include "themegallerydialog.h"
#include "../../theme/themeengine.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
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
    for (const QColor &dot : {c.statusError, c.statusWarning, c.statusSuccess}) {
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
    qreal sw = (body.width() - 20 - (swatches.size() - 1) * 4) / swatches.size();
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
  m_nameLabel = new QLabel(meta);
  m_authorLabel = new QLabel(meta);
  m_typeLabel = new QLabel(meta);
  metaLay->addRow(tr("Name:"), m_nameLabel);
  metaLay->addRow(tr("Author:"), m_authorLabel);
  metaLay->addRow(tr("Type:"), m_typeLabel);
  rightLay->addWidget(meta);

  split->addWidget(rightWrap);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  root->addWidget(split, 1);

  auto *btnRow = new QHBoxLayout();
  m_importBtn = new QPushButton(tr("Import…"), this);
  m_exportBtn = new QPushButton(tr("Export…"), this);
  m_applyBtn = new QPushButton(tr("Apply Theme"), this);
  m_closeBtn = new QPushButton(tr("Close"), this);
  stylePrimaryButton(m_applyBtn);
  btnRow->addWidget(m_importBtn);
  btnRow->addWidget(m_exportBtn);
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
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

  populateThemes();
  applyTheme(ThemeEngine::instance().classicTheme());
}

void ThemeGalleryDialog::populateThemes() {
  m_list->clear();
  QStringList names = ThemeEngine::instance().availableThemes();
  names.sort();
  QString active = ThemeEngine::instance().activeTheme().name;
  for (const QString &n : names) {
    ThemeDefinition d = ThemeEngine::instance().themeByName(n);
    QPixmap px(16, 16);
    px.fill(d.colors.accentPrimary);
    auto *item = new QListWidgetItem(QIcon(px), n);
    m_list->addItem(item);
    if (n == active)
      m_list->setCurrentItem(item);
  }
  if (!m_list->currentItem() && m_list->count() > 0)
    m_list->setCurrentRow(0);
}

void ThemeGalleryDialog::onSelectionChanged() {
  auto *item = m_list->currentItem();
  if (!item)
    return;
  ThemeDefinition d =
      ThemeEngine::instance().themeByName(item->text());
  updatePreview(d);
}

void ThemeGalleryDialog::updatePreview(const ThemeDefinition &def) {
  m_preview->setDefinition(def);
  m_nameLabel->setText(def.name);
  m_authorLabel->setText(def.author.isEmpty() ? tr("—") : def.author);
  m_typeLabel->setText(def.type);
}

void ThemeGalleryDialog::onApplyClicked() {
  auto *item = m_list->currentItem();
  if (!item)
    return;
  ThemeDefinition d = ThemeEngine::instance().themeByName(item->text());
  emit themeSelected(d);
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
  auto *item = m_list->currentItem();
  if (!item)
    return;
  QString path = QFileDialog::getSaveFileName(this, tr("Export Theme"),
                                              item->text() + ".json",
                                              tr("JSON (*.json)"));
  if (path.isEmpty())
    return;
  if (!ThemeEngine::instance().exportTheme(item->text(), path)) {
    QMessageBox::warning(this, tr("Export failed"),
                         tr("Could not write theme file."));
  }
}

void ThemeGalleryDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  m_preview->update();
}
