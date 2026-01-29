#ifndef VIMMODE_H
#define VIMMODE_H

#include <QObject>
#include <QString>
#include <QKeyEvent>
#include <QPlainTextEdit>

/**
 * @brief VIM editing modes
 */
enum class VimEditMode {
    Normal,    // Default mode for navigation
    Insert,    // Text insertion mode
    Visual,    // Selection mode (character)
    VisualLine,// Selection mode (line)
    VisualBlock,// Selection mode (block)
    Command    // Command-line mode (:)
};

/**
 * @brief VIM motion types
 */
enum class VimMotion {
    None,
    Left,           // h
    Right,          // l
    Up,             // k
    Down,           // j
    WordForward,    // w
    WordBack,       // b
    WordEnd,        // e
    LineStart,      // 0
    LineEnd,        // $
    FirstNonSpace,  // ^
    FileStart,      // gg
    FileEnd,        // G
    PageUp,         // Ctrl+u
    PageDown,       // Ctrl+d
    MatchingBrace,  // %
    FindChar,       // f{char}
    FindCharBack,   // F{char}
    ToChar,         // t{char}
    ToCharBack      // T{char}
};

/**
 * @brief VIM operation types
 */
enum class VimOperator {
    None,
    Delete,     // d
    Change,     // c
    Yank,       // y
    Indent,     // >
    Unindent,   // <
    Format      // =
};

/**
 * @brief VIM mode handler for text editors
 * 
 * Provides VIM-style modal editing with support for:
 * - Normal, Insert, Visual modes
 * - Basic motions (h, j, k, l, w, b, e, etc.)
 * - Basic operators (d, c, y)
 * - Basic commands (:w, :q, :wq)
 */
class VimMode : public QObject {
    Q_OBJECT

public:
    explicit VimMode(QPlainTextEdit* editor, QObject* parent = nullptr);
    ~VimMode() = default;

    /**
     * @brief Enable or disable VIM mode
     * @param enabled Whether to enable VIM mode
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if VIM mode is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

    /**
     * @brief Get current edit mode
     * @return Current VIM mode
     */
    VimEditMode mode() const;

    /**
     * @brief Get mode name for display
     * @return Mode name string
     */
    QString modeName() const;

    /**
     * @brief Get current command buffer
     * @return Pending command string
     */
    QString commandBuffer() const;

    /**
     * @brief Process a key event
     * @param event Key event to process
     * @return true if event was handled
     */
    bool processKeyEvent(QKeyEvent* event);

signals:
    /**
     * @brief Emitted when mode changes
     * @param mode New mode
     */
    void modeChanged(VimEditMode mode);

    /**
     * @brief Emitted for status messages
     * @param message Status message
     */
    void statusMessage(const QString& message);

    /**
     * @brief Emitted when a command is executed
     * @param command The executed command
     */
    void commandExecuted(const QString& command);

private:
    bool handleNormalMode(QKeyEvent* event);
    bool handleInsertMode(QKeyEvent* event);
    bool handleVisualMode(QKeyEvent* event);
    bool handleCommandMode(QKeyEvent* event);

    void setMode(VimEditMode mode);
    void executeMotion(VimMotion motion, int count = 1);
    void executeOperator(VimOperator op, VimMotion motion, int count = 1);
    void executeCommand(const QString& command);
    
    void moveCursor(QTextCursor::MoveOperation op, int count = 1);
    void moveCursorWord(bool forward);
    void moveCursorToChar(QChar ch, bool before, bool backward = false);
    
    void deleteText(VimMotion motion, int count = 1);
    void yankText(VimMotion motion, int count = 1);
    void changeText(VimMotion motion, int count = 1);
    
    void insertNewLine(bool above);
    void joinLines();
    void replaceChar(QChar ch);

    QPlainTextEdit* m_editor;
    bool m_enabled;
    VimEditMode m_mode;
    QString m_commandBuffer;
    QString m_registerBuffer;  // Yank/delete register
    VimOperator m_pendingOperator;
    int m_count;
    QChar m_findChar;
    bool m_findCharBefore;
    bool m_findCharBackward;
};

#endif // VIMMODE_H
