#ifndef COMPLETIONITEM_H
#define COMPLETIONITEM_H

#include <QString>
#include <QIcon>

/**
 * @brief Kind of completion item, following LSP specification
 * 
 * Values match LSP CompletionItemKind for compatibility.
 */
enum class CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25
};

/**
 * @brief A rich completion item for autocompletion suggestions
 * 
 * This structure represents a single completion suggestion that can be
 * displayed in the completion popup. It supports:
 * - Rich display with icons and documentation
 * - Snippet expansion with placeholders
 * - Priority-based sorting
 * - Custom filtering and sorting text
 */
struct CompletionItem {
    /**
     * @brief Display text shown in completion popup
     */
    QString label;
    
    /**
     * @brief Text to insert when completion is accepted
     * 
     * If empty, label is used. May contain snippet placeholders
     * like ${1:placeholder} when isSnippet is true.
     */
    QString insertText;
    
    /**
     * @brief Text used for filtering/matching
     * 
     * If empty, label is used for matching.
     */
    QString filterText;
    
    /**
     * @brief Kind of completion item for icon selection
     */
    CompletionItemKind kind = CompletionItemKind::Text;
    
    /**
     * @brief Short detail text (e.g., type signature)
     * 
     * Displayed alongside the label in the popup.
     */
    QString detail;
    
    /**
     * @brief Full documentation for the item
     * 
     * Displayed in a separate documentation panel.
     * Supports Markdown formatting.
     */
    QString documentation;
    
    /**
     * @brief Sorting priority (lower = higher priority)
     * 
     * Items are sorted by priority first, then alphabetically.
     */
    int priority = 100;
    
    /**
     * @brief Whether insertText contains snippet placeholders
     */
    bool isSnippet = false;
    
    /**
     * @brief Custom sort key
     * 
     * If empty, label is used for sorting.
     */
    QString sortText;
    
    /**
     * @brief Optional custom icon
     * 
     * If null, a default icon based on kind is used.
     */
    QIcon icon;
    
    /**
     * @brief Provider ID that created this item
     * 
     * Used for debugging and filtering.
     */
    QString providerId;
    
    /**
     * @brief Get effective filter text
     * @return filterText if not empty, otherwise label
     */
    QString effectiveFilterText() const {
        return filterText.isEmpty() ? label : filterText;
    }
    
    /**
     * @brief Get effective sort text
     * @return sortText if not empty, otherwise label
     */
    QString effectiveSortText() const {
        return sortText.isEmpty() ? label : sortText;
    }
    
    /**
     * @brief Get effective insert text
     * @return insertText if not empty, otherwise label
     */
    QString effectiveInsertText() const {
        return insertText.isEmpty() ? label : insertText;
    }
    
    /**
     * @brief Compare items for sorting
     * 
     * Sorts by priority first, then by effective sort text.
     */
    bool operator<(const CompletionItem& other) const {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return effectiveSortText().compare(other.effectiveSortText(), Qt::CaseInsensitive) < 0;
    }
    
    /**
     * @brief Check equality based on label and provider
     */
    bool operator==(const CompletionItem& other) const {
        return label == other.label && providerId == other.providerId;
    }
};

#endif // COMPLETIONITEM_H
