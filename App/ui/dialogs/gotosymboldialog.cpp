#include "gotosymboldialog.h"
#include <QApplication>
#include <algorithm>

GoToSymbolDialog::GoToSymbolDialog(QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_searchBox(nullptr)
    , m_resultsList(nullptr)
    , m_layout(nullptr)
{
    setupUI();
}

GoToSymbolDialog::~GoToSymbolDialog()
{
}

void GoToSymbolDialog::setupUI()
{
    setMinimumWidth(500);
    setMaximumHeight(400);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(4);

    // Search box
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Go to symbol..."));
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px;"
        "  font-size: 14px;"
        "  border: 1px solid #2a3241;"
        "  border-radius: 4px;"
        "  background: #1f2632;"
        "  color: #e6edf3;"
        "}"
    );
    m_layout->addWidget(m_searchBox);

    // Results list
    m_resultsList = new QListWidget(this);
    m_resultsList->setStyleSheet(
        "QListWidget {"
        "  border: none;"
        "  background: #0e1116;"
        "  color: #e6edf3;"
        "}"
        "QListWidget::item {"
        "  padding: 8px;"
        "  border-bottom: 1px solid #2a3241;"
        "}"
        "QListWidget::item:selected {"
        "  background: #1b2a43;"
        "}"
        "QListWidget::item:hover {"
        "  background: #222a36;"
        "}"
    );
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layout->addWidget(m_resultsList);

    // Connections
    connect(m_searchBox, &QLineEdit::textChanged, this, &GoToSymbolDialog::onSearchTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated, this, &GoToSymbolDialog::onItemActivated);
    connect(m_resultsList, &QListWidget::itemClicked, this, &GoToSymbolDialog::onItemClicked);

    // Install event filter for keyboard navigation
    m_searchBox->installEventFilter(this);

    setStyleSheet("GoToSymbolDialog { background: #171c24; border: 1px solid #2a3241; border-radius: 8px; }");
}

void GoToSymbolDialog::setSymbols(const QList<LspDocumentSymbol>& symbols)
{
    m_symbols.clear();
    flattenSymbols(symbols);
    updateResults(QString());
}

void GoToSymbolDialog::flattenSymbols(const QList<LspDocumentSymbol>& symbols, const QString& prefix)
{
    for (const auto& symbol : symbols) {
        SymbolItem item;
        item.name = prefix.isEmpty() ? symbol.name : prefix + "." + symbol.name;
        item.detail = symbol.detail;
        item.kind = symbol.kind;
        item.line = symbol.selectionRange.start.line;
        item.column = symbol.selectionRange.start.character;
        item.score = 0;
        m_symbols.append(item);
        
        // Recursively add children
        if (!symbol.children.isEmpty()) {
            flattenSymbols(symbol.children, item.name);
        }
    }
}

void GoToSymbolDialog::clearSymbols()
{
    m_symbols.clear();
    m_filteredIndices.clear();
    m_resultsList->clear();
}

void GoToSymbolDialog::showDialog()
{
    m_searchBox->clear();
    updateResults(QString());

    // Position at top center of parent
    if (parentWidget()) {
        QPoint parentCenter = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        int x = parentCenter.x() - width() / 2;
        int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
        move(x, y);
    }

    show();
    m_searchBox->setFocus();
    
    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }
}

void GoToSymbolDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        hide();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_resultsList->currentRow() >= 0) {
            selectSymbol(m_resultsList->currentRow());
        }
        break;
    case Qt::Key_Up:
        selectPrevious();
        break;
    case Qt::Key_Down:
        selectNext();
        break;
    default:
        QDialog::keyPressEvent(event);
    }
}

bool GoToSymbolDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_searchBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Up:
            selectPrevious();
            return true;
        case Qt::Key_Down:
            selectNext();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_resultsList->currentRow() >= 0) {
                selectSymbol(m_resultsList->currentRow());
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void GoToSymbolDialog::onSearchTextChanged(const QString& text)
{
    updateResults(text);
}

void GoToSymbolDialog::onItemActivated(QListWidgetItem* item)
{
    int row = m_resultsList->row(item);
    if (row >= 0) {
        selectSymbol(row);
    }
}

void GoToSymbolDialog::onItemClicked(QListWidgetItem* item)
{
    onItemActivated(item);
}

void GoToSymbolDialog::updateResults(const QString& query)
{
    m_resultsList->clear();
    m_filteredIndices.clear();

    QList<QPair<int, int>> scored;  // score, index

    for (int i = 0; i < m_symbols.size(); ++i) {
        int score = 0;
        if (query.isEmpty()) {
            // When empty, order by position in document
            score = 1000 - i;
        } else {
            score = fuzzyMatch(query.toLower(), m_symbols[i].name.toLower());
        }

        if (score > 0) {
            scored.append(qMakePair(score, i));
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.first > b.first;
    });

    // Limit results
    int maxResults = 20;
    for (int i = 0; i < qMin(scored.size(), maxResults); ++i) {
        int idx = scored[i].second;
        m_filteredIndices.append(idx);

        const SymbolItem& sym = m_symbols[idx];
        QListWidgetItem* item = new QListWidgetItem();
        
        QString displayText = symbolKindIcon(sym.kind) + " " + sym.name;
        if (!sym.detail.isEmpty()) {
            displayText += "  - " + sym.detail;
        }
        displayText += QString("  :%1").arg(sym.line + 1);  // Line numbers are 0-based
        
        item->setText(displayText);
        item->setData(Qt::UserRole, idx);
        m_resultsList->addItem(item);
    }

    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }

    // Adjust height
    int itemHeight = 35;
    int newHeight = qMin(m_resultsList->count() * itemHeight + 60, 400);
    setFixedHeight(newHeight);
}

int GoToSymbolDialog::fuzzyMatch(const QString& pattern, const QString& text)
{
    if (pattern.isEmpty())
        return 1000;

    // Exact match gets highest score
    if (text.contains(pattern))
        return 2000 + (1000 - text.indexOf(pattern));

    // Fuzzy matching - all characters must appear in order
    int patternIdx = 0;
    int score = 0;
    int lastMatchIdx = -1;

    for (int i = 0; i < text.length() && patternIdx < pattern.length(); ++i) {
        if (text[i] == pattern[patternIdx]) {
            // Bonus for consecutive matches
            if (lastMatchIdx == i - 1) {
                score += 15;
            }
            // Bonus for word boundary matches (after . or _)
            if (i == 0 || text[i - 1] == '.' || text[i - 1] == '_') {
                score += 10;
            }
            score += 10;
            lastMatchIdx = i;
            patternIdx++;
        }
    }

    // All pattern characters must be matched
    if (patternIdx != pattern.length())
        return 0;

    return score;
}

void GoToSymbolDialog::selectSymbol(int row)
{
    if (row < 0 || row >= m_filteredIndices.size())
        return;

    int symIdx = m_filteredIndices[row];
    if (symIdx < 0 || symIdx >= m_symbols.size())
        return;

    hide();

    const SymbolItem& sym = m_symbols[symIdx];
    emit symbolSelected(sym.line, sym.column);
}

void GoToSymbolDialog::selectNext()
{
    int current = m_resultsList->currentRow();
    if (current < m_resultsList->count() - 1) {
        m_resultsList->setCurrentRow(current + 1);
    }
}

void GoToSymbolDialog::selectPrevious()
{
    int current = m_resultsList->currentRow();
    if (current > 0) {
        m_resultsList->setCurrentRow(current - 1);
    }
}

QString GoToSymbolDialog::symbolKindIcon(LspSymbolKind kind) const
{
    // Using simple Unicode symbols for better cross-platform and accessibility compatibility
    switch (kind) {
    case LspSymbolKind::File: return "\u2630";      // ☰ trigram for heaven
    case LspSymbolKind::Module: return "\u25A6";    // ▦ square with orthogonal crosshatch
    case LspSymbolKind::Namespace: return "\u25C7"; // ◇ white diamond
    case LspSymbolKind::Package: return "\u25A6";   // ▦ square with orthogonal crosshatch
    case LspSymbolKind::Class: return "\u25C6";     // ◆ black diamond
    case LspSymbolKind::Method: return "\u25B8";    // ▸ black right-pointing small triangle
    case LspSymbolKind::Property: return "\u25CB";  // ○ white circle
    case LspSymbolKind::Field: return "\u25A1";     // □ white square
    case LspSymbolKind::Constructor: return "\u25B2"; // ▲ black up-pointing triangle
    case LspSymbolKind::Enum: return "\u2261";      // ≡ identical to
    case LspSymbolKind::Interface: return "\u25C7"; // ◇ white diamond
    case LspSymbolKind::Function: return "\u0192";  // ƒ latin small letter f with hook
    case LspSymbolKind::Variable: return "\u03BD";  // ν greek small letter nu
    case LspSymbolKind::Constant: return "\u03C0";  // π greek small letter pi
    case LspSymbolKind::String: return "\u0022";    // " quotation mark
    case LspSymbolKind::Number: return "\u0023";    // # number sign
    case LspSymbolKind::Boolean: return "\u2713";   // ✓ check mark
    case LspSymbolKind::Array: return "\u25A4";     // ▤ square with horizontal fill
    case LspSymbolKind::Object: return "\u25A3";    // ▣ white square containing small black square
    case LspSymbolKind::Key: return "\u25CB";       // ○ white circle
    case LspSymbolKind::Null: return "\u2205";      // ∅ empty set
    case LspSymbolKind::EnumMember: return "\u2261"; // ≡ identical to
    case LspSymbolKind::Struct: return "\u25A0";    // ■ black square
    case LspSymbolKind::Event: return "\u26A1";     // ⚡ high voltage sign
    case LspSymbolKind::Operator: return "\u002B";  // + plus sign
    case LspSymbolKind::TypeParameter: return "\u03C4"; // τ greek small letter tau
    default: return "\u2022";                       // • bullet
    }
}

QString GoToSymbolDialog::symbolKindName(LspSymbolKind kind) const
{
    switch (kind) {
    case LspSymbolKind::File: return tr("File");
    case LspSymbolKind::Module: return tr("Module");
    case LspSymbolKind::Namespace: return tr("Namespace");
    case LspSymbolKind::Package: return tr("Package");
    case LspSymbolKind::Class: return tr("Class");
    case LspSymbolKind::Method: return tr("Method");
    case LspSymbolKind::Property: return tr("Property");
    case LspSymbolKind::Field: return tr("Field");
    case LspSymbolKind::Constructor: return tr("Constructor");
    case LspSymbolKind::Enum: return tr("Enum");
    case LspSymbolKind::Interface: return tr("Interface");
    case LspSymbolKind::Function: return tr("Function");
    case LspSymbolKind::Variable: return tr("Variable");
    case LspSymbolKind::Constant: return tr("Constant");
    case LspSymbolKind::String: return tr("String");
    case LspSymbolKind::Number: return tr("Number");
    case LspSymbolKind::Boolean: return tr("Boolean");
    case LspSymbolKind::Array: return tr("Array");
    case LspSymbolKind::Object: return tr("Object");
    case LspSymbolKind::Key: return tr("Key");
    case LspSymbolKind::Null: return tr("Null");
    case LspSymbolKind::EnumMember: return tr("Enum Member");
    case LspSymbolKind::Struct: return tr("Struct");
    case LspSymbolKind::Event: return tr("Event");
    case LspSymbolKind::Operator: return tr("Operator");
    case LspSymbolKind::TypeParameter: return tr("Type Parameter");
    default: return tr("Symbol");
    }
}
