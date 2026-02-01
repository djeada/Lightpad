#ifndef VIMMODE_H
#define VIMMODE_H

#include <QObject>
#include <QString>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QMap>

/**
 * @brief VIM editing modes
 */
enum class VimEditMode {
    Normal,    // Default mode for navigation
    Insert,    // Text insertion mode
    Visual,    // Selection mode (character)
    VisualLine,// Selection mode (line)
    VisualBlock,// Selection mode (block)
    Command,   // Command-line mode (:)
    Replace    // Replace mode (R)
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
    HalfPageUp,     // Ctrl+u (half page)
    HalfPageDown,   // Ctrl+d (half page)
    FullPageUp,     // Ctrl+b
    FullPageDown,   // Ctrl+f
    MatchingBrace,  // %
    FindChar,       // f{char}
    FindCharBack,   // F{char}
    ToChar,         // t{char}
    ToCharBack,     // T{char}
    NextParagraph,  // }
    PrevParagraph,  // {
    NextSentence,   // )
    PrevSentence,   // (
    SearchNext,     // n
    SearchPrev,     // N
    WordUnderCursor,// *
    WordUnderCursorBack // #
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
    Format,     // =
    ToggleCase  // ~ (or g~)
};

/**
 * @brief VIM text object types
 */
enum class VimTextObject {
    None,
    InnerWord,      // iw
    AroundWord,     // aw
    InnerWORD,      // iW
    AroundWORD,     // aW
    InnerParen,     // i(, i)
    AroundParen,    // a(, a)
    InnerBracket,   // i[, i]
    AroundBracket,  // a[, a]
    InnerBrace,     // i{, i}
    AroundBrace,    // a{, a}
    InnerAngle,     // i<, i>
    AroundAngle,    // a<, a>
    InnerQuote,     // i"
    AroundQuote,    // a"
    InnerSingleQuote,// i'
    AroundSingleQuote,// a'
    InnerBacktick,  // i`
    AroundBacktick  // a`
};

/**
 * @brief VIM mode handler for text editors
 * 
 * Provides VIM-style modal editing with support for:
 * - Normal, Insert, Visual, Replace modes
 * - Basic motions (h, j, k, l, w, b, e, etc.)
 * - Advanced motions (f, t, %, *, #, n, N, {, }, etc.)
 * - Basic operators (d, c, y, >, <, ~)
 * - Text objects (iw, aw, i", a", i(, a(, etc.)
 * - Repeat command (.)
 * - Marks (m{a-z}, '{a-z})
 * - Basic commands (:w, :q, :wq, /search)
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

    /**
     * @brief Emitted when command buffer changes in command mode
     * @param buffer The current command buffer
     */
    void commandBufferChanged(const QString& buffer);

private:
    bool handleNormalMode(QKeyEvent* event);
    bool handleInsertMode(QKeyEvent* event);
    bool handleVisualMode(QKeyEvent* event);
    bool handleCommandMode(QKeyEvent* event);
    bool handleReplaceMode(QKeyEvent* event);

    void setMode(VimEditMode mode);
    void executeMotion(VimMotion motion, int count = 1);
    void executeOperator(VimOperator op, VimMotion motion, int count = 1);
    void executeOperatorOnTextObject(VimOperator op, VimTextObject textObj);
    void executeCommand(const QString& command);
    
    void moveCursor(QTextCursor::MoveOperation op, int count = 1);
    void moveCursorWord(bool forward);
    void moveCursorToChar(QChar ch, bool before, bool backward = false);
    bool moveCursorToMatchingBrace();
    void moveCursorToParagraph(bool forward);
    void moveCursorToSentence(bool forward);
    
    void deleteText(VimMotion motion, int count = 1);
    void yankText(VimMotion motion, int count = 1);
    void changeText(VimMotion motion, int count = 1);
    void indentText(VimMotion motion, int count = 1, bool indent = true);
    void toggleCase(VimMotion motion, int count = 1);
    
    bool selectTextObject(VimTextObject textObj);
    
    void insertNewLine(bool above);
    void joinLines();
    void replaceChar(QChar ch);
    
    void setMark(QChar mark);
    bool jumpToMark(QChar mark);
    
    void repeatLastChange();
    void recordChange(const QString& change);
    
    void searchWord(bool forward);
    void searchNext(bool forward);
    void scrollLines(int lines);

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
    
    // Search state
    QString m_searchPattern;
    bool m_searchForward;
    
    // Marks storage (a-z, A-Z)
    QMap<QChar, int> m_marks;
    
    // Repeat command state
    QString m_lastChange;
    int m_lastChangeCount;
    bool m_recordingChange;
};

#endif // VIMMODE_H
