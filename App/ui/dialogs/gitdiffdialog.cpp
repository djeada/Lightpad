#include "gitdiffdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QVBoxLayout>

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include "../uistylehelper.h"

namespace {
constexpr int kDiffPreviewLimit = 80000;
}

GitDiffDialog::GitDiffDialog(GitIntegration *git, const QString &targetId,
                             DiffTarget target, bool staged, const Theme &theme,
                             QWidget *parent)
    : QDialog(parent), m_git(git), m_targetId(targetId), m_target(target),
      m_staged(staged), m_diffText(), m_wordDiffText(), m_summaryText(),
      m_viewMode(DiffViewMode::Unified), m_commitAuthor(), m_commitDate(),
      m_commitMessage(), m_mainSplitter(nullptr), m_fileListPanel(nullptr),
      m_fileList(nullptr), m_fileListHeader(nullptr), m_diffView(nullptr),
      m_minimapFrame(nullptr), m_minimapLabel(nullptr), m_summaryLabel(nullptr),
      m_commitInfoLabel(nullptr), m_changeCounterLabel(nullptr),
      m_searchCounterLabel(nullptr), m_toolbarSeparator1(nullptr),
      m_toolbarSeparator2(nullptr), m_modeSelector(nullptr),
      m_wrapToggle(nullptr), m_searchField(nullptr), m_findPrevButton(nullptr),
      m_findNextButton(nullptr), m_prevButton(nullptr), m_nextButton(nullptr),
      m_copyButton(nullptr), m_lines(), m_files(), m_changeBlocks(),
      m_currentChange(0), m_addedCount(0), m_deletedCount(0), m_totalAdded(0),
      m_totalDeleted(0), m_currentSearchMatch(0), m_totalSearchMatches(0),
      m_theme(theme) {
  setModal(true);
  setWindowTitle(tr("Diff Viewer"));
  if (m_target == DiffTarget::File) {
    QFileInfo info(m_targetId);
    m_summaryText =
        tr("Diff • %1")
            .arg(info.fileName().isEmpty() ? m_targetId : info.fileName());
  } else {
    m_summaryText = tr("Commit • %1").arg(m_targetId.left(7));
  }
  resize(900, 600);
  setMinimumSize(700, 450);
  buildUi();
  applyTheme(theme);
}

void GitDiffDialog::setDiffText(const QString &diffText) {
  m_diffText = diffText.left(kDiffPreviewLimit);
  m_wordDiffText.clear();
  if (m_summaryText.isEmpty()) {
    m_summaryText = tr("Diff");
  }
  m_totalAdded = 0;
  m_totalDeleted = 0;
  parseDiff(m_diffText);
  parseFiles();
  m_totalAdded = m_addedCount;
  m_totalDeleted = m_deletedCount;
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText);
  }
  if (m_changeCounterLabel) {
    m_changeCounterLabel->setText(
        tr("0 changes • +%1 -%2").arg(m_totalAdded).arg(m_totalDeleted));
  }

  if (m_fileListPanel && m_fileList) {
    bool showFileList = m_files.size() > 1;
    m_fileListPanel->setVisible(showFileList);
    if (showFileList) {
      m_fileList->clear();
      for (const auto &file : m_files) {
        QFileInfo info(file.filename);
        QString displayName = info.fileName();
        if (displayName.isEmpty()) {
          displayName = file.filename;
        }

        if (displayName.length() > 25) {
          displayName = displayName.left(22) + "...";
        }
        auto *item = new QListWidgetItem(displayName);
        item->setToolTip(file.filename);
        item->setData(Qt::UserRole, file.startLine);

        QString statsText =
            QString("+%1 -%2").arg(file.addedCount).arg(file.deletedCount);
        item->setData(Qt::UserRole + 1, statsText);
        m_fileList->addItem(item);
      }
      m_fileListHeader->setText(tr("FILES (%1)").arg(m_files.size()));
    }
  }

  updateDiffPresentation();
}

void GitDiffDialog::setCommitInfo(const QString &author, const QString &date,
                                  const QString &message) {
  m_commitAuthor = author;
  m_commitDate = date;
  m_commitMessage = message;
  if (m_commitInfoLabel && !author.isEmpty()) {
    QString shortMsg = message.section('\n', 0, 0).trimmed();
    if (shortMsg.length() > 80) {
      shortMsg = shortMsg.left(77) + "...";
    }

    QString html =
        QString("<div style='margin-bottom: 4px;'>"
                "<span style='font-weight: 600;'>%1</span>"
                "<span style='color: %4; margin-left: 12px;'>%2</span>"
                "</div>"
                "<div style='color: %4;'>%3</div>")
            .arg(htmlEscape(author))
            .arg(htmlEscape(date))
            .arg(htmlEscape(shortMsg))
            .arg(m_theme.singleLineCommentFormat.name());

    m_commitInfoLabel->setText(html);
    m_commitInfoLabel->setTextFormat(Qt::RichText);
    m_commitInfoLabel->setVisible(true);
  }
}

void GitDiffDialog::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Find)) {
    if (m_searchField) {
      m_searchField->setFocus();
      m_searchField->selectAll();
    }
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_F3 && !(event->modifiers() & Qt::ShiftModifier)) {
    performSearch(false);
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F3 && (event->modifiers() & Qt::ShiftModifier)) {
    performSearch(true);
    event->accept();
    return;
  }

  if (!m_searchField || !m_searchField->hasFocus()) {
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_K) {
      onPrevChange();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Down || event->key() == Qt::Key_J) {
      onNextChange();
      event->accept();
      return;
    }
  }

  if (event->key() == Qt::Key_Escape) {
    close();
    event->accept();
    return;
  }

  QDialog::keyPressEvent(event);
}

void GitDiffDialog::resizeEvent(QResizeEvent *event) {
  QDialog::resizeEvent(event);
}

void GitDiffDialog::onFileSelected(int index) {
  if (index < 0 || index >= m_files.size()) {
    return;
  }
  scrollToFile(index);
}

void GitDiffDialog::onModeChanged(int index) {
  if (index == 0) {
    m_viewMode = DiffViewMode::Unified;
  } else if (index == 1) {
    m_viewMode = DiffViewMode::Split;
  } else {
    m_viewMode = DiffViewMode::Word;
  }
  updateDiffPresentation();
}

void GitDiffDialog::onWrapToggled(bool enabled) {
  if (m_diffView) {
    m_diffView->setLineWrapMode(enabled ? QTextEdit::WidgetWidth
                                        : QTextEdit::NoWrap);
  }
}

void GitDiffDialog::onPrevChange() {
  if (m_changeBlocks.isEmpty()) {
    return;
  }
  m_currentChange =
      (m_currentChange - 1 + m_changeBlocks.size()) % m_changeBlocks.size();
  scrollToChange(m_currentChange);
  updateChangeCounter();
}

void GitDiffDialog::onNextChange() {
  if (m_changeBlocks.isEmpty()) {
    return;
  }
  m_currentChange = (m_currentChange + 1) % m_changeBlocks.size();
  scrollToChange(m_currentChange);
  updateChangeCounter();
}

void GitDiffDialog::onCopy() {
  if (!m_diffView) {
    return;
  }
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_diffView->toPlainText());
}

void GitDiffDialog::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *headerWidget = new QWidget(this);
  headerWidget->setObjectName("diffHeader");
  auto *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(16, 12, 16, 12);
  headerLayout->setSpacing(12);

  m_summaryLabel = new QLabel(tr("Diff"), this);
  m_summaryLabel->setObjectName("diffTitle");
  headerLayout->addWidget(m_summaryLabel);

  m_changeCounterLabel = new QLabel(this);
  m_changeCounterLabel->setObjectName("changeCounter");
  headerLayout->addWidget(m_changeCounterLabel);

  headerLayout->addStretch(1);

  auto *viewGroup = new QWidget(this);
  viewGroup->setObjectName("toolbarGroup");
  auto *viewLayout = new QHBoxLayout(viewGroup);
  viewLayout->setContentsMargins(8, 4, 8, 4);
  viewLayout->setSpacing(8);

  m_modeSelector = new QComboBox(this);
  m_modeSelector->addItems({tr("Unified"), tr("Split"), tr("Word")});
  m_modeSelector->setFixedWidth(90);
  viewLayout->addWidget(m_modeSelector);

  m_wrapToggle = new QCheckBox(tr("Wrap"), this);
  m_wrapToggle->setChecked(true);
  viewLayout->addWidget(m_wrapToggle);

  headerLayout->addWidget(viewGroup);

  auto *navGroup = new QWidget(this);
  navGroup->setObjectName("toolbarGroup");
  auto *navLayout = new QHBoxLayout(navGroup);
  navLayout->setContentsMargins(8, 4, 8, 4);
  navLayout->setSpacing(4);

  m_prevButton = new QPushButton(tr("◀ Prev"), this);
  m_prevButton->setToolTip(tr("Previous change (↑)"));
  navLayout->addWidget(m_prevButton);

  m_nextButton = new QPushButton(tr("Next ▶"), this);
  m_nextButton->setToolTip(tr("Next change (↓)"));
  navLayout->addWidget(m_nextButton);

  headerLayout->addWidget(navGroup);

  auto *searchGroup = new QWidget(this);
  searchGroup->setObjectName("toolbarGroup");
  auto *searchLayout = new QHBoxLayout(searchGroup);
  searchLayout->setContentsMargins(8, 4, 8, 4);
  searchLayout->setSpacing(6);

  m_searchField = new QLineEdit(this);
  m_searchField->setPlaceholderText(tr("Find..."));
  m_searchField->setFixedWidth(140);
  m_searchField->setClearButtonEnabled(false);
  searchLayout->addWidget(m_searchField);

  m_searchCounterLabel = new QLabel(this);
  m_searchCounterLabel->setObjectName("searchCounter");
  m_searchCounterLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_searchCounterLabel->setSizePolicy(QSizePolicy::Minimum,
                                      QSizePolicy::Preferred);
  m_searchCounterLabel->setVisible(false);
  searchLayout->addWidget(m_searchCounterLabel);

  m_findPrevButton = new QPushButton(tr("◀"), this);
  m_findPrevButton->setFixedWidth(26);
  m_findPrevButton->setToolTip(tr("Find previous"));
  searchLayout->addWidget(m_findPrevButton);

  m_findNextButton = new QPushButton(tr("▶"), this);
  m_findNextButton->setFixedWidth(26);
  m_findNextButton->setToolTip(tr("Find next"));
  searchLayout->addWidget(m_findNextButton);

  headerLayout->addWidget(searchGroup);

  m_copyButton = new QPushButton(tr("Copy All"), this);
  m_copyButton->setObjectName("copyButton");
  headerLayout->addWidget(m_copyButton);

  layout->addWidget(headerWidget);

  m_commitInfoLabel = new QLabel(this);
  m_commitInfoLabel->setObjectName("commitInfo");
  m_commitInfoLabel->setVisible(false);
  m_commitInfoLabel->setWordWrap(true);
  layout->addWidget(m_commitInfoLabel);

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->setHandleWidth(1);
  m_mainSplitter->setChildrenCollapsible(false);

  m_fileListPanel = new QWidget(this);
  m_fileListPanel->setObjectName("fileListPanel");
  m_fileListPanel->setVisible(false);
  auto *fileListLayout = new QVBoxLayout(m_fileListPanel);
  fileListLayout->setContentsMargins(0, 0, 0, 0);
  fileListLayout->setSpacing(0);

  m_fileListHeader = new QLabel(tr("FILES"), this);
  m_fileListHeader->setObjectName("fileListHeader");
  fileListLayout->addWidget(m_fileListHeader);

  m_fileList = new QListWidget(this);
  m_fileList->setObjectName("fileList");
  m_fileList->setFocusPolicy(Qt::NoFocus);
  fileListLayout->addWidget(m_fileList);

  m_fileListPanel->setFixedWidth(220);
  m_mainSplitter->addWidget(m_fileListPanel);

  m_diffView = new QTextEdit(this);
  m_diffView->setReadOnly(true);
  m_diffView->setLineWrapMode(QTextEdit::WidgetWidth);
  m_diffView->setUndoRedoEnabled(false);
  m_diffView->setAcceptRichText(true);
  m_diffView->setFontFamily("monospace");
  m_diffView->setTabStopDistance(
      QFontMetrics(m_diffView->font()).horizontalAdvance(' ') * 4);
  m_diffView->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  m_mainSplitter->addWidget(m_diffView);

  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);

  layout->addWidget(m_mainSplitter, 1);

  auto *footerWidget = new QWidget(this);
  footerWidget->setObjectName("diffFooter");
  auto *footerLayout = new QHBoxLayout(footerWidget);
  footerLayout->setContentsMargins(16, 6, 16, 6);
  footerLayout->setSpacing(16);

  auto *shortcutsLabel =
      new QLabel(tr("↑↓ Navigate changes  •  Ctrl+F Find  •  Esc Close"), this);
  shortcutsLabel->setObjectName("shortcutsLabel");
  footerLayout->addWidget(shortcutsLabel);
  footerLayout->addStretch();

  layout->addWidget(footerWidget);

  m_minimapFrame = nullptr;
  m_minimapLabel = nullptr;
  m_toolbarSeparator1 = nullptr;
  m_toolbarSeparator2 = nullptr;

  connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GitDiffDialog::onModeChanged);
  connect(m_wrapToggle, &QCheckBox::toggled, this,
          &GitDiffDialog::onWrapToggled);
  connect(m_prevButton, &QPushButton::clicked, this,
          &GitDiffDialog::onPrevChange);
  connect(m_nextButton, &QPushButton::clicked, this,
          &GitDiffDialog::onNextChange);
  connect(m_searchField, &QLineEdit::returnPressed,
          [this]() { performSearch(false); });
  connect(m_searchField, &QLineEdit::textChanged,
          [this](const QString &queryText) {
            updateSearchCounter();
            if (queryText.size() >= 2) {
              performSearch(false);
            }
          });
  connect(m_findPrevButton, &QPushButton::clicked,
          [this]() { performSearch(true); });
  connect(m_findNextButton, &QPushButton::clicked,
          [this]() { performSearch(false); });
  connect(m_copyButton, &QPushButton::clicked, this, &GitDiffDialog::onCopy);
  connect(m_fileList, &QListWidget::currentRowChanged, this,
          &GitDiffDialog::onFileSelected);
}

void GitDiffDialog::applyTheme(const Theme &theme) {

  QString styles =
      QString(

          "QDialog { background: %1; }"

          "#diffHeader { background: %2; border-bottom: 1px solid %3; }"
          "#diffTitle { font-size: 14px; font-weight: 600; color: %4; }"
          "#changeCounter { font-size: 11px; color: %5; background: %6; "
          "  padding: 3px 10px; border-radius: 10px; }"

          "#toolbarGroup { background: %7; border: 1px solid %3; "
          "border-radius: 6px; }"

          "#searchCounter { font-size: 11px; color: %8; }"

          "#commitInfo { font-size: 12px; color: %4; background: %9; "
          "  border-bottom: 1px solid %3; padding: 10px 16px; }"

          "#diffFooter { background: %2; border-top: 1px solid %3; }"
          "#shortcutsLabel { font-size: 11px; color: %8; }"

          "#fileListPanel { background: %2; border-right: 1px solid %3; }"
          "#fileListHeader { font-size: 10px; font-weight: 600; color: %8; "
          "  letter-spacing: 1px; background: %7; padding: 10px 12px; "
          "  border-bottom: 1px solid %3; }"
          "#fileList { background: %2; color: %4; border: none; outline: none; "
          "}"
          "#fileList::item { padding: 8px 12px; border-left: 3px solid "
          "transparent; }"
          "#fileList::item:selected { background: %10; border-left-color: %11; "
          "}"
          "#fileList::item:hover { background: %6; }"

          "#copyButton { background: %11; color: white; border: none; "
          "  border-radius: 4px; padding: 6px 12px; font-weight: 500; }"
          "#copyButton:hover { background: %12; }")
          .arg(theme.backgroundColor.name())
          .arg(theme.surfaceColor.name())
          .arg(theme.borderColor.name())
          .arg(theme.foregroundColor.name())
          .arg(theme.foregroundColor.name())
          .arg(theme.hoverColor.name())
          .arg(theme.surfaceAltColor.name())
          .arg(theme.singleLineCommentFormat.name())
          .arg(theme.surfaceAltColor.name())
          .arg(theme.accentSoftColor.name())
          .arg(theme.accentColor.name())
          .arg(theme.accentColor.lighter(110).name());

  setStyleSheet(styles);

  QString buttonStyle =
      QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
              "  border-radius: 4px; padding: 5px 10px; font-size: 12px; }"
              "QPushButton:hover { background: %4; border-color: %5; }"
              "QPushButton:pressed { background: %6; }")
          .arg(theme.surfaceAltColor.name())
          .arg(theme.foregroundColor.name())
          .arg(theme.borderColor.name())
          .arg(theme.hoverColor.name())
          .arg(theme.borderColor.darker(110).name())
          .arg(theme.pressedColor.name());

  for (QPushButton *btn :
       {m_prevButton, m_nextButton, m_findPrevButton, m_findNextButton}) {
    if (btn)
      btn->setStyleSheet(buttonStyle);
  }

  if (m_modeSelector) {
    m_modeSelector->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  if (m_wrapToggle) {
    m_wrapToggle->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }

  if (m_searchField) {
    QString searchStyle =
        QString(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3; "
            "  border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
            "QLineEdit:focus { border-color: %4; }"
            "QLineEdit::clear-button { image: none; width: 0px; height: 0px; }")
            .arg(theme.backgroundColor.name())
            .arg(theme.foregroundColor.name())
            .arg(theme.borderColor.name())
            .arg(theme.accentColor.name());
    m_searchField->setStyleSheet(searchStyle);
  }

  if (m_diffView) {
    QString diffStyle =
        QString("QTextEdit { background: %1; color: %2; border: none; "
                "  selection-background-color: %3; }"
                "QScrollBar:vertical { background: %4; width: 10px; }"
                "QScrollBar::handle:vertical { background: %5; border-radius: "
                "5px; min-height: 30px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical "
                "{ height: 0; }")
            .arg(theme.backgroundColor.name())
            .arg(theme.foregroundColor.name())
            .arg(theme.accentSoftColor.name())
            .arg(theme.surfaceColor.name())
            .arg(theme.borderColor.name());
    m_diffView->setStyleSheet(diffStyle);
  }
}

void GitDiffDialog::updateDiffPresentation() {
  if (!m_diffView) {
    return;
  }

  if (m_diffText.isEmpty()) {
    m_lines.clear();
    m_changeBlocks.clear();
    updateChangeCounter();

    QString emptyHtml =
        QString(
            "<html><body style='background: %1; color: %2; padding: 40px; "
            "text-align: center; font-family: sans-serif;'>"
            "<div style='font-size: 16px; margin-bottom: 8px;'>No changes</div>"
            "<div style='font-size: 12px; color: %3;'>There are no differences "
            "to display</div>"
            "</body></html>")
            .arg(m_theme.backgroundColor.name())
            .arg(m_theme.foregroundColor.name())
            .arg(m_theme.singleLineCommentFormat.name());
    m_diffView->setHtml(emptyHtml);
    if (m_summaryLabel && !m_summaryText.isEmpty()) {
      m_summaryLabel->setText(m_summaryText);
    }
    return;
  }

  if (m_viewMode == DiffViewMode::Word && m_wordDiffText.isEmpty()) {
    m_wordDiffText = resolveWordDiff();
  }
  const QString diffSource =
      (m_viewMode == DiffViewMode::Word && !m_wordDiffText.isEmpty())
          ? m_wordDiffText
          : m_diffText;
  parseDiff(diffSource);
  m_addedCount = m_totalAdded;
  m_deletedCount = m_totalDeleted;
  collectChangeBlocks();
  int verticalScroll = m_diffView->verticalScrollBar()->value();
  int horizontalScroll = m_diffView->horizontalScrollBar()->value();
  switch (m_viewMode) {
  case DiffViewMode::Unified:
    rebuildUnified();
    break;
  case DiffViewMode::Split:
    rebuildSplit();
    break;
  case DiffViewMode::Word:
    rebuildWord();
    break;
  }
  m_diffView->verticalScrollBar()->setValue(verticalScroll);
  m_diffView->horizontalScrollBar()->setValue(horizontalScroll);
  updateChangeCounter();
}

void GitDiffDialog::rebuildUnified() {

  QString addBgStr = QString("rgba(%1,%2,%3,0.15)")
                         .arg(m_theme.successColor.red())
                         .arg(m_theme.successColor.green())
                         .arg(m_theme.successColor.blue());
  QString delBgStr = QString("rgba(%1,%2,%3,0.15)")
                         .arg(m_theme.errorColor.red())
                         .arg(m_theme.errorColor.green())
                         .arg(m_theme.errorColor.blue());

  QString html;
  html += "<html><head><style>";
  html += QString("body { background: %1; color: %2; margin: 0; padding: 0; }")
              .arg(m_theme.backgroundColor.name())
              .arg(m_theme.foregroundColor.name());
  html += "table { border-collapse: collapse; width: 100%; }";
  html += QString(
      "td { font-family: 'SF Mono', Consolas, monospace; font-size: 12px; "
      "padding: 0 8px; line-height: 20px; vertical-align: top; }");
  html += QString(".ln { color: %1; text-align: right; width: 50px; "
                  "padding-right: 12px; border-right: 1px solid %2; "
                  "user-select: none; background: %3; }")
              .arg(m_theme.singleLineCommentFormat.name())
              .arg(m_theme.borderColor.name())
              .arg(m_theme.surfaceColor.name());
  html += QString(".gutter { width: 4px; padding: 0; }");
  html += QString(".gutter-add { background: %1; }")
              .arg(m_theme.successColor.name());
  html +=
      QString(".gutter-del { background: %1; }").arg(m_theme.errorColor.name());
  html += QString(".hunk { color: %1; background: %2; font-weight: 500; "
                  "padding: 8px 12px; border-top: 1px solid %3; "
                  "border-bottom: 1px solid %3; }")
              .arg(m_theme.accentColor.name())
              .arg(m_theme.surfaceAltColor.name())
              .arg(m_theme.borderColor.name());
  html += QString(".file { color: %1; background: %2; font-weight: 600; "
                  "padding: 10px 12px; font-size: 13px; }")
              .arg(m_theme.foregroundColor.name())
              .arg(m_theme.surfaceColor.name());
  html += QString(".add { background: %1; }").arg(addBgStr);
  html += QString(".del { background: %1; }").arg(delBgStr);
  html += ".code { white-space: pre-wrap; word-break: break-all; }";
  html += "</style></head><body><table>";

  for (const auto &line : m_lines) {
    QString escaped = htmlEscape(line.content);
    QString oldLn = line.oldLineNum > 0 ? QString::number(line.oldLineNum) : "";
    QString newLn = line.newLineNum > 0 ? QString::number(line.newLineNum) : "";

    if (line.prefix == 'd') {

      QString filename = line.content;
      int bIdx = filename.lastIndexOf(" b/");
      if (bIdx > 0)
        filename = filename.mid(bIdx + 3);
      html += QString("<tr><td colspan=\"3\" class=\"file\">📄 %1</td></tr>")
                  .arg(htmlEscape(filename));
    } else if (line.prefix == '@') {
      html += QString("<tr><td colspan=\"3\" class=\"hunk\">%1</td></tr>")
                  .arg(escaped);
    } else if (line.prefix == 'i' || line.prefix == 'h') {

      continue;
    } else if (line.prefix == '+') {
      html += QString("<tr class=\"add\"><td class=\"ln\">%1</td>"
                      "<td class=\"gutter gutter-add\"></td>"
                      "<td class=\"code\">+%2</td></tr>")
                  .arg(newLn, escaped);
    } else if (line.prefix == '-') {
      html += QString("<tr class=\"del\"><td class=\"ln\">%1</td>"
                      "<td class=\"gutter gutter-del\"></td>"
                      "<td class=\"code\">-%2</td></tr>")
                  .arg(oldLn, escaped);
    } else {
      html += QString("<tr><td class=\"ln\">%1</td>"
                      "<td class=\"gutter\"></td>"
                      "<td class=\"code\"> %2</td></tr>")
                  .arg(oldLn.isEmpty() ? newLn : oldLn, escaped);
    }
  }

  html += "</table></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::rebuildSplit() {
  QString addBgStr = QString("rgba(%1,%2,%3,0.15)")
                         .arg(m_theme.successColor.red())
                         .arg(m_theme.successColor.green())
                         .arg(m_theme.successColor.blue());
  QString delBgStr = QString("rgba(%1,%2,%3,0.15)")
                         .arg(m_theme.errorColor.red())
                         .arg(m_theme.errorColor.green())
                         .arg(m_theme.errorColor.blue());

  QString html;
  html += "<html><head><style>";
  html += QString("body { background: %1; color: %2; margin: 0; padding: 0; }")
              .arg(m_theme.backgroundColor.name())
              .arg(m_theme.foregroundColor.name());
  html += "table { width: 100%; border-collapse: collapse; }";
  html += QString(
      "td { font-family: 'SF Mono', Consolas, monospace; font-size: 12px; "
      "padding: 0 8px; line-height: 20px; vertical-align: top; }");
  html += QString(".ln { color: %1; text-align: right; width: 40px; "
                  "user-select: none; background: %2; }")
              .arg(m_theme.singleLineCommentFormat.name())
              .arg(m_theme.surfaceColor.name());
  html += QString(".sep { width: 2px; background: %1; padding: 0; }")
              .arg(m_theme.borderColor.name());
  html += QString(".gutter { width: 4px; padding: 0; }");
  html += QString(".gutter-add { background: %1; }")
              .arg(m_theme.successColor.name());
  html +=
      QString(".gutter-del { background: %1; }").arg(m_theme.errorColor.name());
  html += QString(".hunk { color: %1; background: %2; font-weight: 500; "
                  "padding: 8px 12px; }")
              .arg(m_theme.accentColor.name())
              .arg(m_theme.surfaceAltColor.name());
  html += QString(".file { color: %1; background: %2; font-weight: 600; "
                  "padding: 10px 12px; font-size: 13px; }")
              .arg(m_theme.foregroundColor.name())
              .arg(m_theme.surfaceColor.name());
  html += QString(".add { background: %1; }").arg(addBgStr);
  html += QString(".del { background: %1; }").arg(delBgStr);
  html +=
      QString(".empty { background: %1; }").arg(m_theme.surfaceAltColor.name());
  html += ".left, .right { width: 45%; white-space: pre-wrap; word-break: "
          "break-all; }";
  html += "</style></head><body><table>";

  int i = 0;
  while (i < m_lines.size()) {
    const auto &line = m_lines[i];
    QString escaped = htmlEscape(line.content);

    if (line.prefix == 'd') {
      QString filename = line.content;
      int bIdx = filename.lastIndexOf(" b/");
      if (bIdx > 0)
        filename = filename.mid(bIdx + 3);
      html += QString("<tr><td colspan=\"7\" class=\"file\">📄 %1</td></tr>")
                  .arg(htmlEscape(filename));
      i++;
    } else if (line.prefix == '@') {
      html += QString("<tr><td colspan=\"7\" class=\"hunk\">%1</td></tr>")
                  .arg(escaped);
      i++;
    } else if (line.prefix == 'i' || line.prefix == 'h') {
      i++;
      continue;
    } else if (line.prefix == '-') {
      QString leftLn =
          line.oldLineNum > 0 ? QString::number(line.oldLineNum) : "";
      QString rightContent, rightLn;
      bool hasPair = false;

      if (i + 1 < m_lines.size() && m_lines[i + 1].prefix == '+') {
        hasPair = true;
        rightContent = htmlEscape(m_lines[i + 1].content);
        rightLn = m_lines[i + 1].newLineNum > 0
                      ? QString::number(m_lines[i + 1].newLineNum)
                      : "";
        i++;
      }

      html +=
          QString(
              "<tr>"
              "<td class=\"ln\">%1</td><td class=\"gutter gutter-del\"></td>"
              "<td class=\"del left\">-%2</td>"
              "<td class=\"sep\"></td>"
              "<td class=\"ln\">%3</td><td class=\"gutter%4\"></td>"
              "<td class=\"%5 right\">%6</td>"
              "</tr>")
              .arg(leftLn, escaped, rightLn, hasPair ? " gutter-add" : "",
                   hasPair ? "add" : "empty",
                   hasPair ? "+" + rightContent : "");
      i++;
    } else if (line.prefix == '+') {
      QString newLn =
          line.newLineNum > 0 ? QString::number(line.newLineNum) : "";
      html +=
          QString(
              "<tr>"
              "<td class=\"ln\"></td><td class=\"gutter\"></td>"
              "<td class=\"empty left\"></td>"
              "<td class=\"sep\"></td>"
              "<td class=\"ln\">%1</td><td class=\"gutter gutter-add\"></td>"
              "<td class=\"add right\">+%2</td>"
              "</tr>")
              .arg(newLn, escaped);
      i++;
    } else {
      QString ln = line.oldLineNum > 0 ? QString::number(line.oldLineNum) : "";
      html += QString("<tr>"
                      "<td class=\"ln\">%1</td><td class=\"gutter\"></td>"
                      "<td class=\"left\"> %2</td>"
                      "<td class=\"sep\"></td>"
                      "<td class=\"ln\">%1</td><td class=\"gutter\"></td>"
                      "<td class=\"right\"> %2</td>"
                      "</tr>")
                  .arg(ln, escaped);
      i++;
    }
  }

  html += "</table></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::rebuildWord() {
  QString addBgStr = QString("rgba(%1,%2,%3,0.3)")
                         .arg(m_theme.successColor.red())
                         .arg(m_theme.successColor.green())
                         .arg(m_theme.successColor.blue());
  QString delBgStr = QString("rgba(%1,%2,%3,0.3)")
                         .arg(m_theme.errorColor.red())
                         .arg(m_theme.errorColor.green())
                         .arg(m_theme.errorColor.blue());

  QString html;
  html += "<html><head><style>";
  html += QString("body { background: %1; color: %2; margin: 0; padding: 0; }")
              .arg(m_theme.backgroundColor.name())
              .arg(m_theme.foregroundColor.name());
  html += "table { border-collapse: collapse; width: 100%; }";
  html += QString(
      "td { font-family: 'SF Mono', Consolas, monospace; font-size: 12px; "
      "padding: 0 8px; line-height: 20px; vertical-align: top; }");
  html += QString(".ln { color: %1; text-align: right; width: 50px; "
                  "padding-right: 12px; border-right: 1px solid %2; "
                  "user-select: none; background: %3; }")
              .arg(m_theme.singleLineCommentFormat.name())
              .arg(m_theme.borderColor.name())
              .arg(m_theme.surfaceColor.name());
  html += QString(".hunk { color: %1; background: %2; font-weight: 500; "
                  "padding: 8px 12px; }")
              .arg(m_theme.accentColor.name())
              .arg(m_theme.surfaceAltColor.name());
  html += QString(".file { color: %1; background: %2; font-weight: 600; "
                  "padding: 10px 12px; font-size: 13px; }")
              .arg(m_theme.foregroundColor.name())
              .arg(m_theme.surfaceColor.name());
  html +=
      QString(".add { background: %1; border-radius: 3px; padding: 1px 3px; }")
          .arg(addBgStr);
  html +=
      QString(".del { background: %1; border-radius: 3px; padding: 1px 3px; "
              "text-decoration: line-through; opacity: 0.8; }")
          .arg(delBgStr);
  html += ".code { white-space: pre-wrap; word-break: break-all; }";
  html += "</style></head><body><table>";

  int lineNum = 0;
  for (const auto &line : m_lines) {
    lineNum++;
    const bool hasWordMarkers =
        line.content.contains("{+") || line.content.contains("[-");
    QString lineNumStr = QString::number(lineNum);

    if (line.prefix == 'd') {
      QString filename = line.content;
      int bIdx = filename.lastIndexOf(" b/");
      if (bIdx > 0)
        filename = filename.mid(bIdx + 3);
      html += QString("<tr><td colspan=\"2\" class=\"file\">📄 %1</td></tr>")
                  .arg(htmlEscape(filename));
    } else if (line.prefix == '@') {
      html += QString("<tr><td colspan=\"2\" class=\"hunk\">%1</td></tr>")
                  .arg(htmlEscape(line.content));
    } else if (line.prefix == 'i' || line.prefix == 'h') {
      continue;
    } else if (line.prefix == '+' || line.prefix == '-') {
      if (hasWordMarkers) {
        html +=
            QString(
                "<tr><td class=\"ln\">%1</td><td class=\"code\">%2</td></tr>")
                .arg(lineNumStr, buildWordDiffLine(line.content));
      } else {
        html +=
            QString(
                "<tr><td class=\"ln\">%1</td><td class=\"code\">%2</td></tr>")
                .arg(lineNumStr,
                     styleToken(line.content,
                                line.prefix == '+' ? "add" : "del"));
      }
    } else if (hasWordMarkers) {
      html +=
          QString("<tr><td class=\"ln\">%1</td><td class=\"code\">%2</td></tr>")
              .arg(lineNumStr, buildWordDiffLine(line.content));
    } else {
      html +=
          QString("<tr><td class=\"ln\">%1</td><td class=\"code\">%2</td></tr>")
              .arg(lineNumStr, htmlEscape(line.content));
    }
  }

  html += "</table></body></html>";
  m_diffView->setHtml(html);
  if (m_summaryLabel) {
    m_summaryLabel->setText(m_summaryText.isEmpty() ? tr("Diff")
                                                    : m_summaryText);
  }
}

void GitDiffDialog::parseDiff(const QString &diffText) {
  m_lines.clear();
  m_addedCount = 0;
  m_deletedCount = 0;
  if (diffText.isEmpty()) {
    return;
  }

  int oldLine = 0;
  int newLine = 0;

  const QStringList rawLines = diffText.split('\n');
  for (const QString &rawLine : rawLines) {
    DiffLine line;
    line.oldLineNum = 0;
    line.newLineNum = 0;

    if (rawLine.startsWith("diff --git")) {
      line.prefix = 'd';
      line.content = rawLine;
      oldLine = 0;
      newLine = 0;
    } else if (rawLine.startsWith("index ") || rawLine.startsWith("new file") ||
               rawLine.startsWith("deleted file") ||
               rawLine.startsWith("--- ") || rawLine.startsWith("+++ ") ||
               rawLine.startsWith("rename ") ||
               rawLine.startsWith("similarity ")) {
      line.prefix = 'i';
      line.content = rawLine;
    } else if (rawLine.startsWith("@@")) {
      line.prefix = '@';
      line.content = rawLine;

      QRegularExpression hunkRe("@@ -(\\d+)(?:,\\d+)? \\+(\\d+)(?:,\\d+)? @@");
      QRegularExpressionMatch match = hunkRe.match(rawLine);
      if (match.hasMatch()) {
        oldLine = match.captured(1).toInt();
        newLine = match.captured(2).toInt();
      }
    } else if (rawLine.startsWith("+") && !rawLine.startsWith("+++")) {
      line.prefix = '+';
      line.content = rawLine.mid(1);
      line.newLineNum = newLine++;
      m_addedCount++;
    } else if (rawLine.startsWith("-") && !rawLine.startsWith("---")) {
      line.prefix = '-';
      line.content = rawLine.mid(1);
      line.oldLineNum = oldLine++;
      m_deletedCount++;
    } else {
      line.prefix = ' ';
      line.content = rawLine.startsWith(" ") ? rawLine.mid(1) : rawLine;
      line.oldLineNum = oldLine++;
      line.newLineNum = newLine++;
    }
    m_lines.append(line);
  }
}

void GitDiffDialog::parseFiles() {
  m_files.clear();
  FileSection currentFile;
  currentFile.startLine = 0;
  currentFile.addedCount = 0;
  currentFile.deletedCount = 0;

  for (int i = 0; i < m_lines.size(); ++i) {
    const auto &line = m_lines[i];
    if (line.prefix == 'd') {

      if (!currentFile.filename.isEmpty()) {
        m_files.append(currentFile);
      }

      QString content = line.content;
      int bIndex = content.lastIndexOf(" b/");
      if (bIndex > 0) {
        currentFile.filename = content.mid(bIndex + 3);
      } else {
        currentFile.filename = content;
      }
      currentFile.startLine = i;
      currentFile.addedCount = 0;
      currentFile.deletedCount = 0;
    } else if (line.prefix == '+') {
      currentFile.addedCount++;
    } else if (line.prefix == '-') {
      currentFile.deletedCount++;
    }
  }

  if (!currentFile.filename.isEmpty()) {
    m_files.append(currentFile);
  }
}

void GitDiffDialog::collectChangeBlocks() {
  m_changeBlocks.clear();
  int start = -1;
  for (int i = 0; i < m_lines.size(); ++i) {
    const QChar prefix = m_lines[i].prefix;
    const bool isChange = (prefix == '+' || prefix == '-');
    if (isChange && start < 0) {
      start = i;
    } else if (!isChange && start >= 0) {
      m_changeBlocks.append({start, i - 1});
      start = -1;
    }
  }
  if (start >= 0) {
    m_changeBlocks.append({start, m_lines.size() - 1});
  }
  m_currentChange = m_changeBlocks.isEmpty()
                        ? 0
                        : qMin(m_currentChange, m_changeBlocks.size() - 1);
}

void GitDiffDialog::scrollToChange(int index) {
  if (!m_diffView || m_changeBlocks.isEmpty() || index < 0 ||
      index >= m_changeBlocks.size()) {
    return;
  }
  int lineIndex = m_changeBlocks[index].first;
  QTextCursor cursor = m_diffView->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineIndex);
  m_diffView->setTextCursor(cursor);
  m_diffView->ensureCursorVisible();
}

void GitDiffDialog::scrollToFile(int fileIndex) {
  if (!m_diffView || fileIndex < 0 || fileIndex >= m_files.size()) {
    return;
  }
  int lineIndex = m_files[fileIndex].startLine;
  QTextCursor cursor = m_diffView->textCursor();
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineIndex);
  m_diffView->setTextCursor(cursor);
  m_diffView->ensureCursorVisible();
}

void GitDiffDialog::updateChangeCounter() {
  if (!m_changeCounterLabel) {
    return;
  }
  QString addStyle = QString("<span style='color: %1'>+%2</span>")
                         .arg(m_theme.successColor.name())
                         .arg(m_addedCount);
  QString delStyle = QString("<span style='color: %1'>-%2</span>")
                         .arg(m_theme.errorColor.name())
                         .arg(m_deletedCount);

  if (m_changeBlocks.isEmpty()) {
    m_changeCounterLabel->setText(QString("%1  %2").arg(addStyle, delStyle));
  } else {
    m_changeCounterLabel->setText(QString("%1/%2  %3  %4")
                                      .arg(m_currentChange + 1)
                                      .arg(m_changeBlocks.size())
                                      .arg(addStyle, delStyle));
  }
  m_changeCounterLabel->setTextFormat(Qt::RichText);
}

void GitDiffDialog::updateSearchCounter() {
  if (!m_searchCounterLabel || !m_searchField) {
    return;
  }
  const QString query = m_searchField->text();
  if (query.isEmpty()) {
    m_searchCounterLabel->clear();
    m_totalSearchMatches = 0;
    m_searchCounterLabel->setVisible(false);
    return;
  }
  m_totalSearchMatches = countSearchMatches(query);
  if (m_totalSearchMatches == 0) {
    m_searchCounterLabel->setText(tr("No results"));
  } else {
    m_searchCounterLabel->setText(tr("%1 found").arg(m_totalSearchMatches));
  }
  m_searchCounterLabel->setVisible(true);
}

int GitDiffDialog::countSearchMatches(const QString &query) {
  if (!m_diffView || query.isEmpty()) {
    return 0;
  }
  QTextDocument *doc = m_diffView->document();
  if (!doc) {
    return 0;
  }
  int count = 0;
  QTextCursor cursor(doc);
  while (!cursor.isNull() && !cursor.atEnd()) {
    cursor = doc->find(query, cursor);
    if (!cursor.isNull()) {
      count++;
    }
  }
  return count;
}

void GitDiffDialog::updateMinimap() {}

void GitDiffDialog::performSearch(bool backwards) {
  if (!m_diffView || !m_searchField) {
    return;
  }
  const QString query = m_searchField->text();
  if (query.isEmpty()) {
    return;
  }
  if (!m_diffView->hasFocus()) {
    m_diffView->setFocus();
  }
  QTextDocument::FindFlags flags;
  if (backwards) {
    flags |= QTextDocument::FindBackward;
  }
  if (!m_diffView->find(query, flags)) {
    QTextCursor cursor = m_diffView->textCursor();
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    m_diffView->setTextCursor(cursor);
    m_diffView->find(query, flags);
  }
}

QString GitDiffDialog::resolveWordDiff() {
  if (!m_git) {
    return QString();
  }
  if (m_target == DiffTarget::Commit) {
    return m_git->executeWordDiff({"show", "--word-diff", "--color=never",
                                   "--pretty=format:", m_targetId});
  }
  QString relativePath = m_targetId;
  if (!m_git->repositoryPath().isEmpty() &&
      relativePath.startsWith(m_git->repositoryPath())) {
    relativePath = relativePath.mid(m_git->repositoryPath().length() + 1);
  }
  if (m_staged) {
    return m_git->executeWordDiff({"diff", "--cached", "--word-diff",
                                   "--color=never", "--", relativePath});
  }
  return m_git->executeWordDiff(
      {"diff", "--word-diff", "--color=never", "--", relativePath});
}

QString GitDiffDialog::htmlEscape(const QString &text) const {
  QString escaped = text;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

QString GitDiffDialog::buildWordDiffLine(const QString &line) const {
  QString output;
  bool inDelete = false;
  bool inAdd = false;
  QString token;

  auto flushToken = [&]() {
    if (token.isEmpty()) {
      return;
    }
    if (inDelete) {
      output += styleToken(token, "del");
    } else if (inAdd) {
      output += styleToken(token, "add");
    } else {
      output += htmlEscape(token);
    }
    token.clear();
  };

  for (int i = 0; i < line.size(); ++i) {
    const QChar ch = line[i];
    if (ch == '{' && i + 1 < line.size() && line[i + 1] == '+') {
      flushToken();
      inAdd = true;
      i++;
      continue;
    }
    if (ch == '+' && inAdd && i + 1 < line.size() && line[i + 1] == '}') {
      flushToken();
      inAdd = false;
      i++;
      continue;
    }
    if (ch == '[' && i + 1 < line.size() && line[i + 1] == '-') {
      flushToken();
      inDelete = true;
      i++;
      continue;
    }
    if (ch == '-' && inDelete && i + 1 < line.size() && line[i + 1] == ']') {
      flushToken();
      inDelete = false;
      i++;
      continue;
    }
    token.append(ch);
  }
  flushToken();
  return output;
}

QString GitDiffDialog::styleToken(const QString &token,
                                  const QString &cssClass) const {
  return QString("<span class=\"%1\">%2</span>")
      .arg(cssClass, htmlEscape(token));
}
