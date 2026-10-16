#include "explorersearchwidget.h"

#include "../core/async/asyncworker.h"
#include "../theme/themedefinition.h"
#include "../ui/uistylehelper.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
enum {
  MatchRole = Qt::UserRole + 1,
  DetailRole,
  HighlightStartRole,
  HighlightLengthRole,
  PathRole
};

class SearchResultDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!index.data(DetailRole).isValid()) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QString title = opt.text;
    opt.text.clear();
    const QStyle *style =
        opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    painter->save();
    painter->setClipRect(option.rect);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const QColor foreground = opt.palette.color(
        selected ? QPalette::HighlightedText : QPalette::Text);
    const QRect rect =
        option.rect.adjusted(opt.icon.isNull() ? 6 : 28, 3, -6, -3);
    QFont font = option.font;
    painter->setFont(font);
    painter->setPen(foreground);
    const QFontMetrics metrics(font);
    painter->drawText(rect.left(), rect.top() + metrics.ascent(),
                      metrics.elidedText(title, Qt::ElideMiddle, rect.width()));
    if (font.pointSizeF() > 0) {
      font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1));
    }
    QTextLayout detail(index.data(DetailRole).toString(), font);
    QTextCharFormat normal;
    QColor muted = foreground;
    muted.setAlpha(selected ? 235 : 175);
    normal.setForeground(muted);
    QList<QTextLayout::FormatRange> formats{
        {0, int(detail.text().size()), normal}};
    const int length = index.data(HighlightLengthRole).toInt();
    if (length > 0) {
      QTextCharFormat highlight;
      highlight.setFontWeight(QFont::Bold);
      highlight.setForeground(foreground);
      QColor background = option.palette.color(QPalette::Highlight);
      background.setAlpha(110);
      highlight.setBackground(background);
      formats.append(
          {index.data(HighlightStartRole).toInt(), length, highlight});
    }
    detail.setFormats(formats);
    detail.beginLayout();
    QTextLine line = detail.createLine();
    if (line.isValid()) {
      line.setLineWidth(qMax(1, rect.width()));
    }
    detail.endLayout();
    detail.draw(painter,
                QPointF(rect.left(), rect.top() + metrics.height() + 1));
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    if (index.data(DetailRole).isValid()) {
      size.setHeight(2 * QFontMetrics(option.font).height() + 10);
    }
    return size;
  }
};
} // namespace

ExplorerSearchWidget::ExplorerSearchWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("explorerSearch"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  auto *modes = new QHBoxLayout;
  modes->setSpacing(3);
  auto makeButton = [this](const QString &text, const QString &name,
                           const QString &tip, bool checkable = true) {
    auto *button = new QToolButton(this);
    button->setObjectName(name);
    button->setText(text);
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    return button;
  };
  m_namesButton = makeButton(tr("Files"), "searchFilesMode",
                             tr("Search file and folder names"));
  m_contentsButton = makeButton(tr("Contents"), "searchContentsMode",
                                tr("Search text inside project files"));
  auto *modeGroup = new QButtonGroup(this);
  modeGroup->addButton(m_namesButton);
  modeGroup->addButton(m_contentsButton);
  modes->addWidget(m_namesButton, 1);
  modes->addWidget(m_contentsButton, 1);
  layout->addLayout(modes);

  m_query = new QLineEdit(this);
  m_query->setObjectName("treeFilterEdit");
  m_query->setClearButtonEnabled(true);
  m_query->setAccessibleName(tr("Search project"));
  m_query->setMinimumHeight(32);
  m_query->installEventFilter(this);
  layout->addWidget(m_query);

  auto *tools = new QHBoxLayout;
  tools->setSpacing(3);
  m_caseButton = makeButton("Aa", "explorerMatchCase", tr("Match case"));
  m_wordButton = makeButton("ab", "explorerWholeWord", tr("Match whole word"));
  m_regexButton =
      makeButton(".*", "explorerRegex", tr("Use regular expression"));
  m_filtersButton = makeButton(tr("Filters"), "explorerFiltersButton",
                               tr("Include and exclude files"));
  tools->addWidget(m_caseButton);
  tools->addWidget(m_wordButton);
  tools->addWidget(m_regexButton);
  tools->addStretch();
  tools->addWidget(m_filtersButton);
  layout->addLayout(tools);

  m_filters = new QWidget(this);
  auto *form = new QFormLayout(m_filters);
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);
  form->setRowWrapPolicy(QFormLayout::WrapAllRows);
  m_includes = new QLineEdit(m_filters);
  m_includes->setObjectName("explorerIncludes");
  m_includes->setPlaceholderText(tr("e.g. *.cpp, src/**"));
  m_includes->setAccessibleName(tr("Files to include"));
  m_excludes = new QLineEdit(m_filters);
  m_excludes->setObjectName("explorerExcludes");
  m_excludes->setAccessibleName(tr("Files and folders to exclude"));
  const QString globHelp = tr("Comma-separated patterns. * matches a name; "
                              "** matches across folders. Paths are relative "
                              "to the project root.");
  m_includes->setToolTip(globHelp);
  m_excludes->setToolTip(globHelp);
  form->addRow(tr("Include"), m_includes);
  form->addRow(tr("Exclude"), m_excludes);
  m_hidden = new QCheckBox(tr("Include hidden files"), m_filters);
  m_hidden->setObjectName("explorerHiddenFiles");
  form->addRow(m_hidden);
  m_filters->hide();
  layout->addWidget(m_filters);

  m_scopeButton =
      makeButton({}, "explorerSearchScope",
                 tr("Clear folder scope and search the project"), false);
  m_scopeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_scopeButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_scopeButton->hide();
  layout->addWidget(m_scopeButton);

  m_feedback = new QWidget(this);
  auto *feedback = new QHBoxLayout(m_feedback);
  feedback->setContentsMargins(2, 0, 0, 0);
  m_status = new QLabel(m_feedback);
  m_status->setObjectName("explorerSearchStatus");
  m_status->setWordWrap(true);
  m_status->setTextFormat(Qt::PlainText);
  m_status->setAccessibleName(tr("Search status"));
  feedback->addWidget(m_status, 1);
  m_stopButton =
      makeButton(tr("Stop"), "explorerStopSearch", tr("Cancel search"), false);
  feedback->addWidget(m_stopButton);
  m_feedback->hide();
  layout->addWidget(m_feedback);

  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("explorerSearchResults");
  m_tree->setHeaderHidden(true);
  m_tree->setColumnCount(1);
  m_tree->setFrameShape(QFrame::NoFrame);
  m_tree->setIndentation(12);
  m_tree->setTextElideMode(Qt::ElideMiddle);
  m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);
  m_tree->setItemDelegate(new SearchResultDelegate(m_tree));
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tree->setAccessibleName(tr("Project search results"));
  m_tree->installEventFilter(this);
  m_tree->hide();
  layout->addWidget(m_tree, 1);

  m_timer = new QTimer(this);
  m_timer->setSingleShot(true);
  m_timer->setInterval(180);
  connect(m_timer, &QTimer::timeout, this, &ExplorerSearchWidget::startSearch);
  connect(m_query, &QLineEdit::textChanged, this,
          &ExplorerSearchWidget::changed);
  connect(m_includes, &QLineEdit::textChanged, this,
          &ExplorerSearchWidget::changed);
  connect(m_excludes, &QLineEdit::textChanged, this,
          &ExplorerSearchWidget::changed);
  connect(m_hidden, &QCheckBox::toggled, this, &ExplorerSearchWidget::changed);
  connect(modeGroup, &QButtonGroup::buttonClicked, this,
          [this]() { changed(); });
  for (auto *button : {m_caseButton, m_wordButton, m_regexButton}) {
    connect(button, &QToolButton::toggled, this,
            &ExplorerSearchWidget::changed);
  }
  connect(m_filtersButton, &QToolButton::toggled, m_filters,
          &QWidget::setVisible);
  connect(m_scopeButton, &QToolButton::clicked, this, [this]() {
    m_options.scopePath.clear();
    changed();
  });
  connect(m_stopButton, &QToolButton::clicked, this, [this]() {
    cancelSearch();
    m_timer->stop();
    m_dirty = false;
    m_status->setText(
        tr("Search cancelled. Edit the query or refresh to retry."));
    m_stopButton->hide();
  });
  connect(m_tree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item, int) { activateItem(item); });
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &position) {
            auto *item = position.x() < 0 ? m_tree->currentItem()
                                          : m_tree->itemAt(position);
            const auto *match = matchForItem(item);
            const QString path = match  ? match->filePath
                                 : item ? item->data(0, PathRole).toString()
                                        : QString();
            if (!path.isEmpty()) {
              emit entryContextMenuRequested(
                  path,
                  m_tree->viewport()->mapToGlobal(
                      position.x() < 0 ? m_tree->visualItemRect(item).center()
                                       : position));
            }
          });
  setOptions(m_options);
}

ExplorerSearchWidget::~ExplorerSearchWidget() { cancelSearch(); }

ProjectSearch::Options ExplorerSearchWidget::options() const {
  return m_options;
}

void ExplorerSearchWidget::setOptions(const ProjectSearch::Options &options) {
  if (options == m_options &&
      (m_namesButton->isChecked() || m_contentsButton->isChecked())) {
    return;
  }
  m_settingOptions = true;
  m_options = options;
  m_query->setText(options.query);
  m_includes->setText(options.includes);
  m_excludes->setText(options.excludes);
  m_namesButton->setChecked(options.mode == ProjectSearch::Mode::FileNames);
  m_contentsButton->setChecked(options.mode == ProjectSearch::Mode::Contents);
  m_caseButton->setChecked(options.caseSensitive);
  m_wordButton->setChecked(options.wholeWord);
  m_regexButton->setChecked(options.regularExpression);
  m_hidden->setChecked(options.includeHidden);
  m_settingOptions = false;
  updateControls();
  scheduleSearch();
}

void ExplorerSearchWidget::setRootPath(const QString &path) {
  if (m_options.rootPath == path) {
    return;
  }
  m_options.rootPath = path;
  m_options.scopePath.clear();
  updateControls();
  scheduleSearch();
}

void ExplorerSearchWidget::setSearchText(const QString &text) {
  if (m_query->text() != text) {
    m_query->setText(text);
  }
}

void ExplorerSearchWidget::searchInFolder(const QString &path) {
  m_options.scopePath = path;
  m_contentsButton->setChecked(true);
  changed();
  m_query->setFocus();
  m_query->selectAll();
}

void ExplorerSearchWidget::focusSearch(ProjectSearch::Mode mode) {
  const bool modeChanged = m_options.mode != mode;
  (mode == ProjectSearch::Mode::Contents ? m_contentsButton : m_namesButton)
      ->setChecked(true);
  if (modeChanged)
    changed();
  m_query->setFocus();
  m_query->selectAll();
}

void ExplorerSearchWidget::refresh() { scheduleSearch(); }

void ExplorerSearchWidget::changed() {
  if (m_settingOptions) {
    return;
  }
  m_options.query = m_query->text();
  m_options.includes = m_includes->text();
  m_options.excludes = m_excludes->text();
  m_options.mode = m_contentsButton->isChecked()
                       ? ProjectSearch::Mode::Contents
                       : ProjectSearch::Mode::FileNames;
  m_options.caseSensitive = m_caseButton->isChecked();
  m_options.wholeWord = m_wordButton->isChecked();
  m_options.regularExpression = m_regexButton->isChecked();
  m_options.includeHidden = m_hidden->isChecked();
  updateControls();
  scheduleSearch();
  emit optionsChanged();
}

void ExplorerSearchWidget::updateControls() {
  const bool contents = m_options.mode == ProjectSearch::Mode::Contents;
  m_query->setPlaceholderText(contents ? tr("Search file contents…")
                                       : tr("Find files by name or path…"));
  m_query->setToolTip(contents ? tr("Search saved UTF-8 text in project files")
                               : tr("Fuzzy names, paths, or wildcards: *.cpp"));
  m_wordButton->setEnabled(contents);
  m_regexButton->setEnabled(contents);
  m_scopeButton->setVisible(!m_options.scopePath.isEmpty());
  const QString relative =
      QDir(m_options.rootPath).relativeFilePath(m_options.scopePath);
  m_scopeButton->setText(tr("In: %1  ×").arg(relative));
  m_scopeButton->setToolTip(
      tr("%1\nClick to search the whole project").arg(m_options.scopePath));
}

void ExplorerSearchWidget::cancelSearch() {
  ++m_request;
  if (m_cancelled) {
    m_cancelled->store(true);
    m_cancelled.reset();
  }
}

void ExplorerSearchWidget::scheduleSearch() {
  cancelSearch();
  m_timer->stop();
  m_dirty = true;
  m_results = {};
  m_tree->clear();
  const bool active = m_options.mode == ProjectSearch::Mode::Contents
                          ? !m_options.query.isEmpty()
                          : !m_options.query.trimmed().isEmpty();
  m_feedback->setVisible(active);
  m_tree->setVisible(active);
  m_stopButton->setVisible(active);
  emit searchActiveChanged(active);
  if (active) {
    m_status->setText(tr("Searching…"));
    if (isVisible()) {
      m_timer->start();
    }
  }
}

void ExplorerSearchWidget::startSearch() {
  if (!isVisible() || m_options.query.isEmpty() ||
      (m_options.mode == ProjectSearch::Mode::FileNames &&
       m_options.query.trimmed().isEmpty())) {
    return;
  }
  m_dirty = false;
  const int request = m_request;
  const ProjectSearch::Options options = m_options;
  auto token = std::make_shared<std::atomic_bool>(false);
  m_cancelled = token;
  auto results = std::make_shared<ProjectSearch::Results>();
  auto *task = new AsyncTask([options, results, token](AsyncTask *worker) {
    *results = ProjectSearch::search(
        options, [token]() { return token->load(); },
        [worker](int scanned) {
          worker->reportProgress(
              0, tr("Searching… %n files scanned", nullptr, scanned));
        });
  });
  connect(task, &AsyncWorker::progress, this,
          [this, request](int, const QString &message) {
            if (request == m_request) {
              m_status->setText(message);
            }
          });
  connect(task, &AsyncWorker::finished, this, [this, request, results]() {
    if (request == m_request) {
      m_cancelled.reset();
      displayResults(*results);
    }
  });
  connect(task, &AsyncWorker::error, this,
          [this, request](const QString &error) {
            if (request == m_request) {
              m_stopButton->hide();
              m_status->setText(error);
            }
          });
  AsyncThreadPool::instance().submit(task);
}

void ExplorerSearchWidget::displayResults(
    const ProjectSearch::Results &results) {
  m_results = results;
  m_tree->clear();
  m_stopButton->hide();
  if (!results.error.isEmpty()) {
    m_status->setText(results.error);
    return;
  }
  QHash<QString, QTreeWidgetItem *> files;
  const bool contents = m_options.mode == ProjectSearch::Mode::Contents;
  for (int i = 0; i < results.matches.size(); ++i) {
    const auto &match = results.matches.at(i);
    QTreeWidgetItem *parent = nullptr;
    if (contents) {
      parent = files.value(match.filePath);
      if (!parent) {
        parent = new QTreeWidgetItem(m_tree, {match.relativePath});
        parent->setData(0, PathRole, match.filePath);
        parent->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        parent->setToolTip(0, match.filePath);
        parent->setExpanded(true);
        files.insert(match.filePath, parent);
      }
    }
    auto *item =
        parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
    item->setData(0, MatchRole, i);
    item->setData(0, PathRole, match.filePath);
    if (!contents) {
      item->setIcon(0, style()->standardIcon(match.isDirectory
                                                 ? QStyle::SP_DirIcon
                                                 : QStyle::SP_FileIcon));
    }
    item->setText(
        0, contents
               ? tr("Line %1 · Col %2").arg(match.line).arg(match.column)
               : QFileInfo(match.filePath).fileName() +
                     (match.isDirectory ? QStringLiteral("/") : QString()));
    item->setData(0, DetailRole, contents ? match.preview : match.relativePath);
    item->setData(0, HighlightStartRole, contents ? match.previewStart : 0);
    item->setData(0, HighlightLengthRole, contents ? match.previewLength : 0);
    item->setToolTip(0, contents ? tr("%1:%2:%3\n%4")
                                       .arg(match.filePath)
                                       .arg(match.line)
                                       .arg(match.column)
                                       .arg(match.preview)
                                 : match.filePath);
  }
  if (m_tree->topLevelItemCount() > 0) {
    auto *first = m_tree->topLevelItem(0);
    m_tree->setCurrentItem(contents ? first->child(0) : first);
  }
  const int count = results.matches.size();
  const QString matchCount =
      count == 1 ? tr("1 match") : tr("%n matches", nullptr, count);
  const QString fileCount =
      files.size() == 1 ? tr("1 file") : tr("%n files", nullptr, files.size());
  QString status = contents     ? tr("%1 in %2").arg(matchCount, fileCount)
                   : count == 1 ? tr("1 result")
                                : tr("%n results", nullptr, count);
  if (results.truncated) {
    status += tr(" · limit reached; narrow your search");
  }
  if (results.matches.isEmpty()) {
    status = tr("No results. Try another query or adjust filters.");
  }
  if (results.skippedFiles) {
    status += tr(" · %n files skipped", nullptr, results.skippedFiles);
  }
  m_status->setText(status);
  m_status->setToolTip(
      tr("%1 files scanned. Content search uses saved files; "
         "binary, unreadable, linked and files over 2 MiB are skipped.")
          .arg(results.scannedFiles));
}

const ProjectSearch::Match *
ExplorerSearchWidget::matchForItem(QTreeWidgetItem *item) const {
  if (!item || !item->data(0, MatchRole).isValid()) {
    return nullptr;
  }
  const int index = item->data(0, MatchRole).toInt();
  return index >= 0 && index < m_results.matches.size()
             ? &m_results.matches.at(index)
             : nullptr;
}

void ExplorerSearchWidget::activateItem(QTreeWidgetItem *item) {
  const auto *found = matchForItem(item);
  if (!found) {
    return;
  }

  const ProjectSearch::Match match = *found;
  if (match.isDirectory) {
    emit revealRequested(match.filePath);
  } else {
    emit navigateRequested(match.filePath, match.line, match.column,
                           match.length);
  }
}

bool ExplorerSearchWidget::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Escape) {
      if (!m_query->text().isEmpty()) {
        m_query->clear();
      } else if (!m_options.scopePath.isEmpty()) {
        m_options.scopePath.clear();
        changed();
      }
      m_query->setFocus();
      return true;
    }
    if (object == m_query &&
        (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up)) {
      m_tree->setFocus();
      return true;
    }
    if (object == m_query &&
        (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
      if (m_timer->isActive()) {
        m_timer->stop();
        startSearch();
      } else {
        activateItem(m_tree->currentItem());
      }
      return true;
    }
  }
  return QWidget::eventFilter(object, event);
}

void ExplorerSearchWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (m_dirty) {
    scheduleSearch();
  }
}

void ExplorerSearchWidget::hideEvent(QHideEvent *event) {
  m_timer->stop();
  cancelSearch();
  m_dirty = true;
  QWidget::hideEvent(event);
}

void ExplorerSearchWidget::applyTheme(const ThemeDefinition &theme) {
  const auto &c = theme.colors;
  QPalette colors = palette();
  colors.setColor(QPalette::Window, c.surfaceRaised);
  colors.setColor(QPalette::WindowText, c.textPrimary);
  colors.setColor(QPalette::Base, c.surfaceBase);
  colors.setColor(QPalette::Text, c.textPrimary);
  colors.setColor(QPalette::Highlight, c.treeSelectedBg);
  colors.setColor(QPalette::HighlightedText, c.textPrimary);
  setPalette(colors);
  setAutoFillBackground(true);
  setStyleSheet(
      QString("QToolButton { color: %1; background: transparent; border: 1px "
              "solid transparent; "
              "border-radius: 5px; padding: 4px 6px; min-height: 20px; } "
              "QToolButton:hover { background: %2; } "
              "QToolButton:checked { background: %3; color: %4; border-color: "
              "%4; } "
              "QToolButton:disabled { color: %5; } "
              "QLabel, QCheckBox { color: %1; background: transparent; } "
              "QLabel#explorerSearchStatus { color: %5; font-size: 11px; } ")
          .arg(c.textSecondary.name(), c.btnGhostHover.name(),
               c.surfaceOverlay.name(), c.accentPrimary.name(),
               c.textMuted.name()));
  for (auto *edit : {m_query, m_includes, m_excludes}) {
    edit->setMinimumHeight(32);
    edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }
  m_tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
}
