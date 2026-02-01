#include "codefolding.h"
#include <QTextDocument>
#include <QTextBlock>
#include <QStringList>

CodeFoldingManager::CodeFoldingManager(QTextDocument* document)
    : m_document(document)
{
}

bool CodeFoldingManager::isFolded(int blockNumber) const
{
    return m_foldedBlocks.contains(blockNumber);
}

bool CodeFoldingManager::isSingleLineComment(const QString& trimmedText)
{
    if (trimmedText.startsWith("//")) {
        return true;
    }
    if (trimmedText.startsWith("#") && 
        !trimmedText.startsWith("#include") && 
        !trimmedText.startsWith("#define") && 
        !trimmedText.startsWith("#pragma") &&
        !trimmedText.startsWith("#if") && 
        !trimmedText.startsWith("#else") &&
        !trimmedText.startsWith("#endif") && 
        !trimmedText.startsWith("#region") &&
        !trimmedText.startsWith("#endregion")) {
        return true;
    }
    return false;
}

bool CodeFoldingManager::isFoldable(int blockNumber) const
{
    if (!m_document) return false;
    
    QTextBlock block = m_document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text();
    QString trimmed = text.trimmed();
    
    // Check for #region marker
    if (isRegionStart(blockNumber))
        return true;
    
    // Check for multi-line comment block start
    if (isCommentBlockStart(blockNumber))
        return true;
    
    // Foldable if line ends with { or :
    if (trimmed.endsWith('{') || trimmed.endsWith(':'))
        return true;
    
    // Or if next line has greater indent
    QTextBlock nextBlock = block.next();
    if (nextBlock.isValid()) {
        QString nextText = nextBlock.text();
        
        int currentIndent = 0;
        for (QChar c : text) {
            if (c == ' ') currentIndent++;
            else if (c == '\t') currentIndent += 4;
            else break;
        }
        
        int nextIndent = 0;
        for (QChar c : nextText) {
            if (c == ' ') nextIndent++;
            else if (c == '\t') nextIndent += 4;
            else break;
        }
        
        if (nextIndent > currentIndent && !nextText.trimmed().isEmpty())
            return true;
    }
    
    return false;
}

int CodeFoldingManager::getFoldingLevel(int blockNumber) const
{
    if (!m_document) return 0;
    
    QTextBlock block = m_document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return 0;
    
    QString text = block.text();
    int indent = 0;
    
    for (QChar c : text) {
        if (c == ' ') indent++;
        else if (c == '\t') indent += 4;
        else break;
    }
    
    // Count brace nesting level
    int braceLevel = 0;
    QTextBlock prevBlock = m_document->begin();
    while (prevBlock.isValid() && prevBlock.blockNumber() < blockNumber) {
        QString prevText = prevBlock.text();
        for (QChar c : prevText) {
            if (c == '{') braceLevel++;
            else if (c == '}') braceLevel--;
        }
        prevBlock = prevBlock.next();
    }
    
    int indentLevel = indent / 4;
    return qMax(indentLevel, braceLevel);
}

int CodeFoldingManager::findFoldEndBlock(int startBlock) const
{
    if (!m_document) return startBlock;
    
    QTextBlock block = m_document->findBlockByNumber(startBlock);
    if (!block.isValid())
        return startBlock;
    
    QString text = block.text();
    
    // Check for #region marker
    if (isRegionStart(startBlock)) {
        return findRegionEndBlock(startBlock);
    }
    
    // Check for comment block start
    if (isCommentBlockStart(startBlock)) {
        return findCommentBlockEnd(startBlock);
    }
    
    int startIndent = 0;
    for (QChar c : text) {
        if (c == ' ') startIndent++;
        else if (c == '\t') startIndent += 4;
        else break;
    }
    
    // Check for brace-based folding
    bool braceStyle = text.contains('{');
    int braceCount = 0;
    
    if (braceStyle) {
        for (QChar c : text) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
        }
    }
    
    block = block.next();
    int lastNonEmpty = startBlock;
    
    while (block.isValid()) {
        text = block.text().trimmed();
        
        if (braceStyle) {
            for (QChar c : block.text()) {
                if (c == '{') braceCount++;
                else if (c == '}') braceCount--;
            }
            
            if (braceCount <= 0) {
                return block.blockNumber();
            }
        } else {
            // Indent-based folding
            if (!text.isEmpty()) {
                int indent = 0;
                for (QChar c : block.text()) {
                    if (c == ' ') indent++;
                    else if (c == '\t') indent += 4;
                    else break;
                }
                
                if (indent <= startIndent) {
                    return lastNonEmpty;
                }
                lastNonEmpty = block.blockNumber();
            }
        }
        
        block = block.next();
    }
    
    return lastNonEmpty;
}

bool CodeFoldingManager::foldBlock(int blockNumber)
{
    if (!m_document) return false;
    
    if (isFoldable(blockNumber) && !m_foldedBlocks.contains(blockNumber)) {
        m_foldedBlocks.insert(blockNumber);
        
        int endBlock = findFoldEndBlock(blockNumber);
        QTextBlock block = m_document->findBlockByNumber(blockNumber + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(false);
            block = block.next();
        }
        
        return true;
    }
    return false;
}

bool CodeFoldingManager::unfoldBlock(int blockNumber)
{
    if (!m_document) return false;
    
    // Check if we're inside a folded region
    for (int foldedBlock : m_foldedBlocks) {
        int endBlock = findFoldEndBlock(foldedBlock);
        if (blockNumber >= foldedBlock && blockNumber <= endBlock) {
            blockNumber = foldedBlock;
            break;
        }
    }
    
    if (m_foldedBlocks.contains(blockNumber)) {
        m_foldedBlocks.remove(blockNumber);
        
        int endBlock = findFoldEndBlock(blockNumber);
        QTextBlock block = m_document->findBlockByNumber(blockNumber + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
        
        return true;
    }
    return false;
}

void CodeFoldingManager::toggleFoldAtLine(int line)
{
    if (!m_document) return;
    
    if (m_foldedBlocks.contains(line)) {
        m_foldedBlocks.remove(line);
        
        int endBlock = findFoldEndBlock(line);
        QTextBlock block = m_document->findBlockByNumber(line + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
    } else if (isFoldable(line)) {
        m_foldedBlocks.insert(line);
        
        int endBlock = findFoldEndBlock(line);
        QTextBlock block = m_document->findBlockByNumber(line + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(false);
            block = block.next();
        }
    }
}

void CodeFoldingManager::foldAll()
{
    if (!m_document) return;
    
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        if (isFoldable(block.blockNumber())) {
            int blockNum = block.blockNumber();
            if (!m_foldedBlocks.contains(blockNum)) {
                m_foldedBlocks.insert(blockNum);
                
                int endBlock = findFoldEndBlock(blockNum);
                QTextBlock innerBlock = block.next();
                
                while (innerBlock.isValid() && innerBlock.blockNumber() <= endBlock) {
                    innerBlock.setVisible(false);
                    innerBlock = innerBlock.next();
                }
            }
        }
        block = block.next();
    }
}

void CodeFoldingManager::unfoldAll()
{
    if (!m_document) return;
    
    m_foldedBlocks.clear();
    
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        block.setVisible(true);
        block = block.next();
    }
}

void CodeFoldingManager::foldToLevel(int level)
{
    if (!m_document) return;
    
    unfoldAll();
    
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        if (isFoldable(blockNum)) {
            int blockLevel = getFoldingLevel(blockNum);
            
            if (blockLevel >= level) {
                m_foldedBlocks.insert(blockNum);
                
                int endBlock = findFoldEndBlock(blockNum);
                QTextBlock innerBlock = block.next();
                
                while (innerBlock.isValid() && innerBlock.blockNumber() <= endBlock) {
                    innerBlock.setVisible(false);
                    innerBlock = innerBlock.next();
                }
            }
        }
        block = block.next();
    }
}

bool CodeFoldingManager::isRegionStart(int blockNumber) const
{
    if (!m_document) return false;
    
    QTextBlock block = m_document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    static const QStringList regionPatterns = {
        "#region", "// region", "//region", "//#region", "// #region",
        "/* region", "/*region", "/* #region", "/*#region",
        "# region", "#pragma region"
    };
    
    QString lowerText = text.toLower();
    for (const QString& pattern : regionPatterns) {
        if (lowerText.startsWith(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool CodeFoldingManager::isRegionEnd(int blockNumber) const
{
    if (!m_document) return false;
    
    QTextBlock block = m_document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    static const QStringList endregionPatterns = {
        "#endregion", "// endregion", "//endregion", "//#endregion", "// #endregion",
        "/* endregion", "/*endregion", "/* #endregion", "/*#endregion",
        "# endregion", "#pragma endregion"
    };
    
    QString lowerText = text.toLower();
    for (const QString& pattern : endregionPatterns) {
        if (lowerText.startsWith(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

int CodeFoldingManager::findRegionEndBlock(int startBlock) const
{
    if (!m_document) return startBlock;
    
    int depth = 1;
    QTextBlock block = m_document->findBlockByNumber(startBlock + 1);
    
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        
        if (isRegionStart(blockNum)) {
            depth++;
        } else if (isRegionEnd(blockNum)) {
            depth--;
            if (depth == 0) {
                return blockNum;
            }
        }
        
        block = block.next();
    }
    
    return m_document->blockCount() - 1;
}

bool CodeFoldingManager::isCommentBlockStart(int blockNumber) const
{
    if (!m_document) return false;
    
    QTextBlock block = m_document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    
    QString text = block.text().trimmed();
    
    // Check for C-style block comment start
    if (text.startsWith("/*") && !text.contains("*/")) {
        return true;
    }
    
    // Check for consecutive single-line comments (3 or more lines)
    if (isSingleLineComment(text)) {
        QTextBlock prevBlock = block.previous();
        if (prevBlock.isValid()) {
            QString prevText = prevBlock.text().trimmed();
            if (isSingleLineComment(prevText)) {
                return false;
            }
        }
        
        int consecutiveComments = 1;
        QTextBlock nextBlock = block.next();
        while (nextBlock.isValid() && consecutiveComments < 3) {
            QString nextText = nextBlock.text().trimmed();
            if (isSingleLineComment(nextText)) {
                consecutiveComments++;
                nextBlock = nextBlock.next();
            } else {
                break;
            }
        }
        
        return consecutiveComments >= 3;
    }
    
    return false;
}

int CodeFoldingManager::findCommentBlockEnd(int startBlock) const
{
    if (!m_document) return startBlock;
    
    QTextBlock block = m_document->findBlockByNumber(startBlock);
    if (!block.isValid())
        return startBlock;
    
    QString text = block.text().trimmed();
    
    // Handle C-style block comments
    if (text.startsWith("/*")) {
        QTextBlock searchBlock = block;
        while (searchBlock.isValid()) {
            if (searchBlock.text().contains("*/")) {
                return searchBlock.blockNumber();
            }
            searchBlock = searchBlock.next();
        }
        return m_document->blockCount() - 1;
    }
    
    // Handle consecutive single-line comments
    int lastCommentBlock = startBlock;
    QTextBlock nextBlock = block.next();
    
    while (nextBlock.isValid()) {
        QString nextText = nextBlock.text().trimmed();
        
        if (isSingleLineComment(nextText)) {
            lastCommentBlock = nextBlock.blockNumber();
            nextBlock = nextBlock.next();
        } else {
            break;
        }
    }
    
    return lastCommentBlock;
}

void CodeFoldingManager::foldComments()
{
    if (!m_document) return;
    
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        int blockNum = block.blockNumber();
        if (isCommentBlockStart(blockNum) && !m_foldedBlocks.contains(blockNum)) {
            m_foldedBlocks.insert(blockNum);
            
            int endBlock = findCommentBlockEnd(blockNum);
            QTextBlock innerBlock = block.next();
            
            while (innerBlock.isValid() && innerBlock.blockNumber() <= endBlock) {
                innerBlock.setVisible(false);
                innerBlock = innerBlock.next();
            }
        }
        block = block.next();
    }
}

void CodeFoldingManager::unfoldComments()
{
    if (!m_document) return;
    
    QList<int> commentBlocks;
    for (int blockNum : m_foldedBlocks) {
        if (isCommentBlockStart(blockNum)) {
            commentBlocks.append(blockNum);
        }
    }
    
    for (int blockNum : commentBlocks) {
        m_foldedBlocks.remove(blockNum);
        
        int endBlock = findCommentBlockEnd(blockNum);
        QTextBlock block = m_document->findBlockByNumber(blockNum + 1);
        
        while (block.isValid() && block.blockNumber() <= endBlock) {
            block.setVisible(true);
            block = block.next();
        }
    }
}
