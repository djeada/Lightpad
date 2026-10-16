#include "findreplacepanel.h"
#include "../../core/async/asyncworker.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int HighlightStartRole = Qt::UserRole + 3;
constexpr int HighlightLengthRole = Qt::UserRole + 4;
class MatchDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!index.data(HighlightStartRole).isValid()) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();
    const auto *style =
        opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    QTextLayout layout(text, opt.font);
    QTextCharFormat normal, highlight;
    normal.setForeground(opt.palette.color(
        opt.state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText
                                                   : QPalette::Text));
    highlight.setFontWeight(QFont::Bold);
    QColor background = opt.palette.color(QPalette::Highlight);
    background.setAlpha(90);
    highlight.setBackground(background);
    layout.setFormats({{0, int(text.size()), normal},
                       {index.data(HighlightStartRole).toInt(),
                        index.data(HighlightLengthRole).toInt(), highlight}});
    const QRect rect = option.rect.adjusted(5, 0, -5, 0);
    layout.beginLayout();
    auto line = layout.createLine();
    if (line.isValid())
      line.setLineWidth(qMax(1, rect.width()));
    layout.endLayout();
    painter->save();
    painter->setClipRect(rect);
    layout.draw(painter, QPointF(rect.left(),
                                 rect.top() + (rect.height() -
                                               layout.boundingRect().height()) /
                                                  2));
    painter->restore();
  }
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), QFontMetrics(option.font).height() + 8));
    return size;
  }
};
QString editorKey(QPlainTextEdit *editor, const QString &path) {
  return path.isEmpty()
             ? QStringLiteral("buffer:%1").arg(quintptr(editor), 0, 16)
             : SearchReplace::fileKey(path);
}
QString snippet(QString text) {
  text.replace('\n', QStringLiteral(" ↵ "));
  text.replace('\t', QStringLiteral(" → "));
  return text.size() > 240 ? text.left(240) + QChar(0x2026) : text;
}
QToolButton *tool(QWidget *parent, const QString &name, const QString &text,
                  const QString &tip, bool checkable = false) {
  auto *button = new QToolButton(parent);
  button->setObjectName(name);
  button->setText(text);
  button->setToolTip(tip);
  button->setAccessibleName(tip);
  button->setCheckable(checkable);
  button->setAutoRaise(true);
  button->setMinimumSize(30, 28);
  button->setCursor(Qt::PointingHandCursor);
  return button;
}
} // namespace

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent)
    : QWidget(parent), m_onlyFind(onlyFind) {
  setObjectName("FindReplacePanel");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 10, 14, 10);
  layout->setSpacing(7);

  m_header = new QWidget(this);
  auto *header = new QHBoxLayout(m_header);
  header->setContentsMargins(0, 0, 0, 0);
  auto *title = new QLabel(tr("Search"), this);
  auto titleFont = title->font();
  titleFont.setBold(true);
  title->setFont(titleFont);
  header->addWidget(title);
  m_scope = new QComboBox(this);
  m_scope->setObjectName("searchScope");
  m_scope->setAccessibleName(tr("Search scope"));
  m_scope->addItems(
      {tr("Current file"), tr("Selection"), tr("Open files"), tr("Project")});
  m_scope->setMinimumWidth(140);
  header->addWidget(m_scope);
  header->addStretch();
  m_undoButton = new QPushButton(tr("Undo replacement"), this);
  m_undoButton->setObjectName("undoReplacementBatch");
  m_undoButton->setToolTip(
      tr("Undo the last replacement batch, if those files have not changed"));
  header->addWidget(m_undoButton);
  m_resultsToggle = tool(this, "showSearchResults", tr("Results"),
                         tr("Show matching lines"), true);
  header->addWidget(m_resultsToggle);
  m_more = tool(this, "more", tr("Options"),
                tr("Search options and file filters"), true);
  header->addWidget(m_more);
  m_refresh = tool(this, "refreshSearch", QString(QChar(0x21bb)),
                   tr("Refresh search results (Ctrl+Enter)"));
  header->addWidget(m_refresh);
  m_stop = tool(this, "stopSearch", tr("Stop"),
                tr("Cancel the current search or replacement"));
  header->addWidget(m_stop);
  m_close =
      tool(this, "close", QString(QChar(0x00d7)), tr("Close search (Escape)"));
  header->addWidget(m_close);
  layout->addWidget(m_header);

  auto *findRow = new QHBoxLayout;
  m_queryLabel = new QLabel(tr("Find"), this);
  m_queryLabel->setMinimumWidth(55);
  findRow->addWidget(m_queryLabel);
  m_find = new QLineEdit(this);
  m_find->setObjectName("searchFind");
  m_find->setAccessibleName(tr("Find text"));
  m_find->setPlaceholderText(tr("Find text in your code…"));
  m_find->setClearButtonEnabled(true);
  m_find->setMinimumWidth(100);
  m_queryLabel->setBuddy(m_find);
  findRow->addWidget(m_find, 1);
  m_findHistoryButton = tool(this, "searchHistory", QString(QChar(0x25be)),
                             tr("Recent searches (Alt+Up / Alt+Down)"));
  findRow->addWidget(m_findHistoryButton);
  m_case = tool(this, "btnMatchCase", "Aa", tr("Match case (Alt+C)"), true);
  m_case->setShortcut(QKeySequence("Alt+C"));
  m_word =
      tool(this, "wholeWords", "ab", tr("Match whole words (Alt+W)"), true);
  m_word->setShortcut(QKeySequence("Alt+W"));
  m_regex =
      tool(this, "btnRegex", ".*", tr("Use regular expression (Alt+R)"), true);
  m_regex->setShortcut(QKeySequence("Alt+R"));
  findRow->addWidget(m_case);
  findRow->addWidget(m_word);
  findRow->addWidget(m_regex);
  m_previous = tool(this, "findPrevious", QString(QChar(0x2191)),
                    tr("Previous match (Shift+Enter / Shift+F3)"));
  m_next =
      tool(this, "find", QString(QChar(0x2193)), tr("Next match (Enter / F3)"));
  findRow->addWidget(m_previous);
  findRow->addWidget(m_next);
  layout->addLayout(findRow);

  m_replaceRow = new QWidget(this);
  auto *replaceRow = new QHBoxLayout(m_replaceRow);
  replaceRow->setContentsMargins(0, 0, 0, 0);
  auto *replaceLabel = new QLabel(tr("Replace"), this);
  replaceLabel->setMinimumWidth(55);
  replaceRow->addWidget(replaceLabel);
  m_replace = new QLineEdit(this);
  m_replace->setObjectName("fieldReplace");
  m_replace->setAccessibleName(tr("Replacement text"));
  m_replace->setPlaceholderText(tr("Replace with… (empty deletes matches)"));
  m_replace->setClearButtonEnabled(true);
  m_replace->setToolTip(
      tr("Regex replacements: $1, \\1, ${name}, $0. Use $$ for a literal "
         "dollar sign and \\\\ for a literal backslash."));
  m_replace->setMinimumWidth(100);
  replaceLabel->setBuddy(m_replace);
  replaceRow->addWidget(m_replace, 1);
  m_replaceHistoryButton =
      tool(this, "replacementHistory", QString(QChar(0x25be)),
           tr("Recent replacements"));
  replaceRow->addWidget(m_replaceHistoryButton);
  m_preserve = tool(this, "preserveCase", "AB",
                    tr("Preserve the matched text’s case"), true);
  replaceRow->addWidget(m_preserve);
  m_replaceOne = new QPushButton(tr("Replace"), this);
  m_replaceOne->setObjectName("replaceSingle");
  m_replaceOne->setToolTip(tr("Replace the selected match (Ctrl+Shift+1); "
                              "otherwise select the next match"));
  replaceRow->addWidget(m_replaceOne);
  m_previewButton = new QPushButton(tr("Preview changes"), this);
  m_previewButton->setObjectName("replaceAll");
  m_previewButton->setToolTip(
      tr("Review all replacements before applying (Ctrl+Alt+Enter)"));
  replaceRow->addWidget(m_previewButton);
  layout->addWidget(m_replaceRow);

  m_options = new QWidget(this);
  auto *advanced = new QVBoxLayout(m_options);
  advanced->setContentsMargins(59, 0, 0, 0);
  advanced->setSpacing(7);
  auto *flags = new QHBoxLayout;
  m_wrap = new QCheckBox(tr("Wrap around"), this);
  m_wrap->setObjectName("wrapSearch");
  m_wrap->setChecked(true);
  m_escapes = new QCheckBox(tr("Escape sequences"), this);
  m_escapes->setObjectName("escapeSequences");
  m_escapes->setToolTip(tr("Interpret \\n, \\r, \\t and \\\\ in literal search "
                           "and replacement. Regex search uses regex syntax."));
  m_dotAll = new QCheckBox(tr(". matches newline"), this);
  m_dotAll->setObjectName("dotMatchesNewline");
  flags->addWidget(m_wrap);
  flags->addWidget(m_escapes);
  flags->addWidget(m_dotAll);
  flags->addStretch();
  advanced->addLayout(flags);
  m_filters = new QWidget(this);
  auto *filters = new QHBoxLayout(m_filters);
  filters->setContentsMargins(0, 0, 0, 0);
  auto *includeLabel = new QLabel(tr("Include"), this);
  filters->addWidget(includeLabel);
  m_include = new QLineEdit(this);
  m_include->setObjectName("fileMaskEdit");
  m_include->setAccessibleName(tr("Files to include"));
  m_include->setPlaceholderText(tr("e.g. *.cpp, src/**"));
  m_include->setClearButtonEnabled(true);
  m_include->setToolTip(tr("Comma-separated filename or relative-path globs. "
                           "Leave empty for all text files."));
  includeLabel->setBuddy(m_include);
  filters->addWidget(m_include, 1);
  auto *excludeLabel = new QLabel(tr("Exclude"), this);
  filters->addWidget(excludeLabel);
  m_exclude = new QLineEdit(this);
  m_exclude->setObjectName("excludeFiles");
  m_exclude->setAccessibleName(tr("Files to exclude"));
  m_exclude->setText(ProjectSearch::Options{}.excludes);
  m_exclude->setClearButtonEnabled(true);
  m_exclude->setToolTip(tr("Comma-separated globs. Matching directories are "
                           "skipped, including their contents."));
  excludeLabel->setBuddy(m_exclude);
  filters->addWidget(m_exclude, 1);
  advanced->addWidget(m_filters);
  layout->addWidget(m_options);

  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("searchResults");
  m_tree->setAccessibleName(tr("Search results and replacement preview"));
  m_tree->setColumnCount(3);
  m_tree->setHeaderLabels({tr("Location"), tr("Before"), tr("After")});
  m_tree->setUniformRowHeights(true);
  m_tree->setItemDelegate(new MatchDelegate(m_tree));
  m_tree->setAllColumnsShowFocus(true);
  m_tree->setTextElideMode(Qt::ElideRight);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->setMinimumHeight(160);
  m_tree->setMaximumHeight(260);
  m_tree->header()->setStretchLastSection(true);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_tree->setColumnWidth(0, 280);
  layout->addWidget(m_tree);

  m_pager = new QWidget(this);
  auto *pager = new QHBoxLayout(m_pager);
  pager->setContentsMargins(0, 0, 0, 0);
  pager->addStretch();
  m_prevPage = tool(this, "previousResultsPage", tr("Previous page"),
                    tr("Previous page of results"));
  m_nextPage = tool(this, "nextResultsPage", tr("Next page"),
                    tr("Next page of results"));
  m_pageLabel = new QLabel(this);
  pager->addWidget(m_prevPage);
  pager->addWidget(m_pageLabel);
  pager->addWidget(m_nextPage);
  layout->addWidget(m_pager);

  m_footer = new QWidget(this);
  auto *footer = new QHBoxLayout(m_footer);
  footer->setContentsMargins(0, 0, 0, 0);
  m_status = new QLabel(this);
  m_status->setObjectName("searchStatus");
  m_status->setTextFormat(Qt::PlainText);
  m_status->setWordWrap(true);
  m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_status->setAccessibleName(tr("Search status"));
  footer->addWidget(m_status, 1);
  m_applyButton = new QPushButton(this);
  m_applyButton->setObjectName("applyReplacements");
  footer->addWidget(m_applyButton);
  layout->addWidget(m_footer);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  m_timer->setInterval(180);
  connect(m_timer, &QTimer::timeout, this,
          &FindReplacePanel::refreshSearchResults);
  for (auto *field : {m_find, m_replace, m_include, m_exclude}) {
    field->installEventFilter(this);
    connect(field, &QLineEdit::textChanged, this, [this]() { invalidate(); });
  }
  m_tree->installEventFilter(this);
  for (auto *button : {m_case, m_word, m_regex, m_preserve})
    connect(button, &QToolButton::toggled, this, [this]() {
      invalidate();
      updateUi();
    });
  for (auto *box : {m_escapes, m_dotAll})
    connect(box, &QCheckBox::toggled, this, [this]() { invalidate(); });
  connect(m_scope, &QComboBox::currentIndexChanged, this, [this]() {
    if (scope() == SearchReplace::Scope::Selection) {
      m_selection = m_editor ? m_editor->textCursor() : QTextCursor();
    }
    invalidate();
    updateUi();
  });
  connect(m_more, &QToolButton::toggled, this, [this]() { updateUi(); });
  connect(m_resultsToggle, &QToolButton::toggled, this,
          [this]() { updateUi(); });
  connect(m_next, &QToolButton::clicked, this, &FindReplacePanel::findNext);
  connect(m_previous, &QToolButton::clicked, this,
          &FindReplacePanel::findPrevious);
  connect(m_close, &QToolButton::clicked, this, &FindReplacePanel::closePanel);
  connect(m_refresh, &QToolButton::clicked, this,
          &FindReplacePanel::refreshSearchResults);
  connect(m_stop, &QToolButton::clicked, this, [this]() {
    if (m_busy && m_writeCancel) {
      m_writeCancel->store(true);
      updateStatus(tr("Cancelling and restoring any files already written…"));
    } else {
      cancelSearch();
      m_previewRequested = false;
      m_stale = true;
      updateStatus(tr("Search stopped. Press Enter to search again."));
      updateUi();
    }
  });
  connect(m_replaceOne, &QPushButton::clicked, this,
          &FindReplacePanel::replaceOne);
  connect(m_previewButton, &QPushButton::clicked, this,
          &FindReplacePanel::previewReplacements);
  connect(m_applyButton, &QPushButton::clicked, this,
          &FindReplacePanel::applyReplacements);
  connect(m_undoButton, &QPushButton::clicked, this,
          &FindReplacePanel::undoReplacementBatch);
  connect(m_findHistoryButton, &QToolButton::clicked, this,
          [this]() { showHistory(m_find, m_findHistory); });
  connect(m_replaceHistoryButton, &QToolButton::clicked, this,
          [this]() { showHistory(m_replace, m_replaceHistory); });
  connect(m_prevPage, &QToolButton::clicked, this, [this]() {
    --m_page;
    displayResults();
  });
  connect(m_nextPage, &QToolButton::clicked, this, [this]() {
    ++m_page;
    displayResults();
  });
  connect(m_tree, &QTreeWidget::itemChanged, this,
          &FindReplacePanel::onItemChanged);
  connect(m_tree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item) {
            if (!item || !item->parent())
              return;
            const int index = item->data(0, Qt::UserRole + 1).toInt(),
                      id = m_requestId;
            QTimer::singleShot(0, this, [this, index, id]() {
              if (id == m_requestId)
                navigateTo(index, true);
            });
          });
  connect(m_tree, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem *item, int column) {
            if (!item || !item->parent() || (m_preview && column == 0))
              return;
            const int index = item->data(0, Qt::UserRole + 1).toInt(),
                      id = m_requestId;
            QTimer::singleShot(0, this, [this, index, id]() {
              if (id == m_requestId)
                navigateTo(index);
            });
          });
  updateStatus();
  updateUi();
}

FindReplacePanel::~FindReplacePanel() {
  cancelSearch();
  if (m_writeCancel)
    m_writeCancel->store(true);
}

void FindReplacePanel::toggleExtensionVisibility() {
  setReplaceVisibility(m_onlyFind);
}
void FindReplacePanel::setReplaceVisibility(bool flag) { setOnlyFind(!flag); }
bool FindReplacePanel::isOnlyFind() const { return m_onlyFind; }
void FindReplacePanel::setOnlyFind(bool flag) {
  const bool removePreview = flag && m_preview;
  m_onlyFind = flag;
  if (flag) {
    m_preview = false;
    m_previewRequested = false;
  }
  if (removePreview && !m_stale)
    displayResults();
  updateUi();
}
void FindReplacePanel::setDocument(QTextDocument *document) {
  if (m_editor && m_editor->document() != document)
    m_editor->setDocument(document);
}
void FindReplacePanel::setEditor(QPlainTextEdit *editor, const QString &path) {
  const bool changed = m_editor != editor || m_editorPath != path;
  m_editor = editor;
  m_editorPath = path;
  if (changed) {
    if (!m_navigating && (scope() == SearchReplace::Scope::CurrentFile ||
                          scope() == SearchReplace::Scope::Selection)) {
      m_selection = editor ? editor->textCursor() : QTextCursor();
      invalidate();
    }
    watchDocuments();
  }
}
void FindReplacePanel::setDocumentProvider(
    std::function<QVector<OpenDocument>()> provider) {
  m_documents = std::move(provider);
  watchDocuments();
  invalidate();
}
void FindReplacePanel::setProjectPath(const QString &path) {
  if (m_projectPath == path)
    return;
  m_projectPath = path;
  m_scope->setItemData(int(SearchReplace::Scope::Project), path,
                       Qt::ToolTipRole);
  if (isGlobalMode())
    invalidate();
}
void FindReplacePanel::setGlobalMode(bool enabled) {
  setScope(enabled ? SearchReplace::Scope::Project
                   : SearchReplace::Scope::CurrentFile);
}
bool FindReplacePanel::isGlobalMode() const {
  return scope() == SearchReplace::Scope::Project;
}
void FindReplacePanel::setScope(SearchReplace::Scope value) {
  m_scope->setCurrentIndex(int(value));
}
SearchReplace::Scope FindReplacePanel::scope() const {
  return SearchReplace::Scope(m_scope->currentIndex());
}
void FindReplacePanel::setFocusOnSearchBox() {
  m_find->setFocus();
  m_find->selectAll();
}
void FindReplacePanel::setSearchText(const QString &text) {
  m_find->setText(text);
}
void FindReplacePanel::setSearchCursorPosition(int position) {
  m_find->setCursorPosition(position);
}
bool FindReplacePanel::isBusy() const { return m_busy; }
bool FindReplacePanel::isVimCommandMode() const { return m_vim; }
void FindReplacePanel::setSearchPrefix(const QString &prefix) {
  m_queryLabel->setText(prefix);
}

void FindReplacePanel::setVimCommandMode(bool enabled) {
  if (m_vim == enabled)
    return;
  if (enabled) {
    m_savedQuery = m_find->text();
    m_savedScope = scope();
    m_savedOnlyFind = m_onlyFind;
    cancelSearch();
    m_timer->stop();
    m_vim = true;
  } else {
    m_vim = false;
    m_keepVimHighlight = true;
    const QSignalBlocker blockFind(m_find), blockScope(m_scope);
    m_queryLabel->setText(tr("Find"));
    m_find->setText(m_savedQuery);
    setScope(m_savedScope);
    m_onlyFind = m_savedOnlyFind;
    m_stale = true;
    m_preview = false;
    m_previewRequested = false;
  }
  updateUi();
}

QVector<FindReplacePanel::OpenDocument> FindReplacePanel::documents() const {
  auto result = m_documents ? m_documents() : QVector<OpenDocument>{};
  if (m_editor) {
    const auto found = std::find_if(
        result.cbegin(), result.cend(),
        [this](const OpenDocument &d) { return d.editor == m_editor; });
    if (found == result.cend())
      result.append({currentKey(), m_editorPath,
                     m_editorPath.isEmpty()
                         ? tr("Untitled")
                         : QFileInfo(m_editorPath).fileName(),
                     m_editor});
  }
  for (auto &doc : result)
    if (doc.editor && doc.key.isEmpty())
      doc.key = editorKey(doc.editor, doc.path);
  return result;
}
QString FindReplacePanel::currentKey() const {
  if (m_documents) {
    for (const auto &doc : m_documents())
      if (doc.editor == m_editor && !doc.key.isEmpty())
        return doc.key;
  }
  return editorKey(m_editor, m_editorPath);
}
SearchReplace::Options FindReplacePanel::options() const {
  SearchReplace::Options value;
  value.query = m_find->text();
  value.replacement = m_replace->text();
  value.matchCase = m_case->isChecked();
  value.wholeWord = m_word->isChecked();
  value.regex = m_regex->isChecked();
  value.preserveCase = m_preserve->isChecked();
  value.escapeSequences = m_escapes->isChecked();
  value.dotMatchesNewline = m_dotAll->isChecked();
  return value;
}
SearchReplace::Request FindReplacePanel::request() const {
  SearchReplace::Request value;
  value.options = options();
  value.scope = scope();
  value.currentKey = currentKey();
  value.files.rootPath = m_projectPath;
  value.files.includes = m_include->text();
  value.files.excludes = m_exclude->text();
  value.selectionStart = m_selection.selectionStart();
  value.selectionEnd = m_selection.selectionEnd();
  for (const auto &doc : documents()) {
    if (!doc.editor)
      continue;
    value.buffers.append(
        {doc.key, doc.path, doc.label, doc.editor->toPlainText(), {}, true});
  }
  return value;
}
void FindReplacePanel::watchDocuments() {
  for (const auto &connection : m_documentConnections)
    disconnect(connection);
  m_documentConnections.clear();
  QSet<QTextDocument *> seen;
  for (const auto &doc : documents()) {
    if (!doc.editor || seen.contains(doc.editor->document()))
      continue;
    auto *document = doc.editor->document();
    seen.insert(document);
    if (!m_generations.contains(document))
      m_generations.insert(document, 0);
    m_documentConnections.append(
        connect(document, &QTextDocument::contentsChanged, this,
                [this, document, previous = document->toPlainText()]() mutable {
                  const QString current = document->toPlainText();
                  if (current == previous)
                    return; // Syntax highlighting also emits contentsChanged.
                  previous = current;
                  ++m_generations[document];
                  if (!m_applying && !m_navigating)
                    invalidate(false);
                }));
    m_documentConnections.append(
        connect(document, &QObject::destroyed, this, [this]() {
          if (!m_applying && !m_navigating)
            QTimer::singleShot(0, this, [this]() { invalidate(false); });
        }));
  }
  for (auto it = m_generations.begin(); it != m_generations.end();) {
    if (!seen.contains(it.key()))
      it = m_generations.erase(it);
    else
      ++it;
  }
}
void FindReplacePanel::cancelSearch() {
  ++m_requestId;
  if (m_searchCancel)
    m_searchCancel->store(true);
  m_searchCancel.reset();
  m_searching = false;
}
void FindReplacePanel::invalidate(bool clearHighlight) {
  if (m_vim || m_applying)
    return;
  cancelSearch();
  m_stale = true;
  m_preview = false;
  m_previewRequested = false;
  if (m_highlight && clearHighlight && isVisible())
    m_highlight(QRegularExpression());
  m_current = -1;
  m_pendingDirection = 0;
  m_tree->clear();
  if (isVisible() && !m_busy)
    m_timer->start();
  updateStatus();
  updateUi();
}
void FindReplacePanel::refreshSearchResults() {
  m_timer->stop();
  if (m_vim || m_busy || !isVisible())
    return;
  cancelSearch();
  watchDocuments();
  m_stale = true;
  m_preview = false;
  if (m_find->text().isEmpty()) {
    m_results = {};
    m_stale = false;
    m_previewRequested = false;
    displayResults();
    updateStatus();
    updateUi();
    emit searchCompleted(0);
    return;
  }
  if (scope() == SearchReplace::Scope::Selection &&
      (m_selection.isNull() || !m_selection.hasSelection())) {
    m_results = {};
    m_results.error =
        tr("Select text in the editor, then choose Selection scope.");
    m_stale = false;
    updateStatus();
    updateUi();
    emit searchCompleted(0);
    return;
  }
  auto input = request();
  auto output = std::make_shared<SearchReplace::Results>();
  auto cancel = std::make_shared<std::atomic_bool>(false);
  m_searchCancel = cancel;
  const int id = m_requestId;
  m_searching = true;
  m_tree->clear();
  m_page = 0;
  updateStatus(tr("Searching…"));
  updateUi();
  auto *task = new AsyncTask(
      [input = std::move(input), output, cancel](AsyncTask *worker) {
        *output = SearchReplace::search(
            input, [cancel]() { return cancel->load(); },
            [worker](int count) {
              worker->reportProgress(
                  0, QObject::tr("Searching… %1 files scanned").arg(count));
            });
      });
  connect(task, &AsyncTask::progress, this,
          [this, id](int, const QString &message) {
            if (id == m_requestId)
              updateStatus(message);
          });
  connect(task, &AsyncTask::finished, this, [this, id, output, cancel]() {
    if (id != m_requestId || cancel->load())
      return;
    m_searching = false;
    m_stale = false;
    m_searchCancel.reset();
    m_results = std::move(*output);
    m_current = -1;
    m_preview = m_previewRequested && m_results.error.isEmpty() &&
                !m_results.truncated && !m_results.cancelled;
    m_previewRequested = false;
    remember(m_findHistory, m_find->text());
    if (m_highlight && scope() != SearchReplace::Scope::Selection)
      m_highlight(SearchReplace::pattern(options()));
    displayResults();
    updateStatus();
    updateUi();
    emit searchCompleted(m_results.matchCount);
    const int direction = m_pendingDirection;
    m_pendingDirection = 0;
    if (direction)
      navigate(direction);
  });
  connect(task, &AsyncTask::error, this, [this, id](const QString &message) {
    if (id != m_requestId)
      return;
    m_searching = false;
    m_stale = true;
    m_previewRequested = false;
    updateStatus(message);
    updateUi();
    emit searchCompleted(0);
  });
  AsyncThreadPool::instance().submit(task);
}

void FindReplacePanel::updateUi() {
  const bool regular = !m_vim;
  m_header->setVisible(regular);
  m_replaceRow->setVisible(regular && !m_onlyFind);
  m_options->setVisible(regular && m_more->isChecked());
  m_filters->setVisible(isGlobalMode());
  m_dotAll->setEnabled(m_regex->isChecked());
  m_footer->setVisible(regular);
  m_findHistoryButton->setVisible(regular);
  for (auto *button : {m_case, m_word, m_regex, m_next, m_previous})
    button->setVisible(regular);
  const bool showResults = regular && !m_stale && m_results.matchCount > 0 &&
                           (m_preview || isGlobalMode() ||
                            scope() == SearchReplace::Scope::OpenFiles ||
                            m_resultsToggle->isChecked());
  m_tree->setVisible(showResults);
  m_tree->setColumnHidden(2, !m_preview);
  m_tree->headerItem()->setText(1,
                                m_preview ? tr("Before") : tr("Matching line"));
  m_pager->setVisible(showResults && m_results.matchCount > PageSize);
  m_stop->setVisible(m_busy || m_searching);
  m_close->setEnabled(!m_busy);
  m_refresh->setEnabled(!m_busy);
  m_undoButton->setVisible(!m_undo.empty());
  m_undoButton->setEnabled(!m_busy);
  const bool hasQuery = !m_find->text().isEmpty();
  m_next->setEnabled(hasQuery && !m_busy);
  m_previous->setEnabled(hasQuery && !m_busy);
  m_replaceOne->setEnabled(hasQuery && !m_busy && !m_searching);
  m_previewButton->setEnabled(hasQuery && !m_busy);
  for (auto *widget : QVector<QWidget *>{m_scope, m_find, m_replace, m_include,
                                         m_exclude, m_case, m_word, m_regex,
                                         m_preserve, m_escapes, m_dotAll})
    widget->setEnabled(!m_busy);
  m_tree->setEnabled(!m_busy);
  updateApplyButton();
}
void FindReplacePanel::updateStatus(const QString &text) {
  if (!text.isEmpty()) {
    m_status->setText(text);
    return;
  }
  if (m_find->text().isEmpty()) {
    m_status->setText(
        tr("Enter to find next · Shift+Enter for previous · Escape to close"));
    return;
  }
  if (m_stale) {
    m_status->setText(tr("Results need refreshing…"));
    return;
  }
  if (!m_results.error.isEmpty()) {
    m_status->setText(m_results.error);
    return;
  }
  const QString matches = m_results.matchCount == 1
                              ? tr("1 match")
                              : tr("%1 matches").arg(m_results.matchCount);
  const QString files = m_results.files.size() == 1
                            ? tr("1 file")
                            : tr("%1 files").arg(m_results.files.size());
  QString message =
      m_results.matchCount == 0
          ? tr("No matches · %1 files searched").arg(m_results.scannedFiles)
          : tr("%1 in %2").arg(matches, files);
  if (m_current >= 0)
    message = tr("Match %1 of %2 · %3")
                  .arg(m_current + 1)
                  .arg(m_results.matchCount)
                  .arg(files);
  if (m_results.truncated)
    message += tr(" · Limit reached; narrow the search to enable replacement");
  if (m_results.cancelled)
    message += tr(" · Cancelled; refresh before replacing");
  if (m_results.skippedFiles)
    message += tr(" · %1 files skipped (binary, encoding, size, or access)")
                   .arg(m_results.skippedFiles);
  if (m_preview)
    message += tr(" · Open editors stay unsaved; closed files are saved");
  if (scope() == SearchReplace::Scope::Selection)
    message += tr(" · Within captured selection");
  m_status->setText(message);
}
void FindReplacePanel::updateApplyButton() {
  int count = 0;
  for (const auto &file : m_results.files)
    for (const auto &hit : file.occurrences)
      if (hit.selected &&
          file.source.content.mid(hit.location.matchStart,
                                  hit.location.matchLength) != hit.replacement)
        ++count;
  m_applyButton->setText(count == 1 ? tr("Apply 1 replacement")
                                    : tr("Apply %1 replacements").arg(count));
  m_applyButton->setVisible(m_preview && !m_onlyFind && !m_vim);
  m_applyButton->setEnabled(count > 0 && !m_stale && !m_busy && !m_searching &&
                            !m_results.truncated && !m_results.cancelled &&
                            m_results.error.isEmpty());
}
void FindReplacePanel::displayResults() {
  const QSignalBlocker blocker(m_tree);
  const int scroll = m_tree->verticalScrollBar()->value();
  m_tree->clear();
  const int pageCount =
      qMax(1, (m_results.matchCount + PageSize - 1) / PageSize);
  m_page = qBound(0, m_page, pageCount - 1);
  const int begin = m_page * PageSize,
            end = qMin(begin + PageSize, m_results.matchCount);
  int index = 0;
  for (int f = 0; f < m_results.files.size(); ++f) {
    const auto &file = m_results.files.at(f);
    const int fileEnd = index + file.occurrences.size();
    if (fileEnd <= begin || index >= end) {
      index = fileEnd;
      continue;
    }
    QString path = file.source.path.isEmpty()
                       ? file.source.label
                       : QDir(m_projectPath).relativeFilePath(file.source.path);
    if (path.isEmpty())
      path = tr("Untitled");
    auto *group = new QTreeWidgetItem(m_tree);
    group->setFlags(group->flags() & ~Qt::ItemIsUserCheckable);
    group->setText(0, tr("%1  (%2)").arg(path).arg(file.occurrences.size()));
    group->setToolTip(0, file.source.path.isEmpty() ? file.source.label
                                                    : file.source.path);
    group->setText(1, file.source.openBuffer ? tr("Open editor")
                                             : tr("Project file"));
    group->setData(0, Qt::UserRole, f);
    group->setData(0, Qt::UserRole + 1, -1);
    auto font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    if (m_preview) {
      int eligible = 0, selected = 0;
      for (const auto &hit : file.occurrences) {
        if (file.source.content.mid(hit.location.matchStart,
                                    hit.location.matchLength) !=
            hit.replacement) {
          ++eligible;
          selected += hit.selected;
        }
      }
      if (eligible) {
        group->setFlags(group->flags() | Qt::ItemIsUserCheckable);
        group->setCheckState(0, selected == 0          ? Qt::Unchecked
                                : selected == eligible ? Qt::Checked
                                                       : Qt::PartiallyChecked);
      }
    }
    for (int h = 0; h < file.occurrences.size(); ++h, ++index) {
      if (index < begin || index >= end)
        continue;
      const auto &hit = file.occurrences.at(h);
      auto *item = new QTreeWidgetItem(group);
      item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
      item->setData(0, Qt::UserRole, f);
      item->setData(0, Qt::UserRole + 1, index);
      item->setData(0, Qt::UserRole + 2, h);
      item->setText(0, tr("Line %1:%2")
                           .arg(hit.location.lineNumber)
                           .arg(hit.location.columnNumber));
      const int matchOffset = hit.location.columnNumber - 1;
      const int snippetStart = qMax(0, matchOffset - 70);
      item->setText(1, (snippetStart ? QString(QChar(0x2026)) : QString()) +
                           snippet(hit.location.lineContent.mid(snippetStart)));
      const QString matched = file.source.content.mid(hit.location.matchStart,
                                                      hit.location.matchLength);
      const int highlightStart =
          snippet(hit.location.lineContent.mid(snippetStart,
                                               matchOffset - snippetStart))
              .size() +
          (snippetStart ? 1 : 0);
      item->setData(1, HighlightStartRole, highlightStart);
      item->setData(1, HighlightLengthRole,
                    snippet(matched.section('\n', 0, 0)).size());
      item->setToolTip(
          1, tr("Matched: %1\n\n%2")
                 .arg(matched.left(2000), hit.location.lineContent.left(2000)));
      QString after = hit.location.lineContent;
      after.replace(
          matchOffset,
          qMin(hit.location.matchLength, int(after.size()) - matchOffset),
          hit.replacement);
      item->setText(2, after.isEmpty() ? tr("(empty line)")
                                       : (snippetStart ? QString(QChar(0x2026))
                                                       : QString()) +
                                             snippet(after.mid(snippetStart)));
      item->setData(2, HighlightStartRole, highlightStart);
      item->setData(2, HighlightLengthRole, snippet(hit.replacement).size());
      item->setToolTip(2, tr("Replacement: %1\n\n%2")
                              .arg(hit.replacement.isEmpty()
                                       ? tr("(delete)")
                                       : hit.replacement.left(2000),
                                   after.left(2000)));
      if (m_preview && matched != hit.replacement) {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, hit.selected ? Qt::Checked : Qt::Unchecked);
      } else if (m_preview)
        item->setText(2, tr("(unchanged)"));
      if (index == m_current)
        m_tree->setCurrentItem(item);
    }
    group->setExpanded(true);
  }
  m_tree->verticalScrollBar()->setValue(scroll);
  m_prevPage->setEnabled(m_page > 0);
  m_nextPage->setEnabled(m_page + 1 < pageCount);
  m_pageLabel->setText(
      tr("%1–%2 of %3").arg(begin + 1).arg(end).arg(m_results.matchCount));
  updateUi();
}
void FindReplacePanel::onItemChanged(QTreeWidgetItem *item, int column) {
  if (!m_preview || column != 0 || !item)
    return;
  const int fileIndex = item->data(0, Qt::UserRole).toInt();
  if (fileIndex < 0 || fileIndex >= m_results.files.size())
    return;
  auto &file = m_results.files[fileIndex];
  const bool selected = item->checkState(0) == Qt::Checked;
  if (!item->parent()) {
    for (auto &hit : file.occurrences)
      hit.selected = selected;
    const QSignalBlocker blocker(m_tree);
    for (int i = 0; i < item->childCount(); ++i) {
      auto *child = item->child(i);
      if (child->flags().testFlag(Qt::ItemIsUserCheckable))
        child->setCheckState(0, selected ? Qt::Checked : Qt::Unchecked);
    }
  } else {
    const int hitIndex = item->data(0, Qt::UserRole + 2).toInt();
    if (hitIndex >= 0 && hitIndex < file.occurrences.size())
      file.occurrences[hitIndex].selected = selected;
    const QSignalBlocker blocker(m_tree);
    int count = 0, eligible = 0;
    for (const auto &hit : file.occurrences) {
      if (file.source.content.mid(hit.location.matchStart,
                                  hit.location.matchLength) !=
          hit.replacement) {
        ++eligible;
        count += hit.selected;
      }
    }
    item->parent()->setCheckState(0, count == 0 ? Qt::Unchecked
                                     : count == eligible
                                         ? Qt::Checked
                                         : Qt::PartiallyChecked);
  }
  updateApplyButton();
}
QPair<int, int> FindReplacePanel::occurrenceAt(int index) const {
  if (index < 0)
    return {-1, -1};
  for (int f = 0; f < m_results.files.size(); ++f) {
    if (index < m_results.files.at(f).occurrences.size())
      return {f, index};
    index -= m_results.files.at(f).occurrences.size();
  }
  return {-1, -1};
}
void FindReplacePanel::findNext() { navigate(1); }
void FindReplacePanel::findPrevious() { navigate(-1); }
void FindReplacePanel::navigate(int direction) {
  if (m_busy || m_vim || m_find->text().isEmpty())
    return;
  if (m_stale || m_searching) {
    m_pendingDirection = direction;
    if (!m_searching)
      refreshSearchResults();
    return;
  }
  if (m_results.matchCount == 0 || !m_results.error.isEmpty()) {
    updateStatus();
    return;
  }
  int target = -1;
  const auto previous = occurrenceAt(m_current);
  bool atCurrent = false;
  if (m_editor && previous.first >= 0) {
    const auto &file = m_results.files.at(previous.first);
    const auto &hit = file.occurrences.at(previous.second).location;
    const auto cursor = m_editor->textCursor();
    atCurrent = file.source.key == currentKey() &&
                cursor.selectionStart() == hit.matchStart &&
                cursor.selectionEnd() == hit.matchStart + hit.matchLength;
  }
  if (atCurrent)
    target = m_current + direction;
  else if (m_editor) {
    const int position = direction > 0
                             ? m_editor->textCursor().selectionEnd()
                             : m_editor->textCursor().selectionStart();
    int index = 0, fileBegin = -1, fileEnd = -1;
    for (const auto &file : m_results.files) {
      if (file.source.key == currentKey()) {
        fileBegin = index;
        fileEnd = index + file.occurrences.size();
        for (const auto &hit : file.occurrences) {
          if (direction > 0 && hit.location.matchStart >= position &&
              target < 0)
            target = index;
          if (direction < 0 && hit.location.matchStart < position)
            target = index;
          ++index;
        }
      } else
        index += file.occurrences.size();
    }
    if (target < 0 && fileBegin >= 0)
      target = direction > 0 ? fileEnd : fileBegin - 1;
    else if (target < 0)
      target = direction > 0 ? 0 : m_results.matchCount - 1;
  } else
    target = direction > 0 ? 0 : m_results.matchCount - 1;
  bool wrapped = false;
  if (target < 0 || target >= m_results.matchCount) {
    if (!m_wrap->isChecked()) {
      updateStatus(direction > 0 ? tr("End of results · Wrap around is off")
                                 : tr("Start of results · Wrap around is off"));
      return;
    }
    target = direction > 0 ? 0 : m_results.matchCount - 1;
    wrapped = true;
  }
  navigateTo(target);
  if (wrapped)
    updateStatus(m_status->text() + (direction > 0 ? tr(" · Wrapped to start")
                                                   : tr(" · Wrapped to end")));
}
void FindReplacePanel::navigateTo(int index, bool focusEditor) {
  if (m_stale || m_busy)
    return;
  const auto position = occurrenceAt(index);
  if (position.first < 0)
    return;
  const auto file = m_results.files.at(position.first);
  const auto hit = file.occurrences.at(position.second).location;
  const int requestId = m_requestId;
  QPointer<QPlainTextEdit> target;
  for (const auto &doc : documents())
    if (doc.key == file.source.key && doc.editor) {
      target = doc.editor;
      break;
    }
  m_navigating = true;
  if (!target && !file.source.path.isEmpty()) {
    emit navigateToFile(file.source.path, hit.lineNumber, hit.columnNumber,
                        hit.matchLength);
    for (const auto &doc : documents())
      if (doc.key == file.source.key && doc.editor) {
        target = doc.editor;
        break;
      }
  }
  if (requestId != m_requestId) {
    m_navigating = false;
    return;
  }
  if (target) {
    if (target->toPlainText() != file.source.content) {
      m_navigating = false;
      invalidate();
      updateStatus(tr("The file changed. Refreshing results…"));
      return;
    }
    if (m_activateEditor)
      m_activateEditor(target);
    setEditor(target, file.source.path);
    QTextCursor cursor(target->document());
    cursor.setPosition(hit.matchStart);
    cursor.setPosition(hit.matchStart + hit.matchLength,
                       QTextCursor::KeepAnchor);
    target->setTextCursor(cursor);
    target->ensureCursorVisible();
    if (focusEditor)
      target->setFocus();
  }
  m_navigating = false;
  m_current = index;
  if (m_highlight && scope() != SearchReplace::Scope::Selection)
    m_highlight(SearchReplace::pattern(options()));
  if (m_page != index / PageSize) {
    m_page = index / PageSize;
    displayResults();
  } else {
    for (int f = 0; f < m_tree->topLevelItemCount(); ++f) {
      auto *group = m_tree->topLevelItem(f);
      for (int h = 0; h < group->childCount(); ++h) {
        auto *item = group->child(h);
        if (item->data(0, Qt::UserRole + 1).toInt() == index) {
          m_tree->setCurrentItem(item);
          m_tree->scrollToItem(item);
          break;
        }
      }
    }
  }
  updateStatus();
}
void FindReplacePanel::remember(QStringList &history, const QString &value) {
  if (value.isEmpty())
    return;
  history.removeAll(value);
  history.prepend(value);
  while (history.size() > 20)
    history.removeLast();
}
void FindReplacePanel::showHistory(QLineEdit *field, QStringList &history) {
  QMenu menu(this);
  for (const auto &entry : history) {
    auto *action = menu.addAction(snippet(entry));
    action->setToolTip(entry);
    connect(action, &QAction::triggered, field, [field, entry]() {
      field->setText(entry);
      field->setFocus();
    });
  }
  if (history.isEmpty())
    menu.addAction(tr("No recent entries"))->setEnabled(false);
  menu.exec(field->mapToGlobal(QPoint(0, field->height())));
}
void FindReplacePanel::closePanel() {
  if (m_busy)
    return;
  if (m_vim && m_vimKey) {
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    m_vimKey(&escape);
  } else
    hide();
  if (m_focusEditor)
    m_focusEditor();
  else if (m_editor)
    m_editor->setFocus();
}
bool FindReplacePanel::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::ShortcutOverride && m_vim && obj == m_find) {
    event->accept();
    return true;
  }
  if (event->type() != QEvent::KeyPress)
    return QWidget::eventFilter(obj, event);
  auto *key = static_cast<QKeyEvent *>(event);
  if (m_vim && obj == m_find && m_vimKey) {
    m_vimKey(key);
    return true;
  }
  if (key->key() == Qt::Key_Escape) {
    closePanel();
    return true;
  }
  if (m_busy)
    return QWidget::eventFilter(obj, event);
  if (key->key() == Qt::Key_F3) {
    navigate(key->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
    return true;
  }
  if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
    if (obj == m_tree) {
      auto *item = m_tree->currentItem();
      if (item && item->parent())
        navigateTo(item->data(0, Qt::UserRole + 1).toInt(), true);
      return true;
    }
    if (key->modifiers() == Qt::ControlModifier)
      refreshSearchResults();
    else if (key->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
      if (m_preview)
        applyReplacements();
      else
        previewReplacements();
    } else if (obj == m_replace &&
               !key->modifiers().testFlag(Qt::ShiftModifier))
      replaceOne();
    else
      navigate(key->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1);
    return true;
  }
  if (obj == m_find && key->modifiers() == Qt::AltModifier &&
      (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
    if (!m_findHistory.isEmpty()) {
      m_historyIndex =
          qBound(0, m_historyIndex + (key->key() == Qt::Key_Up ? 1 : -1),
                 int(m_findHistory.size()) - 1);
      m_find->setText(m_findHistory.at(m_historyIndex));
    }
    return true;
  }
  if (obj == m_find && key->key() == Qt::Key_Down &&
      key->modifiers() == Qt::NoModifier && m_tree->isVisible()) {
    m_tree->setFocus();
    if (!m_tree->currentItem() && m_tree->topLevelItemCount()) {
      auto *group = m_tree->topLevelItem(0);
      m_tree->setCurrentItem(group->childCount() ? group->child(0) : group);
    }
    return true;
  }
  if (key->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      key->key() == Qt::Key_1) {
    replaceOne();
    return true;
  }
  return QWidget::eventFilter(obj, event);
}
void FindReplacePanel::hideEvent(QHideEvent *event) {
  m_timer->stop();
  cancelSearch();
  m_stale = true;
  m_preview = false;
  m_previewRequested = false;
  if (m_highlight && !m_vim && !m_keepVimHighlight)
    m_highlight(QRegularExpression());
  QWidget::hideEvent(event);
}
void FindReplacePanel::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (!m_vim) {
    m_keepVimHighlight = false;
    invalidate();
  }
}

void FindReplacePanel::previewReplacements() {
  if (m_busy || m_vim || m_find->text().isEmpty())
    return;
  setOnlyFind(false);
  m_previewRequested = true;
  refreshSearchResults();
}
void FindReplacePanel::replaceOne() {
  if (m_busy || m_vim)
    return;
  if (m_stale || m_searching || m_current < 0) {
    navigate(1);
    return;
  }
  const auto position = occurrenceAt(m_current);
  if (position.first < 0 || !m_editor) {
    navigate(1);
    return;
  }
  const auto &file = m_results.files.at(position.first);
  const auto &hit = file.occurrences.at(position.second);
  const auto cursor = m_editor->textCursor();
  if (file.source.key != currentKey() ||
      cursor.selectionStart() != hit.location.matchStart ||
      cursor.selectionEnd() !=
          hit.location.matchStart + hit.location.matchLength) {
    navigate(1);
    return;
  }
  if (m_editor->isReadOnly() ||
      m_editor->toPlainText() != file.source.content) {
    invalidate();
    updateStatus(
        tr("The editor changed or is read-only. Refresh before replacing."));
    return;
  }
  if (file.source.content.mid(hit.location.matchStart,
                              hit.location.matchLength) == hit.replacement) {
    navigate(1);
    return;
  }
  Batch batch;
  QString after = file.source.content;
  after.replace(hit.location.matchStart, hit.location.matchLength,
                hit.replacement);
  QSet<QTextDocument *> seen;
  for (const auto &doc : documents()) {
    if (doc.key != file.source.key || !doc.editor ||
        seen.contains(doc.editor->document()))
      continue;
    if (doc.editor->isReadOnly() ||
        doc.editor->toPlainText() != file.source.content) {
      updateStatus(tr(
          "Open editors disagree or are read-only. Refresh before replacing."));
      return;
    }
    seen.insert(doc.editor->document());
    batch.buffers.append(
        {doc.editor->document(),
         doc.key,
         file.source.content,
         after,
         {{hit.location.matchStart, hit.location.matchLength, hit.replacement}},
         -1});
  }
  executeBatch(std::move(batch), false);
}
void FindReplacePanel::applyReplacements() {
  if (!m_preview || m_stale || m_busy || m_searching)
    return;
  const auto changes = SearchReplace::selectedChanges(m_results);
  if (changes.isEmpty()) {
    updateStatus(tr("No changed replacements selected."));
    return;
  }
  Batch batch;
  const auto open = documents();
  for (const auto &change : changes) {
    bool found = false;
    QSet<QTextDocument *> seen;
    for (const auto &doc : open) {
      if (doc.key != change.source.key || !doc.editor)
        continue;
      found = true;
      if (doc.editor->isReadOnly() ||
          doc.editor->toPlainText() != change.source.content) {
        invalidate();
        updateStatus(tr("An open editor changed or is read-only. Preview the "
                        "replacements again."));
        return;
      }
      if (seen.contains(doc.editor->document()))
        continue;
      seen.insert(doc.editor->document());
      batch.buffers.append({doc.editor->document(), doc.key,
                            change.source.content, change.after, change.edits,
                            -1});
    }
    if (!found) {
      if (change.source.openBuffer) {
        invalidate();
        updateStatus(tr("An editor was closed or renamed. Preview the "
                        "replacements again."));
        return;
      }
      batch.disks.append(
          {change.source.path, change.source.bytes, change.afterBytes});
    }
  }
  remember(m_replaceHistory, m_replace->text());
  executeBatch(std::move(batch), false);
}
bool FindReplacePanel::validateBuffers(const Batch &batch, bool undo,
                                       QString *error) const {
  const auto open = documents();
  for (const auto &buffer : batch.buffers) {
    if (!buffer.document ||
        buffer.document->toPlainText() !=
            (undo ? buffer.after : buffer.before) ||
        (undo &&
         (m_generations.value(buffer.document, -1) != buffer.generation ||
          buffer.document->availableUndoSteps() != buffer.undoSteps ||
          !buffer.document->isUndoAvailable()))) {
      *error = tr("An editor changed or was closed. The replacement batch "
                  "cannot %1.")
                   .arg(undo ? tr("be undone") : tr("be applied"));
      return false;
    }
    bool found = false;
    for (const auto &doc : open) {
      if (doc.editor && doc.editor->document() == buffer.document &&
          doc.key == buffer.key) {
        found = true;
        break;
      }
    }
    if (!found) {
      *error = tr("An editor was renamed or closed. Refresh the preview before "
                  "replacing.");
      return false;
    }
  }
  // A file may have been opened while the disk worker was running. Do not let
  // that newly opened document become stale after a write or a rollback.
  for (const auto &disk : batch.disks) {
    for (const auto &doc : open) {
      if (!doc.editor || doc.key != SearchReplace::fileKey(disk.path))
        continue;
      const QString expected = SearchReplace::normalizedText(
          QString::fromUtf8(undo ? disk.after : disk.before));
      QString content = expected;
      if (content.startsWith(QChar(0xfeff)))
        content.remove(0, 1);
      QString other = SearchReplace::normalizedText(
          QString::fromUtf8(undo ? disk.before : disk.after));
      if (other.startsWith(QChar(0xfeff)))
        other.remove(0, 1);
      if (doc.editor->document()->isModified() ||
          (doc.editor->toPlainText() != content &&
           doc.editor->toPlainText() != other)) {
        *error =
            tr("An editor for %1 changed while the batch was being prepared.")
                .arg(disk.path);
        return false;
      }
    }
  }
  return true;
}
void FindReplacePanel::undoReplacementBatch() {
  if (m_busy || m_undo.empty())
    return;
  QString error;
  if (!validateBuffers(m_undo, true, &error)) {
    updateStatus(error);
    return;
  }
  for (const auto &doc : documents()) {
    if (!doc.editor)
      continue;
    for (const auto &buffer : m_undo.buffers) {
      if (doc.editor->document() == buffer.document &&
          doc.editor->isReadOnly()) {
        updateStatus(
            tr("An affected editor is read-only. The batch cannot be undone."));
        return;
      }
    }
  }
  executeBatch(m_undo, true);
}
void FindReplacePanel::setBusy(bool busy) {
  m_busy = busy;
  updateUi();
}
void FindReplacePanel::executeBatch(Batch batch, bool undo) {
  if (batch.empty())
    return;
  watchDocuments();
  QString error;
  if (!validateBuffers(batch, undo, &error)) {
    updateStatus(error);
    return;
  }
  cancelSearch();
  m_timer->stop();
  m_writeCancel = std::make_shared<std::atomic_bool>(false);
  const auto cancel = m_writeCancel;
  auto writes = batch.disks;
  if (undo)
    for (auto &disk : writes)
      std::swap(disk.before, disk.after);
  QStringList paths;
  for (const auto &disk : writes)
    paths.append(disk.path);
  if (m_diskGuard)
    m_diskGuard(paths, true);
  // Freeze affected editors for the short commit phase. QPointer still handles
  // tab closure, and the second validation catches programmatic document edits.
  auto frozen =
      std::make_shared<QVector<QPair<QPointer<QPlainTextEdit>, bool>>>();
  for (const auto &doc : documents()) {
    if (!doc.editor)
      continue;
    bool affected = false;
    for (const auto &buffer : batch.buffers)
      affected |= doc.editor->document() == buffer.document;
    for (const auto &disk : batch.disks)
      affected |= doc.key == SearchReplace::fileKey(disk.path);
    if (affected) {
      frozen->append({doc.editor, doc.editor->isReadOnly()});
      doc.editor->setReadOnly(true);
    }
  }
  setBusy(true);
  updateStatus(undo ? tr("Undoing replacement batch…")
                    : tr("Applying selected replacements…"));
  auto report = std::make_shared<SearchReplace::DiskReport>();
  auto *task = new AsyncTask([writes, report, cancel](AsyncTask *worker) {
    *report = SearchReplace::applyDiskChanges(
        writes, [cancel]() { return cancel->load(); },
        [worker](int done, int total) {
          worker->reportProgress(
              total ? done * 100 / total : 0,
              QObject::tr("Writing files… %1 of %2").arg(done).arg(total));
        });
  });
  connect(task, &AsyncTask::progress, this,
          [this](int, const QString &message) { updateStatus(message); });
  auto finish = [this, batch, writes, undo, paths, frozen, report,
                 cancel]() mutable {
    auto thaw = [frozen]() {
      for (const auto &entry : *frozen)
        if (entry.first)
          entry.first->setReadOnly(entry.second);
    };
    QString validationError;
    if (cancel->load())
      validationError = tr("Replacement cancelled.");
    if (report->success &&
        (cancel->load() || !validateBuffers(batch, undo, &validationError))) {
      // Editors can change through commands even while read-only. Restore the
      // disk half before reporting the conflict; this also runs off the UI
      // thread.
      QVector<SearchReplace::DiskChange> reverse;
      for (const auto &write : writes)
        reverse.append({write.path, write.after, write.before});
      report->success = false;
      auto rollback = std::make_shared<SearchReplace::DiskReport>();
      auto *rollbackTask = new AsyncTask([reverse, rollback](AsyncTask *) {
        *rollback = SearchReplace::applyDiskChanges(reverse);
      });
      connect(rollbackTask, &AsyncTask::finished, this,
              [this, paths, thaw, rollback, validationError, writes, undo]() {
                thaw();
                if (m_diskGuard)
                  m_diskGuard(paths, false);
                m_writeCancel.reset();
                setBusy(false);
                invalidate();
                m_timer->stop();
                if (!rollback->success) {
                  // The original writes may remain. Retain guarded undo instead
                  // of losing the only recovery snapshot.
                  m_undo = {};
                  m_undo.disks = writes;
                  if (undo)
                    for (auto &disk : m_undo.disks)
                      std::swap(disk.before, disk.after);
                }
                updateStatus(validationError +
                             (rollback->success
                                  ? tr(" Disk changes were restored.")
                                  : "\n" + rollback->error));
                updateUi();
                emit replacementFinished(false);
              });
      connect(rollbackTask, &AsyncTask::error, this,
              [this, paths, thaw](const QString &message) {
                thaw();
                if (m_diskGuard)
                  m_diskGuard(paths, false);
                m_writeCancel.reset();
                setBusy(false);
                updateStatus(message);
                emit replacementFinished(false);
              });
      AsyncThreadPool::instance().submit(rollbackTask);
      return;
    }
    m_applying = true;
    if (report->success) {
      for (auto &buffer : batch.buffers) {
        if (undo)
          buffer.document->undo();
        else {
          QTextCursor cursor(buffer.document);
          cursor.beginEditBlock();
          for (auto edit = buffer.edits.crbegin(); edit != buffer.edits.crend();
               ++edit) {
            cursor.setPosition(edit->start);
            cursor.setPosition(edit->start + edit->length,
                               QTextCursor::KeepAnchor);
            cursor.insertText(edit->replacement);
          }
          cursor.endEditBlock();
          buffer.generation = m_generations.value(buffer.document, -1);
          buffer.undoSteps = buffer.document->availableUndoSteps();
        }
      }
      // Mirror successful disk writes into files opened since the preview (or
      // opened between apply and undo), preserving their clean saved state.
      QSet<QTextDocument *> mirrored;
      for (const auto &doc : documents()) {
        if (!doc.editor || mirrored.contains(doc.editor->document()))
          continue;
        for (const auto &write : writes)
          if (doc.key == SearchReplace::fileKey(write.path)) {
            QString content =
                SearchReplace::normalizedText(QString::fromUtf8(write.after));
            if (content.startsWith(QChar(0xfeff)))
              content.remove(0, 1);
            QTextCursor cursor(doc.editor->document());
            cursor.beginEditBlock();
            cursor.select(QTextCursor::Document);
            cursor.insertText(content);
            cursor.endEditBlock();
            doc.editor->document()->setModified(false);
            mirrored.insert(doc.editor->document());
          }
      }
      m_undo = undo ? Batch{} : batch;
    } else if (!report->retained.isEmpty()) {
      m_undo = {};
      m_undo.disks = report->retained;
    }
    m_applying = false;
    thaw();
    if (m_diskGuard)
      m_diskGuard(paths, false);
    m_writeCancel.reset();
    setBusy(false);
    invalidate();
    m_timer->stop();
    if (report->success) {
      const QString message =
          undo ? tr("Replacement batch undone.")
               : tr("Replacement complete · %1 open editors updated, %2 closed "
                    "files saved. Undo replacement is available.")
                     .arg(batch.buffers.size())
                     .arg(batch.disks.size());
      updateStatus(message);
    } else
      updateStatus(report->error);
    updateUi();
    emit replacementFinished(report->success);
  };
  connect(task, &AsyncTask::finished, this, finish);
  connect(task, &AsyncTask::error, this,
          [this, paths, frozen](const QString &message) {
            for (const auto &entry : *frozen)
              if (entry.first)
                entry.first->setReadOnly(entry.second);
            if (m_diskGuard)
              m_diskGuard(paths, false);
            m_writeCancel.reset();
            setBusy(false);
            invalidate();
            m_timer->stop();
            updateStatus(message);
            emit replacementFinished(false);
          });
  AsyncThreadPool::instance().submit(task);
}
