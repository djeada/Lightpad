#include "debugvalueinspector.h"

#include "../uistylehelper.h"
#include "debugvalueformatter.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
constexpr int Reference = Qt::UserRole;
constexpr int Expression = Qt::UserRole + 1;
constexpr int RawValue = Qt::UserRole + 2;
constexpr int IndexedCount = Qt::UserRole + 3;
constexpr int NamedCount = Qt::UserRole + 4;
constexpr int Loaded = Qt::UserRole + 6;
constexpr int PageAction = Qt::UserRole + 7;
constexpr int PageStart = Qt::UserRole + 8;
constexpr int MoreReference = Qt::UserRole + 9;
constexpr int ReadError = Qt::UserRole + 10;
constexpr int PointerRow = Qt::UserRole + 12;
constexpr int StringBytes = Qt::UserRole + 13;
constexpr int Variable = Qt::UserRole + 14;
constexpr int NamedChildren = Qt::UserRole + 15;
constexpr int PrefixPaging = Qt::UserRole + 16;
constexpr int NextOffset = Qt::UserRole + 17;
constexpr int FixedArray = Qt::UserRole + 18;
constexpr int RepeatedReference = Qt::UserRole + 19;
constexpr int MoreStart = Qt::UserRole + 20;
constexpr int PageSize = 100;
constexpr int MaxTreeItems = 10000;
constexpr int TableRows = 50;
constexpr int TableColumns = 50;

void removePage(QTreeWidgetItem *parent) {
  const int last = parent->childCount() - 1;
  if (last >= 0 && parent->child(last)->data(0, PageAction).toBool())
    delete parent->takeChild(last);
}
} // namespace

DebugValueInspector::DebugValueInspector(QWidget *parent) : QWidget(parent) {
  setObjectName("debugValueInspector");
  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedFont.setPointSize(qMax(10, font().pointSize()));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  auto *heading = new QHBoxLayout;
  m_title = new QLabel(tr("Value inspector"), this);
  m_title->setObjectName("debugValueTitle");
  m_title->setTextFormat(Qt::PlainText);
  m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_title->setWordWrap(true);
  heading->addWidget(m_title, 1);
  m_liveUpdates = new QCheckBox(tr("Live"), this);
  m_liveUpdates->setObjectName("debugValueLiveUpdates");
  m_liveUpdates->setToolTip(tr("Refresh this expression in its original stack "
                               "frame after each step or breakpoint"));
  m_liveUpdates->setEnabled(false);
  heading->addWidget(m_liveUpdates);
  connect(m_liveUpdates, &QCheckBox::toggled, this, [this](bool enabled) {
    if (enabled && !m_live)
      refreshValue();
  });
  m_refresh = new QPushButton(tr("Refresh"), this);
  m_refresh->setObjectName("debugValueRefresh");
  m_refresh->setEnabled(false);
  connect(m_refresh, &QPushButton::clicked, this,
          [this]() { refreshValue(true); });
  heading->addWidget(m_refresh);
  m_watch = new QPushButton(tr("Add to Watches"), this);
  m_watch->setEnabled(false);
  connect(m_watch, &QPushButton::clicked, this,
          [this]() { emit addWatchRequested(m_value.evaluateName); });
  heading->addWidget(m_watch);
  auto *copy = new QPushButton(tr("Copy"), this);
  copy->setToolTip(
      tr("Copy selected cells as tab-separated text, or the formatted value"));
  connect(copy, &QPushButton::clicked, this,
          &DebugValueInspector::copySelection);
  heading->addWidget(copy);
  layout->addLayout(heading);

  auto *formatRow = new QHBoxLayout;
  formatRow->addWidget(new QLabel(tr("Display"), this));
  m_displayFormat = new QComboBox(this);
  m_displayFormat->setObjectName("debugValueDisplayFormat");
  m_displayFormat->addItems({tr("Automatic"), tr("Decimal"), tr("Hexadecimal"),
                             tr("Binary"), tr("Character")});
  m_displayFormat->setToolTip(tr("Change how integer and character values are "
                                 "displayed without modifying the program"));
  connect(m_displayFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int format) {
            if (m_stringView && format > 0)
              m_tabs->setCurrentIndex(1);
            refreshDisplay();
          });
  formatRow->addWidget(m_displayFormat);
  formatRow->addStretch();
  layout->addLayout(formatRow);

  m_arrayControls = new QWidget(this);
  m_arrayControls->setObjectName("debugArrayControls");
  auto *arrayLayout = new QGridLayout(m_arrayControls);
  arrayLayout->setContentsMargins(0, 0, 0, 0);
  arrayLayout->addWidget(new QLabel(tr("Array / buffer"), this), 0, 0);
  m_arrayExpression = new QLineEdit(this);
  m_arrayExpression->setObjectName("debugArrayExpression");
  m_arrayExpression->setToolTip(
      tr("Pointer or array expression in the selected stack frame"));
  arrayLayout->addWidget(m_arrayExpression, 0, 1, 1, 6);
  arrayLayout->addWidget(new QLabel(tr("Elements"), this), 1, 0);
  m_arrayCount = new QSpinBox(this);
  m_arrayCount->setObjectName("debugArrayCount");
  m_arrayCount->setRange(0, 10000);
  m_arrayCount->setSpecialValueText(tr("Set length"));
  m_arrayCount->setToolTip(tr("Number of elements to inspect. A pointer does "
                              "not record its allocation length."));
  arrayLayout->addWidget(m_arrayCount, 1, 1);
  arrayLayout->addWidget(new QLabel(tr("Element type"), this), 1, 2);
  m_elementType = new QComboBox(this);
  m_elementType->setObjectName("debugArrayElementType");
  m_elementType->setEditable(true);
  m_elementType->addItem(tr("Declared type"), QString());
  for (const QString &type :
       {"char *", "const char *", "char", "signed char", "unsigned char",
        "short", "unsigned short", "int", "unsigned int", "long",
        "unsigned long", "long long", "unsigned long long", "float", "double"})
    m_elementType->addItem(type, type);
  m_elementType->setToolTip(tr("Use the declared element type, or enter a "
                               "C/C++ type such as int or struct Node"));
  arrayLayout->addWidget(m_elementType, 1, 3);
  arrayLayout->addWidget(new QLabel(tr("Columns"), this), 1, 4);
  m_arrayColumns = new QSpinBox(this);
  m_arrayColumns->setObjectName("debugArrayColumns");
  m_arrayColumns->setRange(1, TableColumns);
  m_arrayColumns->setToolTip(tr("Row width for contiguous data, or the number "
                                "of elements in each pointed-to row"));
  arrayLayout->addWidget(m_arrayColumns, 1, 5);
  auto *viewArray = new QPushButton(tr("View array"), this);
  viewArray->setObjectName("debugArrayApply");
  connect(viewArray, &QPushButton::clicked, this,
          &DebugValueInspector::applyArrayView);
  connect(m_arrayExpression, &QLineEdit::returnPressed, this,
          &DebugValueInspector::applyArrayView);
  arrayLayout->addWidget(viewArray, 1, 6);
  arrayLayout->addWidget(new QLabel(tr("Length expression"), this), 2, 0);
  m_lengthExpression = new QLineEdit(this);
  m_lengthExpression->setObjectName("debugArrayLengthExpression");
  m_lengthExpression->setPlaceholderText(tr("Optional, e.g. count"));
  m_lengthExpression->setToolTip(
      tr("Evaluate the outer element count in the "
         "selected stack frame; edit Elements to override"));
  arrayLayout->addWidget(m_lengthExpression, 2, 1, 1, 3);
  connect(m_lengthExpression, &QLineEdit::returnPressed, this,
          &DebugValueInspector::applyArrayView);
  connect(m_lengthExpression, &QLineEdit::textChanged, this, [this]() {
    m_lengthRequest = 0;
    m_lengthResolved = false;
  });
  connect(m_arrayCount, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int count) {
            m_lengthExpression->clear();
            m_lengthRequest = 0;
            m_lengthResolved = count > 0;
            m_arrayCount->setSpecialValueText(tr("Set length"));
          });
  arrayLayout->addWidget(new QLabel(tr("Layout"), this), 2, 4);
  m_arrayLayout = new QComboBox(this);
  m_arrayLayout->setObjectName("debugArrayLayout");
  m_arrayLayout->addItems({tr("Automatic"), tr("Contiguous rows"),
                           tr("Pointer rows"), tr("String characters")});
  m_arrayLayout->setToolTip(tr("Pointer rows follow each row pointer. String "
                               "characters show a ragged character grid."));
  connect(m_arrayLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          [this](int layout) { m_arrayColumns->setEnabled(layout != 3); });
  arrayLayout->addWidget(m_arrayLayout, 2, 5, 1, 2);
  arrayLayout->setColumnStretch(3, 1);
  m_arrayControls->hide();
  layout->addWidget(m_arrayControls);

  m_filter = new QLineEdit(this);
  m_filter->setObjectName("debugValueFilter");
  m_filter->setPlaceholderText(tr("Filter loaded names, values and types…"));
  m_filter->setClearButtonEnabled(true);
  connect(m_filter, &QLineEdit::textChanged, this,
          &DebugValueInspector::filterTree);
  layout->addWidget(m_filter);

  m_tabs = new QTabWidget(this);
  m_tabs->setObjectName("debugValueTabs");
  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("debugValueTree");
  m_tree->setHeaderLabels({tr("Name / index"), tr("Value"), tr("Type")});
  m_tree->setUniformRowHeights(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_tree->setFont(fixedFont);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_tree->setColumnWidth(0, 220);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
          &DebugValueInspector::showTreeMenu);
  connect(m_tree, &QTreeWidget::itemExpanded, this,
          [this](QTreeWidgetItem *item) {
            if (!item->data(0, Loaded).toBool())
              loadChildren(item);
          });
  connect(m_tree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item, int) {
            if (item->data(0, PageAction).toBool() && item->parent()) {
              loadChildren(item->parent(), item->data(0, PageStart).toInt());
            }
          });
  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *item) {
            if (item && !item->data(0, PageAction).toBool()) {
              updateText();
            }
          });
  m_tabs->addTab(m_tree, tr("Tree"));

  m_table = new QTableWidget(this);
  m_table->setObjectName("debugValueTable");
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_table->setAlternatingRowColors(true);
  m_table->setWordWrap(false);
  m_table->setFont(fixedFont);
  m_table->horizontalHeader()->setDefaultSectionSize(130);
  m_table->horizontalHeader()->setMinimumSectionSize(60);
  m_table->verticalHeader()->setDefaultSectionSize(28);
  m_tabs->addTab(m_table, tr("Table"));
  connect(m_table, &QTableWidget::cellActivated, this,
          [this](int rowIndex, int columnIndex) {
            const auto *cell = m_table->item(rowIndex, columnIndex);
            if (!m_root || !cell || !cell->data(Qt::UserRole + 1).isValid())
              return;
            const int rootIndex = cell->data(Qt::UserRole + 1).toInt();
            if (rootIndex < 0 || rootIndex >= m_root->childCount())
              return;
            auto *row = m_root->child(rootIndex);
            auto *target =
                cell->data(Qt::UserRole + 2).isValid()
                    ? row->child(cell->data(Qt::UserRole + 2).toInt())
                    : row;
            if (!target)
              return;
            m_tabs->setCurrentIndex(0);
            row->setExpanded(true);
            target->setExpanded(true);
            m_tree->setCurrentItem(target);
            m_tree->scrollToItem(target);
          });

  m_text = new QPlainTextEdit(this);
  m_text->setObjectName("debugValueText");
  m_text->setReadOnly(true);
  m_text->setFont(fixedFont);
  m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  m_text->setPlaceholderText(tr("Evaluate an expression, or double-click a "
                                "variable or watch to inspect it."));
  m_tabs->addTab(m_text, tr("Text"));
  connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
    m_filter->setVisible(index != 2);
    if (index == 1)
      updateTable();
  });
  connect(m_tabs, &QTabWidget::tabBarClicked, this,
          [this]() { m_autoTable = false; });
  auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_table);
  copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(copyShortcut, &QShortcut::activated, this,
          &DebugValueInspector::copySelection);
  layout->addWidget(m_tabs, 1);
  m_status = new QLabel(
      tr("Inspect nested values without scrolling through a single long line."),
      this);
  m_status->setObjectName("debugValueStatus");
  m_status->setTextFormat(Qt::PlainText);
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
}

void DebugValueInspector::setDapClient(DapClient *client) {
  if (m_client)
    disconnect(m_client, nullptr, this, nullptr);
  clear();
  m_client = client;
  if (!client)
    return;
  connect(client, &DapClient::inspectionVariablesReceived, this,
          &DebugValueInspector::receiveChildren);
  connect(client, &DapClient::evaluateValueReceived, this,
          &DebugValueInspector::receiveElement);
  connect(client, &DapClient::evaluateResponseError, this,
          [this](int seq, const QString &, const QString &message) {
            if (seq == m_refreshRequest) {
              m_refreshRequest = 0;
              m_status->setText(tr("Snapshot — could not refresh %1: %2")
                                    .arg(m_expression, message));
              return;
            }
            if (seq == m_lengthRequest) {
              m_lengthRequest = 0;
              m_lengthResolved = false;
              m_status->setText(tr("Could not read the outer length: %1. "
                                   "Set Elements or a valid length expression.")
                                    .arg(message));
              return;
            }
            auto *item = m_elementRequests.take(seq);
            if (!item || !m_live)
              return;
            ++m_readErrors;
            item->setData(0, ReadError, true);
            item->setData(0, RawValue, tr("<unreadable: %1>").arg(message));
            finishElementPage();
          });
  connect(client, &DapClient::inspectionVariablesFailed, this,
          [this](int seq, const QString &message) {
            if (!m_requests.contains(seq))
              return;
            const Request request = m_requests.take(seq);
            m_status->setText(
                request.start == 0
                    ? tr("Could not load children: %1. Collapse "
                         "and expand to retry.")
                          .arg(message)
                    : tr("Could not load the next page: %1.").arg(message));
          });
  connect(client, &DapClient::stateChanged, this,
          [this](DapClient::State state) {
            if (state != DapClient::State::Stopped)
              invalidate();
          });
  connect(client, &QObject::destroyed, this, &DebugValueInspector::invalidate);
  connect(client, &DapClient::continued, this,
          &DebugValueInspector::invalidate);
  connect(client, &DapClient::restarting, this,
          &DebugValueInspector::invalidate);
  connect(client, &DapClient::inspectionInvalidated, this,
          &DebugValueInspector::invalidate);
  connect(client, &DapClient::stopped, this, [this]() { refreshValue(); });
  connect(client, &DapClient::interactiveEvaluationFinished, this,
          [this]() { refreshValue(); });
  connect(client, &DapClient::stackTraceReceived, this,
          [this](int threadId, const QList<DapStackFrame> &frames, int) {
            receiveFrames(threadId, frames);
          });
}

bool DebugValueInspector::supportsNativeBuffers() const {
  return m_client && DebugValueFormatter::supportsNativeBuffers(
                         m_client->adapterId(), m_client->adapterType());
}

bool DebugValueInspector::usesDelve() const {
  return m_client && (m_client->adapterId() == "go-delve" ||
                      m_client->adapterType() == "go");
}

bool DebugValueInspector::usesJsDebug() const {
  return m_client && (m_client->adapterId() == "node-debug" ||
                      m_client->adapterType() == "node" ||
                      m_client->adapterType() == "pwa-node");
}

bool DebugValueInspector::pagesByIndex(const QString &filter) const {
  // Delve counts map entries, and js-debug counts array slots including holes.
  return filter == QLatin1String("indexed") && (usesDelve() || usesJsDebug());
}

bool DebugValueInspector::isPythonSequence(const QTreeWidgetItem *item) const {
  if (!m_client || (m_client->adapterId() != "python-debugpy" &&
                    m_client->adapterType() != "debugpy"))
    return false;
  const QString type = item->text(2);
  return type == "list" || type == "tuple" || type == "ndarray";
}

bool DebugValueInspector::hasChildren(const QTreeWidgetItem *item) {
  return item->childCount() > 0 ||
         item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator;
}

bool DebugValueInspector::hasKnownEmptyChildren(
    const DapVariable &value) const {
  if (value.indexedVariables > 0 || value.namedVariables > 0)
    return false;
  if (value.hasIndexedVariables && value.hasNamedVariables)
    return true;
  // GDB reports a count for exactly one kind of children.
  return m_client &&
         (m_client->adapterId().contains("gdb", Qt::CaseInsensitive) ||
          m_client->adapterType().compare("gdb", Qt::CaseInsensitive) == 0) &&
         (value.hasIndexedVariables || value.hasNamedVariables);
}

qlonglong DebugValueInspector::indexForName(QString name) {
  name = name.trimmed();
  if (name.startsWith('[') && name.endsWith(']'))
    name = name.mid(1, name.size() - 2);
  bool ok = false;
  const qlonglong index = name.toLongLong(&ok);
  return ok && index >= 0 ? index : -1;
}

QTreeWidgetItem *DebugValueInspector::appendValue(QTreeWidgetItem *parent,
                                                  const DapVariable &value) {
  auto *item =
      parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
  ++m_itemCount;
  updateItem(item, value);
  return item;
}

QString DebugValueInspector::displayValue(const QString &value,
                                          const QString &type) const {
  return DebugValueFormatter::scalar(
      DebugValueFormatter::tableCell(value), type,
      static_cast<DebugValueFormatter::DisplayFormat>(
          m_displayFormat->currentIndex()));
}

QString
DebugValueInspector::itemDisplayValue(const QTreeWidgetItem *item) const {
  QString text =
      displayValue(item->data(0, RawValue).toString(), item->text(2));
  if (text.isEmpty() && item->data(0, Reference).toInt() > 0) {
    const int count = item->data(0, IndexedCount).toInt();
    text = count > 0 ? tr("[%1 elements]").arg(count) : QStringLiteral("{…}");
  }
  if (item->data(0, RepeatedReference).toBool())
    return tr("%1 (already in this branch)").arg(text);
  return text;
}

void DebugValueInspector::updateItem(QTreeWidgetItem *item,
                                     const DapVariable &value) {
  item->setText(0, value.name);
  item->setText(2, value.type);
  item->setData(0, Reference, value.variablesReference);
  QString expression = value.evaluateName;
  if (expression.isEmpty() && item->parent() && supportsNativeBuffers()) {
    expression = DebugValueFormatter::childExpression(
        item->parent()->data(0, Expression).toString(), item->parent()->text(2),
        value.name);
  }
  item->setData(0, Expression, expression);
  item->setData(0, RawValue, value.value);
  item->setData(0, IndexedCount, value.indexedVariables);
  item->setData(0, NamedCount, value.namedVariables);
  item->setData(0, Variable, QVariant::fromValue(value));
  const int length = supportsNativeBuffers() && value.variablesReference == 0
                         ? DebugValueFormatter::arrayLength(value.type)
                         : 0;
  const bool fixedArray = length > 0 && !expression.isEmpty();
  item->setData(0, FixedArray, fixedArray);
  QByteArray bytes;
  const bool cachedCharacters =
      length > 0 && expression.isEmpty() &&
      DebugValueFormatter::isCharacterArray(value.type) &&
      DebugValueFormatter::stringBytes(value.value, &bytes) && !bytes.isEmpty();
  if (fixedArray) {
    item->setData(0, IndexedCount, qMin(length, MaxTreeItems));
  } else if (cachedCharacters) {
    bytes.truncate(length);
    item->setData(0, IndexedCount, bytes.size());
    item->setData(0, StringBytes, bytes);
  }
  bool repeated = false;
  if (value.variablesReference > 0 && !item->data(0, NamedChildren).toBool()) {
    for (auto *ancestor = item->parent(); ancestor;
         ancestor = ancestor->parent()) {
      if (ancestor->data(0, NamedChildren).toBool())
        continue;
      const auto previous = ancestor->data(0, Variable).value<DapVariable>();
      if (previous.variablesReference == value.variablesReference ||
          (supportsNativeBuffers() && !value.memoryReference.isEmpty() &&
           previous.memoryReference == value.memoryReference &&
           previous.type == value.type)) {
        repeated = true;
        break;
      }
    }
  }
  item->setData(0, RepeatedReference, repeated);
  item->setText(1, DebugValueFormatter::summary(itemDisplayValue(item)));
  for (int c = 0; c < 3; ++c)
    item->setToolTip(c, DebugValueFormatter::format(value.value));
  if ((value.variablesReference > 0 && !hasKnownEmptyChildren(value) &&
       !repeated) ||
      fixedArray || cachedCharacters)
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  else
    item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
}

void DebugValueInspector::clearValue() {
  m_requests.clear();
  m_elementRequests.clear();
  m_arrayPages.clear();
  m_lengthRequest = 0;
  m_lengthResolved = false;
  m_root = nullptr;
  m_value = {};
  m_live = false;
  m_itemCount = 0;
  m_bufferView = false;
  m_bufferColumns = 1;
  m_bufferLayout = 0;
  m_readErrors = 0;
  m_stringView = false;
  m_arrayControls->hide();
  m_tree->clear();
  m_table->clear();
  m_table->setRowCount(0);
  m_table->setColumnCount(0);
  m_text->clear();
  m_watch->setEnabled(false);
  m_title->setText(tr("Value inspector"));
  m_status->setText(
      tr("Evaluate an expression, or double-click a variable or watch."));
}

void DebugValueInspector::clear() {
  clearValue();
  m_expression.clear();
  m_valueName.clear();
  m_frameName.clear();
  m_frameSource.clear();
  m_threadId = 0;
  m_frameFromBottom = -1;
  m_refreshRequest = 0;
  m_waitingForFrame = false;
  m_explicitRefresh = false;
  m_savedNodes.clear();
  m_restoreView = false;
  m_liveUpdates->setEnabled(false);
  m_liveUpdates->setChecked(false);
  m_refresh->setEnabled(false);
}

void DebugValueInspector::setValue(const DapVariable &value, int frameId,
                                   const QString &lengthExpression,
                                   int threadId) {
  clear();
  m_frameId = frameId;
  m_expression = value.evaluateName;
  m_valueName = value.name;
  m_threadId = threadId > 0 ? threadId
               : m_client   ? m_client->currentThreadId()
                            : 0;
  if (m_client)
    bindFrame(m_client->cachedStackFrames(m_threadId));
  // A console call is a result to inspect; stepping must not call it again.
  static const QRegularExpression call(
      QStringLiteral("\\b[A-Za-z_]\\w*\\s*\\("));
  const QSignalBlocker blocker(m_liveUpdates);
  m_liveUpdates->setEnabled(!m_expression.isEmpty());
  m_liveUpdates->setChecked(!m_expression.isEmpty() &&
                            !call.match(m_expression).hasMatch());
  m_refresh->setEnabled(m_client &&
                        m_client->state() == DapClient::State::Stopped &&
                        !m_expression.isEmpty());
  renderValue(value, false, lengthExpression);
  if (m_client && m_threadId > 0 && m_frameFromBottom < 0 && frameId >= 0)
    m_client->getStackTrace(m_threadId, 0, 100);
}

void DebugValueInspector::renderValue(const DapVariable &value,
                                      bool preserveView,
                                      const QString &lengthExpression) {
  const bool bufferView = preserveView && m_bufferView;
  const int tab = m_tabs->currentIndex();
  if (preserveView)
    rememberView();
  clearValue();
  m_value = value;
  m_live = m_client && m_client->state() == DapClient::State::Stopped;
  m_autoTable = !preserveView;
  if (!preserveView)
    m_filter->clear();
  m_title->setText(value.type.isEmpty()
                       ? value.name
                       : tr("%1  ·  %2").arg(value.name, value.type));
  m_watch->setEnabled(!value.evaluateName.isEmpty());
  m_text->setPlainText(DebugValueFormatter::format(value.value));
  m_root = appendValue(nullptr, value);
  m_tree->setCurrentItem(m_root);
  m_tabs->setCurrentIndex(
      preserveView ? tab : (value.variablesReference > 0 ? 0 : 2));
  m_status->setText(
      value.variablesReference > 0
          ? tr("Expand a row to inspect its children. Table view aligns nested "
               "sequences and records.")
          : tr("Formatted adapter value. Select text to copy any part."));
  const bool array =
      supportsNativeBuffers() && !value.evaluateName.isEmpty() &&
      DebugValueFormatter::canViewBuffer(value.type, value.memoryReference);
  m_arrayControls->setVisible(array);
  m_arrayControls->setEnabled(m_live);
  const int knownLength = DebugValueFormatter::arrayLength(value.type);
  if (!preserveView) {
    m_arrayExpression->setText(value.evaluateName);
    m_lengthExpression->clear();
    m_arrayCount->setSpecialValueText(tr("Set length"));
    m_arrayCount->setValue(knownLength > 0 ? knownLength : 0);
    m_arrayColumns->setValue(1);
    m_arrayLayout->setCurrentIndex(0);
  }
  const QString pointee = DebugValueFormatter::pointeeType(value.type);
  m_elementType->setItemText(0, pointee.isEmpty()
                                    ? tr("Declared type")
                                    : tr("Declared type (%1)").arg(pointee));
  if (!preserveView)
    m_elementType->setCurrentIndex(0);
  if (bufferView && array && m_live) {
    m_lengthResolved = m_arrayCount->value() > 0;
    applyArrayView();
    m_tabs->setCurrentIndex(tab);
    return;
  }
  if (array && knownLength == 0) {
    m_status->setText(
        tr("A pointer has no array length. Set Elements and choose "
           "View array to inspect its contents."));
  }
  // GDB presents char arrays as strings without any expandable children.
  if (array && knownLength > 0 && value.variablesReference == 0 && m_live) {
    m_tabs->setCurrentIndex(preserveView ? tab : 1);
    m_root->setExpanded(true);
    restoreView();
    return;
  }
  if (array && (!lengthExpression.isEmpty() ||
                DebugValueFormatter::pointerDepth(value.type) >= 2)) {
    m_status->setText(tr("Pointer to rows. Set the outer Elements count or "
                         "a length expression to inspect all rows."));
    if (!lengthExpression.isEmpty() && m_live) {
      m_lengthExpression->setText(lengthExpression);
      applyArrayView();
    } else {
      restoreView();
    }
    return;
  }
  QByteArray bytes;
  if (supportsNativeBuffers() && value.variablesReference == 0 &&
      DebugValueFormatter::stringBytes(value.value, &bytes)) {
    m_stringView = true;
    m_root->setData(0, IndexedCount, bytes.size());
    m_root->setData(0, StringBytes, bytes);
    if (!bytes.isEmpty())
      m_root->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    loadStringPage(m_root, 0);
    if (!preserveView && m_displayFormat->currentIndex() > 0)
      m_tabs->setCurrentIndex(1);
    m_status->setText(
        tr("%1 bytes. Text shows the string; Table shows character values.")
            .arg(bytes.size()));
  } else if (hasKnownEmptyChildren(value)) {
    m_status->setText(tr("Empty collection — 0 elements."));
  }
  m_root->setExpanded(true);
  restoreView();
}

void DebugValueInspector::focusArrayControls() {
  if (!m_arrayControls->isHidden()) {
    m_arrayCount->setFocus();
    m_arrayCount->selectAll();
  }
}

void DebugValueInspector::bindFrame(const QList<DapStackFrame> &frames) {
  if (m_frameFromBottom >= 0)
    return;
  for (int i = 0; i < frames.size(); ++i) {
    const auto &frame = frames.at(i);
    if (frame.id == m_frameId || (m_frameId < 0 && i == 0)) {
      const int total = qMax(static_cast<int>(frames.size()),
                             m_client->cachedStackFrameCount(m_threadId));
      m_frameFromBottom = total - 1 - i;
      m_frameName = frame.name;
      m_frameSource = frame.source.path;
      return;
    }
  }
}

void DebugValueInspector::refreshValue(bool explicitRequest) {
  if (!m_client || m_expression.isEmpty() ||
      m_client->state() != DapClient::State::Stopped)
    return;
  m_refresh->setEnabled(true);
  if ((!explicitRequest && (!m_liveUpdates->isChecked() || m_live)) ||
      m_client->hasPendingInteractiveEvaluation() || m_refreshRequest ||
      m_waitingForFrame)
    return;
  if (!m_client->isThreadStopped(m_threadId)) {
    m_status->setText(tr("Snapshot — the inspected thread is running."));
    return;
  }
  m_explicitRefresh = explicitRequest;
  m_waitingForFrame = true;
  m_status->setText(tr("Refreshing %1…").arg(m_expression));
  // Frame IDs and variable references expire whenever the program resumes.
  // Locate the original invocation again, including while stepping into calls.
  m_client->getStackTrace(m_threadId, 0, 100);
}

void DebugValueInspector::receiveFrames(int threadId,
                                        const QList<DapStackFrame> &frames) {
  if (threadId != m_threadId || !m_client ||
      m_client->state() != DapClient::State::Stopped)
    return;
  bindFrame(frames);
  if (!m_waitingForFrame)
    return;
  m_waitingForFrame = false;
  const int total = qMax(static_cast<int>(frames.size()),
                         m_client->cachedStackFrameCount(m_threadId));
  const int index = total - 1 - m_frameFromBottom;
  if (m_frameFromBottom < 0 || index < 0 || index >= frames.size() ||
      frames.at(index).name != m_frameName ||
      frames.at(index).source.path != m_frameSource) {
    m_status->setText(tr("Snapshot — %1 is out of scope. Its original stack "
                         "frame is no longer available.")
                          .arg(m_expression));
    return;
  }
  m_frameId = frames.at(index).id;
  m_refreshRequest = m_client->evaluate(m_expression, m_frameId,
                                        m_explicitRefresh ? "watch" : "hover",
                                        m_explicitRefresh);
}

QString DebugValueInspector::itemPath(const QTreeWidgetItem *item) const {
  if (!item || item == m_root)
    return {};
  const QString name = item->text(0);
  return itemPath(item->parent()) +
         (item->data(0, NamedChildren).toBool() ? 'n' : 'v') +
         QString::number(name.size()) + ':' + name;
}

void DebugValueInspector::rememberView() {
  if (!m_root || m_restoreView)
    return;
  m_savedNodes.clear();
  std::function<void(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
    if (item->data(0, PageAction).toBool())
      return;
    int count = item->childCount();
    if (count && item->child(count - 1)->data(0, PageAction).toBool())
      --count;
    m_savedNodes.insert(itemPath(item),
                        {item->data(0, NextOffset).toInt(), item->isExpanded(),
                         item->data(0, Loaded).toBool()});
    for (int i = 0; i < count; ++i)
      visit(item->child(i));
  };
  visit(m_root);
  m_selectedPath = itemPath(m_tree->currentItem());
  m_treeScroll = m_tree->verticalScrollBar()->value();
  m_tableScroll = m_table->verticalScrollBar()->value();
  m_savedTab = m_tabs->currentIndex();
  m_restoreView = true;
}

void DebugValueInspector::restoreView() {
  if (!m_restoreView || m_restoringView || !m_root || !m_live)
    return;
  m_restoringView = true;
  std::function<void(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
    const QString path = itemPath(item);
    if (!m_savedNodes.contains(path))
      return;
    const NodeView state = m_savedNodes.value(path);
    item->setExpanded(state.expanded);
    if (path == m_selectedPath)
      m_tree->setCurrentItem(item);
    if (state.loaded && !item->data(0, Loaded).toBool())
      loadChildren(item);
    const int count = item->childCount();
    if (count) {
      auto *page = item->child(count - 1);
      if (page->data(0, PageAction).toBool() &&
          state.nextPage > page->data(0, PageStart).toInt())
        loadChildren(item, page->data(0, PageStart).toInt());
    }
    for (int i = 0; i < item->childCount(); ++i) {
      auto *child = item->child(i);
      if (!child->data(0, PageAction).toBool())
        visit(child);
    }
  };
  visit(m_root);
  if (m_requests.isEmpty() && m_elementRequests.isEmpty() &&
      m_arrayPages.isEmpty() && !m_lengthRequest) {
    m_restoreView = false;
    m_savedNodes.clear();
    m_tree->verticalScrollBar()->setValue(m_treeScroll);
    m_table->verticalScrollBar()->setValue(m_tableScroll);
  }
  m_restoringView = false;
}

void DebugValueInspector::applyArrayView() {
  if (!m_live || !m_client || !supportsNativeBuffers())
    return;
  const QString length = m_lengthExpression->text().trimmed();
  if (!length.isEmpty()) {
    m_lengthResolved = false;
    m_lengthRequest = m_client->evaluate(
        length, m_frameId,
        m_restoreView && !m_explicitRefresh ? "hover" : "watch");
    m_status->setText(tr("Reading outer length from %1…").arg(length));
    return;
  }
  viewArray();
}

void DebugValueInspector::viewArray() {
  if (!m_live || !m_client || !supportsNativeBuffers())
    return;
  const QString expression = m_arrayExpression->text().trimmed();
  if (expression.isEmpty()) {
    m_status->setText(tr("Enter a pointer or array expression."));
    return;
  }
  if (m_arrayCount->value() == 0 && !m_lengthResolved) {
    m_status->setText(
        tr("Set the outer Elements count or a length expression."));
    return;
  }
  m_requests.clear();
  m_elementRequests.clear();
  m_arrayPages.clear();
  m_lengthRequest = 0;
  m_root = nullptr;
  m_tree->clear();
  m_table->clear();
  m_table->setRowCount(0);
  m_itemCount = 0;
  m_readErrors = 0;
  m_bufferView = true;
  m_stringView = false;
  m_bufferColumns = m_arrayColumns->value();
  m_bufferLayout = m_arrayLayout->currentIndex();
  m_bufferElementType =
      m_elementType->currentIndex() == 0 &&
              m_elementType->currentText() == m_elementType->itemText(0)
          ? QString()
          : m_elementType->currentText().trimmed();
  const int depth =
      m_bufferElementType.isEmpty()
          ? DebugValueFormatter::pointerDepth(m_value.type)
          : DebugValueFormatter::pointerDepth(m_bufferElementType) + 1;
  if (m_bufferLayout == 0 && depth >= 2 && m_bufferColumns > 1) {
    const QString rowType = m_bufferElementType.isEmpty()
                                ? DebugValueFormatter::pointeeType(m_value.type)
                                : m_bufferElementType;
    m_bufferLayout = DebugValueFormatter::isCharacterPointer(rowType) ? 3 : 2;
  }
  if (m_bufferLayout == 3 && m_displayFormat->currentIndex() == 0)
    m_displayFormat->setCurrentIndex(4);
  const QString base =
      m_bufferElementType.isEmpty()
          ? expression
          : QStringLiteral("((%1 *)(%2))").arg(m_bufferElementType, expression);
  DapVariable array;
  array.name = expression;
  array.type =
      tr("%1 elements%2")
          .arg(m_arrayCount->value())
          .arg(m_bufferElementType.isEmpty() ? QString()
                                             : " · " + m_bufferElementType);
  array.indexedVariables = m_arrayCount->value();
  array.evaluateName = base;
  m_root = appendValue(nullptr, array);
  m_root->setData(0, Loaded, true);
  if (array.indexedVariables > 0)
    m_root->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  m_root->setExpanded(true);
  m_tree->setCurrentItem(m_root);
  const QString element =
      m_bufferElementType.isEmpty()
          ? QStringLiteral("__typeof__((%1)[0])").arg(expression)
          : m_bufferElementType;
  if (m_client->adapterId().contains("gdb", Qt::CaseInsensitive) ||
      m_client->adapterType().compare("gdb", Qt::CaseInsensitive) == 0) {
    m_value.evaluateName =
        QStringLiteral("(*(%1))@%2").arg(base).arg(m_arrayCount->value());
  } else {
    m_value.evaluateName = QStringLiteral("*((%1 (*)[%2])(%3))")
                               .arg(element)
                               .arg(m_arrayCount->value())
                               .arg(expression);
  }
  m_watch->setEnabled(true);
  m_title->setText(tr("%1  ·  %2").arg(expression, array.type));
  m_tabs->setCurrentIndex(m_restoreView ? m_savedTab : 1);
  loadArrayPage(m_root, 0);
}

void DebugValueInspector::loadArrayPage(QTreeWidgetItem *parent, int start) {
  if (!m_live || !m_client || !parent ||
      start != parent->data(0, NextOffset).toInt())
    return;
  if (m_itemCount >= MaxTreeItems) {
    m_status->setText(tr("Tree preview limited to 10,000 items."));
    return;
  }
  if (!m_elementRequests.isEmpty()) {
    const auto page = qMakePair(parent, start);
    if (!m_arrayPages.contains(page))
      m_arrayPages.append(page);
    return;
  }
  const int total = parent->data(0, IndexedCount).toInt();
  const int end =
      qMin(total, start + qMin(PageSize, MaxTreeItems - m_itemCount));
  const QString base = parent->data(0, Expression).toString();
  removePage(parent);
  parent->setData(0, Loaded, true);
  const bool automatic = parent->data(0, FixedArray).toBool() ||
                         (m_restoreView && !m_explicitRefresh);
  for (int index = start; index < end; ++index) {
    DapVariable value;
    value.name = QString::number(index);
    value.value = tr("Loading…");
    value.evaluateName = QStringLiteral("(%1)[%2]").arg(base).arg(index);
    auto *item = appendValue(parent, value);
    const int seq = m_client->evaluate(value.evaluateName, m_frameId,
                                       automatic ? "hover" : "watch");
    m_elementRequests.insert(seq, item);
  }
  addPage(parent, end, qMin(PageSize, total - end));
  refreshDisplay();
}

void DebugValueInspector::loadStringPage(QTreeWidgetItem *parent, int start) {
  if (!parent || m_itemCount >= MaxTreeItems ||
      start != parent->data(0, NextOffset).toInt())
    return;
  const QByteArray bytes = parent->data(0, StringBytes).toByteArray();
  const int end = qMin(static_cast<int>(bytes.size()),
                       start + qMin(PageSize, MaxTreeItems - m_itemCount));
  removePage(parent);
  parent->setData(0, Loaded, true);
  for (int index = start; index < end; ++index) {
    DapVariable character;
    character.name = QString::number(index);
    character.type = QStringLiteral("unsigned char");
    const QString number = QString::number(static_cast<uchar>(bytes.at(index)));
    character.value = number + ' ' +
                      DebugValueFormatter::scalar(
                          number, character.type,
                          DebugValueFormatter::DisplayFormat::Character);
    appendValue(parent, character);
  }
  addPage(parent, end, qMin(PageSize, static_cast<int>(bytes.size()) - end));
  refreshDisplay();
}

void DebugValueInspector::receiveElement(int seq, const DapVariable &value) {
  if (seq == m_refreshRequest) {
    m_refreshRequest = 0;
    DapVariable refreshed = value;
    refreshed.name = m_valueName;
    refreshed.evaluateName = m_expression;
    renderValue(refreshed, true);
    return;
  }
  if (seq == m_lengthRequest && m_live) {
    m_lengthRequest = 0;
    bool valid = false;
    const qlonglong count =
        DebugValueFormatter::scalar(value.value, value.type,
                                    DebugValueFormatter::DisplayFormat::Decimal)
            .toLongLong(&valid);
    if (!valid || count < 0 || count > 10000) {
      m_lengthResolved = false;
      m_status->setText(
          tr("The outer length must be an integer from 0 to 10,000."));
      return;
    }
    const QSignalBlocker blocker(m_arrayCount);
    m_arrayCount->setSpecialValueText(tr("0 (empty)"));
    m_arrayCount->setValue(static_cast<int>(count));
    m_lengthResolved = true;
    viewArray();
    return;
  }
  auto *item = m_elementRequests.take(seq);
  if (!item || !m_live)
    return;
  DapVariable element = value;
  element.name = item->text(0);
  element.evaluateName = item->data(0, Expression).toString();
  updateItem(item, element);
  if (item->parent() == m_root && m_bufferLayout == 2) {
    if (DebugValueFormatter::pointerDepth(element.type) > 0) {
      item->setData(0, PointerRow, true);
      item->setData(0, IndexedCount, m_bufferColumns);
      item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    } else {
      item->setData(0, ReadError, true);
      item->setData(0, RawValue, tr("<expected a row pointer>"));
      ++m_readErrors;
    }
  } else if (item->parent() == m_root && m_bufferLayout == 3) {
    QByteArray bytes;
    if (DebugValueFormatter::pointerDepth(element.type) <= 1 &&
        DebugValueFormatter::stringBytes(element.value, &bytes)) {
      if (m_arrayLayout->currentIndex() == 0 && m_bufferColumns > 1)
        bytes.truncate(m_bufferColumns);
      item->setData(0, StringBytes, bytes);
      item->setData(0, IndexedCount, bytes.size());
      loadStringPage(item, 0);
    }
  }
  finishElementPage();
}

void DebugValueInspector::finishElementPage() {
  if (!m_elementRequests.isEmpty())
    return;
  while (!m_arrayPages.isEmpty() && m_elementRequests.isEmpty()) {
    const auto page = m_arrayPages.takeFirst();
    loadArrayPage(page.first, page.second);
  }
  refreshDisplay();
}

void DebugValueInspector::refreshDisplay() {
  std::function<void(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
    if (!item->data(0, PageAction).toBool())
      item->setText(1, DebugValueFormatter::summary(itemDisplayValue(item)));
    for (int i = 0; i < item->childCount(); ++i)
      visit(item->child(i));
  };
  if (m_root)
    visit(m_root);
  updateText();
  if (m_tabs->currentIndex() == 1)
    updateTable();
  filterTree();
  restoreView();
}

void DebugValueInspector::updateText() {
  auto *selected = m_tree->currentItem();
  if (!selected)
    return;
  QString text =
      displayValue(selected->data(0, RawValue).toString(), selected->text(2));
  if (text.isEmpty() && selected->childCount()) {
    QStringList lines;
    std::function<void(QTreeWidgetItem *, int)> visit =
        [&](QTreeWidgetItem *item, int depth) {
          if (lines.size() >= 1000 || depth > 8)
            return;
          for (int i = 0; i < item->childCount() && lines.size() < 1000; ++i) {
            auto *child = item->child(i);
            lines << QString(depth * 2, ' ') + child->text(0) + " = " +
                         child->text(1);
            visit(child, depth + 1);
          }
        };
    visit(selected, 0);
    text = lines.join('\n');
  }
  m_text->setPlainText(DebugValueFormatter::format(text));
}

void DebugValueInspector::invalidate() {
  m_live = false;
  m_requests.clear();
  m_elementRequests.clear();
  m_arrayPages.clear();
  m_lengthRequest = 0;
  if (!m_client || !m_client->evaluationExecutedTarget(m_refreshRequest))
    m_refreshRequest = 0;
  m_waitingForFrame = false;
  m_arrayControls->setEnabled(false);
  m_refresh->setEnabled(m_client &&
                        m_client->state() == DapClient::State::Stopped &&
                        !m_expression.isEmpty());
  if (m_root)
    m_status->setText(
        m_liveUpdates->isChecked()
            ? tr("Snapshot while running — updates at the next pause.")
            : tr("Snapshot. Pause and choose Refresh to read the current "
                 "value."));
}

QString DebugValueInspector::childFilter(const QTreeWidgetItem *item) const {
  if (item->data(0, NamedChildren).toBool())
    return QStringLiteral("named");
  const auto value = item->data(0, Variable).value<DapVariable>();
  if (value.indexedVariables > 0 &&
      (value.hasNamedVariables || value.namedVariables > 0 || usesDelve() ||
       usesJsDebug()))
    return QStringLiteral("indexed");
  return {};
}

void DebugValueInspector::loadChildren(QTreeWidgetItem *item, int start,
                                       int referenceOverride) {
  if (!item || start != item->data(0, NextOffset).toInt())
    return;
  if (item->data(0, StringBytes).isValid()) {
    loadStringPage(item, start);
    return;
  }
  if ((m_bufferView && item == m_root) || item->data(0, PointerRow).toBool() ||
      item->data(0, FixedArray).toBool()) {
    loadArrayPage(item, start);
    return;
  }
  if (!m_live || !m_client || item->data(0, Reference).toInt() <= 0 ||
      item->data(0, RepeatedReference).toBool())
    return;
  for (const Request &request : std::as_const(m_requests)) {
    if (request.item == item)
      return;
  }
  const QString filter = childFilter(item);
  const int count =
      pagesByIndex(filter)
          ? std::min({PageSize, MaxTreeItems - m_itemCount,
                      item->data(0, IndexedCount).toInt() - start})
          : qMin(PageSize, MaxTreeItems - m_itemCount);
  if (count <= 0) {
    m_status->setText(tr("Tree preview limited to 10,000 items. Inspect a "
                         "narrower expression to continue."));
    return;
  }

  const int continuation = item->data(0, MoreReference).toInt();
  const int reference =
      referenceOverride > 0 ? referenceOverride
      : continuation > 0 && start >= item->data(0, MoreStart).toInt()
          ? continuation
          : item->data(0, Reference).toInt();
  const bool prefix = item->data(0, PrefixPaging).toBool();
  const int seq = m_client->inspectVariables(
      reference, filter, prefix ? 0 : start,
      prefix ? qMin(MaxTreeItems, start + count) : count);
  if (seq > 0) {
    m_requests.insert(seq, {item, start, count, reference, prefix});
    m_status->setText(tr("Loading children…"));
  }
}

bool DebugValueInspector::repeatsPrefix(const QList<DapVariable> &variables,
                                        const QTreeWidgetItem *item,
                                        int count) {
  int matched = 0;
  for (int i = 0; i < item->childCount() && matched < count; ++i) {
    const auto *child = item->child(i);
    if (child->data(0, NamedChildren).toBool())
      continue;
    const auto &value = variables.at(matched);
    if (child->text(0) != value.name || child->text(2) != value.type)
      return false;
    ++matched;
  }
  return matched == count;
}

void DebugValueInspector::receiveChildren(int seq,
                                          const QList<DapVariable> &variables) {
  if (!m_requests.contains(seq) || !m_live)
    return;
  const Request request = m_requests.take(seq);
  auto *parent = request.item;
  if (isPythonSequence(parent)) {
    receiveSequenceChildren(request, variables);
    return;
  }

  const QString filter = childFilter(parent);
  const bool byIndex = pagesByIndex(filter);
  const bool delveIndexed = usesDelve() && filter == QLatin1String("indexed");
  if (!byIndex && request.start == 0 && variables.size() > request.count)
    parent->setData(0, PrefixPaging, true);
  if (!byIndex && !request.prefix && request.start > 0 &&
      !variables.isEmpty() &&
      repeatsPrefix(variables, parent,
                    qMin(request.start, static_cast<int>(variables.size())))) {
    // Some adapters honor count while always returning children from zero.
    parent->setData(0, PrefixPaging, true);
    loadChildren(parent, request.start, request.reference);
    return;
  }

  const int indexed = parent->data(0, IndexedCount).toInt();
  const int named = parent->data(0, NamedCount).toInt();
  const int total = filter == QLatin1String("indexed") ? indexed
                    : filter == QLatin1String("named") ? named
                                                       : qMax(indexed, named);
  if (request.start == 0 && filter == QLatin1String("indexed") && named > 0 &&
      m_itemCount < MaxTreeItems) {
    DapVariable properties;
    properties.name = tr("Named children");
    properties.variablesReference = request.reference;
    properties.namedVariables = named;
    auto *group = new QTreeWidgetItem(parent);
    group->setData(0, NamedChildren, true);
    ++m_itemCount;
    updateItem(group, properties);
  }

  if (byIndex && usesJsDebug() && named == 0) {
    // js-debug's Map and Set repeat their named properties on every indexed
    // page. Keep them once and page the backing entries by their slot indices.
    QList<DapVariable> members;
    const int next = request.start + request.count;
    QSet<QString> loadedNames;
    for (int i = 0; i < parent->childCount(); ++i) {
      const auto *child = parent->child(i);
      if (!child->data(0, PageAction).toBool())
        loadedNames.insert(child->text(0));
    }
    for (const auto &value : variables) {
      const qlonglong index = indexForName(value.name);
      if (!loadedNames.contains(value.name) ||
          (index >= request.start && index < next))
        members.append(value);
    }
    appendChildren(request, members, next, next < total);
    return;
  }

  const int offset = request.prefix ? request.start : 0;
  const int available = qMax(0, static_cast<int>(variables.size()) - offset);
  // Delve can return two variables per map entry, but its start/count use
  // entries. Keep both the key and the value and advance by entries.
  const int shown = qMin(available, request.count * (delveIndexed ? 2 : 1));
  const int next = request.start + (byIndex ? request.count : shown);
  const bool more = byIndex
                        ? next < total
                        : shown > 0 && (total > next || available > shown ||
                                        (total == 0 && shown == request.count));
  appendChildren(request, variables.mid(offset, shown), next, more);
}

void DebugValueInspector::receiveSequenceChildren(
    const Request &request, const QList<DapVariable> &variables) {
  auto *parent = request.item;
  const bool hasIndices = std::any_of(
      variables.cbegin(), variables.cend(),
      [](const DapVariable &value) { return indexForName(value.name) >= 0; });

  if (!hasIndices) {
    // debugpy exposes NumPy arrays and long sequences through range nodes.
    static const QRegularExpression range(
        QStringLiteral(R"(^\[(\d+):(\d+)\]$)"));
    for (const auto &value : variables) {
      const auto match = range.match(value.name.trimmed());
      if (!match.hasMatch() || value.variablesReference <= 0 ||
          value.variablesReference == request.reference)
        continue;
      const int begin = match.captured(1).toInt();
      const int end = match.captured(2).toInt();
      if (request.start < begin || request.start >= end)
        continue;
      if (request.start == 0 && parent->text(2) == QLatin1String("ndarray")) {
        parent->setData(0, Reference, value.variablesReference);
        parent->setData(0, IndexedCount, end);
        if (parent == m_root) {
          QStringList details;
          for (const auto &metadata : variables) {
            if (metadata.name == QLatin1String("shape"))
              details << metadata.name + ' ' + metadata.value;
            else if (metadata.name == QLatin1String("dtype"))
              details << metadata.value;
          }
          if (!details.isEmpty())
            m_title->setText(m_value.name + "  ·  " + details.join("  ·  "));
        }
      }
      loadChildren(parent, request.start, value.variablesReference);
      return;
    }
  }

  QList<DapVariable> elements;
  QList<DapVariable> properties;
  int continuation = 0;
  qlonglong lastIndex = -1;
  for (const auto &value : variables) {
    if (value.name == QLatin1String("more") &&
        (value.type == QLatin1String("MoreItems") ||
         value.type == QLatin1String("MoreItemsRange")) &&
        value.variablesReference > 0) {
      continuation = value.variablesReference;
    } else if (value.name == QLatin1String("len()")) {
      bool ok = false;
      const int total = value.value.toInt(&ok);
      if (ok && total >= 0)
        parent->setData(0, IndexedCount, total);
    } else {
      const qlonglong index = indexForName(value.name);
      lastIndex = qMax(lastIndex, index);
      if (index >= request.start && index < request.start + request.count)
        elements.append(value);
      else if (index < 0 && request.start == 0 &&
               value.name != QLatin1String("special variables") &&
               value.name != QLatin1String("function variables"))
        properties.append(value);
    }
  }
  if (continuation > 0) {
    parent->setData(0, MoreReference, continuation);
    parent->setData(0, MoreStart,
                    lastIndex < MaxTreeItems ? lastIndex + 1 : MaxTreeItems);
  }
  if (elements.isEmpty() && hasIndices && request.start > 0 &&
      !request.prefix) {
    parent->setData(0, PrefixPaging, true);
    loadChildren(parent, request.start, request.reference);
    return;
  }
  std::stable_sort(elements.begin(), elements.end(),
                   [](const DapVariable &a, const DapVariable &b) {
                     return indexForName(a.name) < indexForName(b.name);
                   });
  const int total = parent->data(0, IndexedCount).toInt();
  const int next = request.start + elements.size();
  const bool more =
      !elements.isEmpty() &&
      (total > next || (total == 0 && elements.size() == request.count));
  appendChildren(request, elements + properties, next, more);
}

void DebugValueInspector::appendChildren(const Request &request,
                                         const QList<DapVariable> &variables,
                                         int next, bool more) {
  auto *parent = request.item;
  removePage(parent);
  const int shown =
      qMin(static_cast<int>(variables.size()), MaxTreeItems - m_itemCount);
  for (int i = 0; i < shown; ++i)
    appendValue(parent, variables.at(i));
  parent->setData(0, Loaded, true);
  if (parent->childCount() == 0 && !more)
    parent->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
  addPage(parent, next, more ? PageSize : 0);
  m_status->setText(m_itemCount >= MaxTreeItems
                        ? tr("Tree preview limited to 10,000 items. Inspect a "
                             "narrower expression to continue.")
                    : parent->childCount() == 0
                        ? tr("Empty collection — 0 elements.")
                        : tr("%1 children loaded. Expand rows for details.")
                              .arg(parent->childCount() - (more ? 1 : 0)));

  if (parent == m_root && m_autoTable && !variables.isEmpty() &&
      (m_value.indexedVariables > 0 ||
       indexForName(variables.first().name) >= 0)) {
    const bool nested = std::any_of(
        variables.cbegin(), variables.cend(),
        [](const DapVariable &value) { return value.variablesReference > 0; });
    if (supportsNativeBuffers() || nested)
      m_tabs->setCurrentIndex(1);
  }
  if (m_tabs->currentIndex() == 1)
    updateTable();
  updateText();
  filterTree();
  restoreView();
}

void DebugValueInspector::addPage(QTreeWidgetItem *parent, int next,
                                  int count) {
  parent->setData(0, NextOffset, next);
  if (count <= 0 || m_itemCount >= MaxTreeItems)
    return;
  auto *page = new QTreeWidgetItem(parent);
  page->setText(0, parent->data(0, StringBytes).isValid()
                       ? tr("Load next %1 characters…").arg(count)
                       : tr("Load next %1 items…").arg(count));
  page->setToolTip(0, tr("Double-click or press Enter to load another page."));
  page->setData(0, PageAction, true);
  page->setData(0, PageStart, next);
}

void DebugValueInspector::updateTable() {
  if (!m_root || m_updatingTable)
    return;
  const QScopedValueRollback updating(m_updatingTable, true);
  QList<QTreeWidgetItem *> rows;
  const bool pointerRows = m_bufferLayout == 2 || m_bufferLayout == 3;
  const int rowLimit =
      TableRows * (m_bufferView && !pointerRows ? m_bufferColumns : 1);
  for (int i = 0; i < m_root->childCount() && rows.size() < rowLimit; ++i) {
    auto *item = m_root->child(i);
    if (!item->data(0, PageAction).toBool() &&
        !item->data(0, NamedChildren).toBool())
      rows << item;
  }
  QStringList columns;
  bool nested = false;
  for (auto *row : rows) {
    if (hasChildren(row)) {
      nested = true;
      if (!row->data(0, Loaded).toBool())
        loadChildren(row);
      for (int i = 0; i < row->childCount() && columns.size() < TableColumns;
           ++i) {
        auto *cell = row->child(i);
        if (!cell->data(0, PageAction).toBool() &&
            !cell->data(0, NamedChildren).toBool() &&
            !columns.contains(cell->text(0)))
          columns << cell->text(0);
      }
    }
  }
  if (m_bufferView && m_bufferColumns > 1 && !nested && !pointerRows) {
    const int count = (rows.size() + m_bufferColumns - 1) / m_bufferColumns;
    m_table->clear();
    m_table->setRowCount(count);
    m_table->setColumnCount(m_bufferColumns);
    QStringList headings;
    for (int c = 0; c < m_bufferColumns; ++c)
      headings << QStringLiteral("[%1]").arg(c);
    m_table->setHorizontalHeaderLabels(headings);
    QStringList labels;
    for (int r = 0; r < count; ++r) {
      labels << rows.at(r * m_bufferColumns)->text(0);
      for (int c = 0; c < m_bufferColumns; ++c) {
        const int index = r * m_bufferColumns + c;
        auto *cell = new QTableWidgetItem;
        if (index < rows.size()) {
          auto *item = rows.at(index);
          const QString text = itemDisplayValue(item);
          cell->setText(DebugValueFormatter::summary(text, 256));
          cell->setData(Qt::UserRole, text);
          cell->setData(Qt::UserRole + 1, m_root->indexOfChild(item));
          cell->setToolTip(item->data(0, Expression).toString() + '\n' + text);
        }
        m_table->setItem(r, c, cell);
      }
    }
    m_table->setVerticalHeaderLabels(labels);
    m_tabs->setTabText(1,
                       tr("Table (%1 × %2)").arg(count).arg(m_bufferColumns));
  } else {
    while (rows.size() > TableRows)
      rows.removeLast();
    if (columns.isEmpty())
      columns << tr("Value");
    m_table->clear();
    m_table->setRowCount(rows.size());
    m_table->setColumnCount(columns.size());
    m_table->setHorizontalHeaderLabels(columns);
    QStringList labels;
    for (int r = 0; r < rows.size(); ++r) {
      auto *row = rows.at(r);
      labels << row->text(0);
      QHash<QString, QTreeWidgetItem *> cells;
      for (int i = 0; i < row->childCount(); ++i) {
        auto *cell = row->child(i);
        if (!cell->data(0, PageAction).toBool())
          cells.insert(cell->text(0), cell);
      }
      for (int c = 0; c < columns.size(); ++c) {
        const bool scalar = !nested || !hasChildren(row);
        auto *item =
            scalar ? (c == 0 ? row : nullptr) : cells.value(columns.at(c));
        const QString text =
            item ? item->data(0, RawValue).toString() : QString();
        const QString display = item ? itemDisplayValue(item) : QString();
        auto *cell =
            new QTableWidgetItem(DebugValueFormatter::summary(display, 256));
        cell->setData(Qt::UserRole, display);
        if (item) {
          cell->setData(Qt::UserRole + 1, m_root->indexOfChild(row));
          if (item != row)
            cell->setData(Qt::UserRole + 2, row->indexOfChild(item));
        }
        cell->setToolTip(
            tr("Double-click or press Enter to inspect in Tree.\n%1")
                .arg(DebugValueFormatter::format(text)));
        bool numeric = false;
        display.toDouble(&numeric);
        if (numeric)
          cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, c, cell);
      }
    }
    m_table->setVerticalHeaderLabels(labels);
    m_tabs->setTabText(
        1, tr("Table (%1 × %2)").arg(rows.size()).arg(columns.size()));
  }
  if (m_bufferView && m_live) {
    m_status->setText(
        tr("%1 of %2 outer elements loaded%3%4. Use Tree to load more.")
            .arg(
                m_root->childCount() -
                (m_root->childCount() && m_root->child(m_root->childCount() - 1)
                                             ->data(0, PageAction)
                                             .toBool()
                     ? 1
                     : 0))
            .arg(m_root->data(0, IndexedCount).toInt())
            .arg(m_elementRequests.isEmpty() ? QString() : tr(" · reading…"))
            .arg(m_readErrors ? tr(" · %1 unreadable").arg(m_readErrors)
                              : QString()));
  } else if (m_requests.isEmpty() && m_elementRequests.isEmpty() && m_live &&
             m_itemCount < MaxTreeItems) {
    m_status->setText(tr("%1 rows × %2 columns shown · preview limited to %3 "
                         "rows and %4 columns. Blank cells represent missing "
                         "entries; use Tree to inspect more.")
                          .arg(m_table->rowCount())
                          .arg(m_table->columnCount())
                          .arg(TableRows)
                          .arg(TableColumns));
  }
}

void DebugValueInspector::showTreeMenu(const QPoint &pos) {
  auto *item = m_tree->itemAt(pos);
  if (!m_live || !item || item == m_root ||
      !item->data(0, Variable).isValid() ||
      item->data(0, NamedChildren).toBool())
    return;
  const QPersistentModelIndex index(m_tree->indexFromItem(item));
  const auto value = item->data(0, Variable).value<DapVariable>();
  QMenu menu(m_tree);
  QAction *inspect = menu.addAction(tr("Inspect Value…"));
  QAction *array = nullptr;
  if (supportsNativeBuffers() &&
      !item->data(0, Expression).toString().isEmpty() &&
      DebugValueFormatter::canViewBuffer(value.type, value.memoryReference))
    array = menu.addAction(tr("View as Array / Buffer…"));
  const QAction *selected = menu.exec(m_tree->viewport()->mapToGlobal(pos));
  if (!selected || !m_live || !index.isValid())
    return;
  item = m_tree->itemFromIndex(index);
  if (!item)
    return;
  auto child = item->data(0, Variable).value<DapVariable>();
  child.evaluateName = item->data(0, Expression).toString();
  if (selected == inspect || selected == array)
    emit inspectValueRequested(child, m_frameId, m_threadId, selected == array);
}

void DebugValueInspector::filterTree() {
  const QString query = m_filter->text().trimmed();
  std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
    bool matches = query.isEmpty() ||
                   item->text(0).contains(query, Qt::CaseInsensitive) ||
                   item->data(0, RawValue)
                       .toString()
                       .contains(query, Qt::CaseInsensitive) ||
                   item->text(2).contains(query, Qt::CaseInsensitive) ||
                   item->data(0, PageAction).toBool();
    for (int i = 0; i < item->childCount(); ++i)
      matches = visit(item->child(i)) || matches;
    item->setHidden(!matches);
    return matches;
  };
  if (m_root)
    visit(m_root);
  for (int r = 0; r < m_table->rowCount(); ++r) {
    bool matches =
        query.isEmpty() || m_table->verticalHeaderItem(r)->text().contains(
                               query, Qt::CaseInsensitive);
    for (int c = 0; c < m_table->columnCount(); ++c) {
      auto *item = m_table->item(r, c);
      matches = matches || (item && item->data(Qt::UserRole)
                                        .toString()
                                        .contains(query, Qt::CaseInsensitive));
    }
    m_table->setRowHidden(r, !matches);
  }
}

void DebugValueInspector::copySelection() {
  QString text;
  if (m_tabs->currentIndex() == 1 && !m_table->selectedRanges().isEmpty()) {
    const auto range = m_table->selectedRanges().first();
    QStringList lines;
    for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
      QStringList cells;
      for (int c = range.leftColumn(); c <= range.rightColumn(); ++c) {
        auto *item = m_table->item(r, c);
        cells << (item ? item->data(Qt::UserRole).toString() : QString());
      }
      lines << cells.join('\t');
    }
    text = lines.join('\n');
  } else {
    text =
        m_text->textCursor().hasSelection()
            ? m_text->textCursor().selectedText().replace(QChar(0x2029), '\n')
            : m_text->toPlainText();
  }
  QApplication::clipboard()->setText(text);
}

void DebugValueInspector::applyTheme(const Theme &theme) {
  QPalette colors = palette();
  colors.setColor(QPalette::Window, theme.backgroundColor);
  colors.setColor(QPalette::WindowText, theme.foregroundColor);
  colors.setColor(QPalette::Base, theme.surfaceColor);
  colors.setColor(QPalette::AlternateBase, theme.surfaceAltColor);
  colors.setColor(QPalette::Text, theme.foregroundColor);
  colors.setColor(QPalette::Button, theme.surfaceColor);
  colors.setColor(QPalette::ButtonText, theme.foregroundColor);
  colors.setColor(QPalette::Highlight, theme.accentSoftColor);
  colors.setColor(QPalette::HighlightedText,
                  UIStyleHelper::readableText(theme, theme.accentSoftColor,
                                              theme.foregroundColor));
  setAutoFillBackground(true);
  setPalette(colors);
  m_tree->setPalette(colors);
  m_table->setPalette(colors);
  for (QHeaderView *header :
       {m_table->horizontalHeader(), m_table->verticalHeader()}) {
    header->setPalette(colors);
    header->viewport()->setPalette(colors);
    header->setStyleSheet(QString("QHeaderView { background: %1; }")
                              .arg(theme.surfaceColor.name()));
  }
  m_title->setStyleSheet(UIStyleHelper::headingStyle(theme, 13));
  m_status->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  setStyleSheet(QString("QWidget#%1 { background: %2; color: %3; }")
                    .arg(objectName(), theme.backgroundColor.name(),
                         theme.foregroundColor.name()) +
                UIStyleHelper::formDialogStyle(theme) +
                UIStyleHelper::treeWidgetStyle(theme) +
                UIStyleHelper::tableWidgetStyle(theme) +
                UIStyleHelper::plainTextEditStyle(theme) +
                UIStyleHelper::lineEditStyle(theme) +
                UIStyleHelper::comboBoxStyle(theme) +
                UIStyleHelper::checkBoxStyle(theme) +
                UIStyleHelper::spinBoxStyle(theme) +
                UIStyleHelper::tabWidgetStyle(theme) +
                UIStyleHelper::secondaryButtonStyle(theme) +
                UIStyleHelper::scrollBarStyle(theme) +
                QString("QTableWidget { alternate-background-color: %1; }")
                    .arg(theme.surfaceAltColor.name()));
}
