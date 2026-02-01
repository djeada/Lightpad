#ifndef CODEFOLDING_H
#define CODEFOLDING_H

#include <QSet>
#include <QString>

class QTextDocument;
class QTextBlock;

/**
 * @brief Manages code folding state and operations
 * 
 * Handles fold/unfold operations, tracking folded blocks,
 * and determining foldable regions based on syntax.
 */
class CodeFoldingManager {
public:
    explicit CodeFoldingManager(QTextDocument* document);
    
    /**
     * @brief Get the set of currently folded block numbers
     */
    const QSet<int>& foldedBlocks() const { return m_foldedBlocks; }
    
    /**
     * @brief Check if a block is currently folded
     */
    bool isFolded(int blockNumber) const;
    
    /**
     * @brief Check if a block is foldable (can be folded)
     */
    bool isFoldable(int blockNumber) const;
    
    /**
     * @brief Get the folding level of a block
     */
    int getFoldingLevel(int blockNumber) const;
    
    /**
     * @brief Find the end block of a fold region starting at startBlock
     */
    int findFoldEndBlock(int startBlock) const;
    
    /**
     * @brief Fold a specific block
     * @return true if the block was folded
     */
    bool foldBlock(int blockNumber);
    
    /**
     * @brief Unfold a specific block
     * @return true if the block was unfolded
     */
    bool unfoldBlock(int blockNumber);
    
    /**
     * @brief Toggle fold state at a line
     */
    void toggleFoldAtLine(int line);
    
    /**
     * @brief Fold all foldable blocks
     */
    void foldAll();
    
    /**
     * @brief Unfold all blocks
     */
    void unfoldAll();
    
    /**
     * @brief Fold all blocks at or above the specified level
     */
    void foldToLevel(int level);
    
    /**
     * @brief Fold all comment blocks
     */
    void foldComments();
    
    /**
     * @brief Unfold all comment blocks
     */
    void unfoldComments();
    
    // Region detection
    bool isRegionStart(int blockNumber) const;
    bool isRegionEnd(int blockNumber) const;
    int findRegionEndBlock(int startBlock) const;
    
    // Comment block detection
    bool isCommentBlockStart(int blockNumber) const;
    int findCommentBlockEnd(int startBlock) const;

private:
    static bool isSingleLineComment(const QString& trimmedText);
    
    QTextDocument* m_document;
    QSet<int> m_foldedBlocks;
};

#endif // CODEFOLDING_H
