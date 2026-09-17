#ifndef VIMMODE_H
#define VIMMODE_H

#include <QKeyEvent>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextCursor>
#include <QVector>
#include <memory>

enum class VimEditMode {
  Normal,
  Insert,
  Visual,
  VisualLine,
  VisualBlock,
  Command,
  Replace
};

enum class VimRegisterType { Charwise, Linewise, Blockwise };

struct VimRegister {
  QString content;
  bool linewise = false;
  bool blockwise = false;
};

class VimMode : public QObject {
  Q_OBJECT

public:
  explicit VimMode(QPlainTextEdit *editor, QObject *parent = nullptr);
  ~VimMode();

  void setEnabled(bool enabled);
  bool isEnabled() const;

  VimEditMode mode() const;
  QString modeName() const;

  QString commandBuffer() const;
  QString commandText() const;
  QChar commandType() const;
  int commandCursorPosition() const;
  void setCommandText(const QString &text);

  QString pendingKeys() const;

  bool isRecordingMacro() const;
  QChar macroRegister() const;

  QString registerContent(QChar reg) const;
  VimRegister registerValue(QChar reg) const;

  QString searchPattern() const;
  void setSearchPattern(const QString &pattern);

  static QString escapePattern(const QString &literal);
  static QString toRegularExpression(const QString &vimPattern,
                                     bool *caseSensitive = nullptr,
                                     bool ignoreCase = false,
                                     bool smartCase = false);

  void setTabWidth(int width);
  void setAutoIndent(bool enabled);

  bool processKeyEvent(QKeyEvent *event);
  bool shouldOverrideShortcut(QKeyEvent *event) const;

  void feedKeys(const QString &keys);

  QVector<QPair<int, int>> visualBlockRanges() const;

  static QString keyEventToToken(QKeyEvent *event);
  static QStringList parseKeyNotation(const QString &keys);
  static QString tokensToNotation(const QStringList &tokens);
  static std::unique_ptr<QKeyEvent> tokenToKeyEvent(const QString &token);

signals:
  void modeChanged(VimEditMode mode);
  void statusMessage(const QString &message);
  void commandExecuted(const QString &command);
  void commandBufferChanged(const QString &buffer);
  void pendingKeysChanged(const QString &keys);
  void macroRecordingChanged(bool recording, QChar reg);
  void searchHighlightRequested(const QString &pattern, bool enabled);
  void registerContentsChanged();
  void visualSelectionChanged();

public:
  enum class MotionType { Exclusive, Inclusive, Linewise };

  struct Range {
    int start = 0;
    int end = 0;
    int startLine = 0;
    int endLine = 0;
    int startVcol = 0;
    int endVcol = 0;
    bool toEol = false;
    VimRegisterType type = VimRegisterType::Charwise;
  };

private:
  struct NormalCmd {
    QChar reg;
    int count = 0;
    QString op;
    QString key;
    QChar arg;
    QString textObject;
    QChar force;
    bool doubledOp = false;
  };

  enum class Parse { Incomplete, Invalid, Complete };

  struct MotionResult {
    bool ok = false;
    int pos = 0;
    MotionType type = MotionType::Exclusive;
    bool jump = false;
    bool keepWantCol = false;
    bool wantEol = false;
  };

  struct TextPos {
    int line = 0;
    int col = 0;
  };

  bool handleKey(const QString &token, QKeyEvent *event);
  bool handleNormalKey(const QString &token);
  bool handleVisualKey(const QString &token);
  bool handleInsertKey(const QString &token, QKeyEvent *event);
  bool handleReplaceKey(const QString &token, QKeyEvent *event);
  bool handleCommandKey(const QString &token);

  Parse parseCommand(const QStringList &keys, NormalCmd &cmd,
                     bool visual) const;
  Parse parseMotionKey(const QStringList &keys, int &i, QString &key,
                       QChar &arg) const;
  static bool isMotionKey(const QString &key);
  static bool isOperatorKey(const QString &key);
  static bool isTextObjectChar(const QString &key);
  static bool isMotionCommand(const QString &key);
  static bool tokenToArg(const QString &token, QChar &arg, bool allowNewline);
  QStringList visualDotPrefix() const;

  void executeNormal(const NormalCmd &cmd, const QStringList &keys);
  void executeVisual(const NormalCmd &cmd, const QStringList &keys);
  bool executeSimpleCommand(const NormalCmd &cmd, const QStringList &keys);
  void executeOperatorMotion(const NormalCmd &cmd, const QStringList &keys);

  MotionResult evalMotion(const QString &key, QChar arg, int count,
                          bool hasOperator, int fromPos, bool visual);
  bool evalTextObject(const QString &obj, int count, int &start, int &end,
                      MotionType &type, bool visual);
  Range motionRange(int a, int b, MotionType type) const;

  void applyOperator(const QString &op, const Range &range, QChar reg,
                     int count, bool fromVisual);
  void operatorDelete(const Range &range, QChar reg, bool change,
                      bool forceNumbered = false);
  void operatorYank(const Range &range, QChar reg, bool moveCursor = true);
  void operatorCase(const QString &op, const Range &range);
  void operatorShift(const Range &range, bool right, int amount);
  void operatorReindent(const Range &range);
  void operatorFormat(const Range &range, bool keepCursor);

  QString rangeText(const Range &range) const;
  Range lineRange(int firstLine, int lastLine) const;
  Range charRange(int start, int endExclusive) const;
  Range blockRange(int anchorPos, int cursorPos, bool toEol) const;
  Range visualRange() const;

  bool fwdWord(TextPos &p, int count, bool bigword, bool eol) const;
  bool endWord(TextPos &p, int count, bool bigword, bool stop,
               bool empty) const;
  bool bckWord(TextPos &p, int count, bool bigword, bool stop) const;
  bool bckendWord(TextPos &p, int count, bool bigword, bool eol) const;
  int incPos(TextPos &p) const;
  int decPos(TextPos &p) const;
  int charClass(const TextPos &p, bool bigword) const;
  QChar charAtTextPos(const TextPos &p) const;
  bool skipChars(TextPos &p, int cls, bool forward, bool bigword) const;
  const QString &cachedLine(int line) const;
  QString textBetween(int start, int end) const;
  bool inIndent(int pos) const;

  int findParagraph(int line, int count, bool forward, bool *inclusive) const;
  int findSentence(int pos, int count, bool forward) const;
  QVector<int> sentenceStarts() const;
  bool findMatchingBracket(int pos, int &result) const;
  bool findUnmatched(int pos, QChar open, QChar close, bool forward, int count,
                     int &result) const;
  bool findCharInLine(int pos, const QString &cmd, QChar ch, int count,
                      bool repeat, int &result) const;

  bool selectWordObject(int count, bool around, bool bigword, int &start,
                        int &end, MotionType &type, bool visual);
  bool selectBracketObject(QChar open, QChar close, int count, bool around,
                           int &start, int &end, MotionType &type, bool visual);
  bool selectQuoteObject(QChar quote, bool around, int &start, int &end,
                         MotionType &type);
  bool selectParagraphObject(int count, bool around, int &start, int &end,
                             MotionType &type, bool visual);
  bool selectSentenceObject(int count, bool around, int &start, int &end,
                            MotionType &type);
  bool selectTagObject(int count, bool around, int &start, int &end,
                       MotionType &type);
  bool selectSearchMatch(bool forward, int &start, int &end);

  void enterInsert(const QString &how, int count, const QStringList &keys);
  void startInsertSession(int count, const QString &kind,
                          const QStringList &dotKeys);
  void finishInsertSession();
  void dispatchToEditor(const QString &token, QKeyEvent *event);
  void insertTextAtCursor(const QString &text);
  void openLine(bool above);
  QString indentForNewLine(int line) const;

  void put(QChar reg, int count, bool after, bool moveAfter, bool adjustIndent);
  void putBlock(const VimRegister &r, int count, bool after, bool moveAfter);
  void visualPut(QChar reg, int count, bool keepRegister);
  void joinLines(int line, int count, bool insertSpace);
  void replaceChars(QChar ch, int count);
  void toggleCaseChars(int count);
  bool incrementNumber(int line, int col, int delta, int endCol,
                       int *resultPos);
  void undo(int count);
  void redo(int count);

  void startVisual(VimEditMode mode, int anchor, int pos);
  void exitVisual(bool keepCursor = true);
  void updateVisualSelection();
  void storeVisualMarks();
  void blockInsert(bool append);

  void enterCommandLine(QChar type, const QString &initial);
  void leaveCommandLine();
  void setCommandBufferInternal(const QString &text, int cursor);
  void executeCommandLine();
  void updateIncrementalSearch();

  bool search(const QString &pattern, bool forward, int count, int fromPos,
              int &matchStart, int &matchEnd, bool *wrapped,
              bool quiet = false);
  MotionResult searchMotion(bool reverse, int count, int fromPos);
  bool doSearchCommand(const QString &input, bool forward, int count,
                       MotionResult &result);
  bool prepareWordSearch(bool forward, bool wholeWord, int fromPos,
                         int &searchFrom);
  QRegularExpression compilePattern(const QString &vimPattern) const;
  void highlightSearch();
  void clearSearchHighlight();
  QString wordUnderCursor(int pos, bool keywordOnly, int *start = nullptr,
                          int *end = nullptr) const;

  bool parseExRange(const QString &cmd, int &i, int &line1, int &line2,
                    bool &hasRange, QString &error);
  bool parseExAddress(const QString &cmd, int &i, int curLine, int &line,
                      bool &found, QString &error);
  void executeEx(const QString &command);
  void exSubstitute(const QString &args, int line1, int line2, bool hasRange,
                    const QString &cmdName);
  void exGlobal(const QString &args, int line1, int line2, bool hasRange,
                bool invert);
  void exSort(const QString &args, bool reverse, int line1, int line2);
  void exNormal(const QString &args, int line1, int line2, bool hasRange);
  void exSet(const QString &args);
  void exRegisters();
  void exMarks();
  QString expandReplacement(const QString &replacement,
                            const QRegularExpressionMatch &match) const;

  void setRegister(QChar reg, const QString &text, VimRegisterType type);
  VimRegister getRegister(QChar reg) const;
  void storeDeleted(QChar reg, const QString &text, VimRegisterType type,
                    bool forceNumbered);
  void storeYanked(QChar reg, const QString &text, VimRegisterType type);
  static bool isValidRegister(QChar reg);

  void setMark(QChar mark, int pos);
  bool markPosition(QChar mark, int &pos) const;
  void pushJump(int pos);
  void jumpOlder(int count);
  void jumpNewer(int count);
  void recordChangePosition(int pos);

  void startMacroRecording(QChar reg);
  void stopMacroRecording();
  void playMacro(QChar reg, int count);
  void replayTokens(const QStringList &tokens);
  void repeatLastChange(int count);
  void setDotCommand(const QStringList &keys);

  void scrollLines(int lines, bool moveCursorWithView);
  void scrollHalfPage(bool down, int count);
  void scrollPage(bool down, int count);
  void scrollCursorTo(int where, bool firstNonBlank);
  int firstVisibleLine() const;
  int visibleLineCount() const;

  void setMode(VimEditMode mode);
  void updateCursorShape();
  void updatePendingKeys();
  void resetPending();

  QTextDocument *doc() const;
  int lineCount() const;
  QString lineText(int line) const;
  int lineLength(int line) const;
  int lineStart(int line) const;
  int lineEndPos(int line) const;
  int lineOf(int pos) const;
  int colOf(int pos) const;
  int posOf(int line, int col) const;
  int docLength() const;
  QChar charAt(int pos) const;
  int firstNonBlankCol(int line) const;
  int firstNonBlankPos(int line) const;
  int vcolOf(const QString &text, int col) const;
  int colForVcol(const QString &text, int vcol) const;

  int cursorPos() const;
  void setCursorPos(int pos, bool updateWantCol = true);
  int clampNormal(int pos) const;
  void moveToLineWithWantCol(int line);
  int posForWantCol(int line) const;
  QString indentString(int width) const;
  int indentWidth(const QString &text) const;
  void replaceRange(int start, int end, const QString &text);

  QPlainTextEdit *m_editor;
  bool m_enabled = false;
  VimEditMode m_mode = VimEditMode::Normal;

  QStringList m_pending;
  bool m_passthrough = false;
  int m_replayDepth = 0;

  int m_wantCol = 0;
  bool m_wantEol = false;

  QStringList m_dotKeys;
  int m_dotCount = 0;
  QStringList m_dotRecording;
  bool m_dotRecordingActive = false;
  int m_dotCountOverride = 0;
  bool m_inDotRepeat = false;
  bool m_opForceNumbered = false;
  bool m_insertRepeating = false;
  int m_visualSetAnchor = -1;
  int m_visualSetPos = -1;
  int m_cmdCount = 0;
  int m_opCursor = -1;
  int m_lastSyncedPos = -1;
  int m_appliedTabWidth = -1;
  int m_blockInsertStartVcol = 0;
  QMap<int, int> m_undoCursors;

  QString m_insertKind;
  int m_insertCount = 1;
  QStringList m_insertKeys;
  bool m_insertEditOpen = false;
  bool m_insertOneCommand = false;
  bool m_insertLiteralNext = false;
  bool m_insertRegisterPending = false;
  int m_insertStartPos = 0;
  QString m_lastInsertedText;
  QVector<QChar> m_replacedChars;
  bool m_blockInsertActive = false;
  bool m_blockInsertAppend = false;
  bool m_blockInsertToEol = false;
  int m_blockInsertFirstLine = 0;
  int m_blockInsertLastLine = 0;
  int m_blockInsertVcol = 0;
  int m_blockInsertLineLength = 0;

  int m_visualAnchor = 0;
  int m_visualPos = 0;
  bool m_visualToEol = false;
  VimEditMode m_lastVisualMode = VimEditMode::Visual;
  bool m_lastVisualToEol = false;

  QChar m_cmdType = ':';
  QString m_cmdText;
  int m_cmdCursor = 0;
  bool m_cmdRegisterPending = false;
  QStringList m_exHistory;
  QStringList m_searchHistory;
  int m_historyIndex = -1;
  QString m_historyDraft;
  int m_incsearchOrigin = -1;
  int m_incsearchScroll = 0;
  QStringList m_cmdOperatorKeys;
  bool m_cmdFromVisual = false;
  VimEditMode m_cmdReturnMode = VimEditMode::Normal;

  QString m_searchPattern;
  QString m_searchOffset;
  bool m_searchForward = true;
  bool m_searchHighlightActive = false;

  QString m_lastFindCmd;
  QChar m_lastFindChar;

  QMap<QChar, VimRegister> m_registers;
  QMap<QChar, QTextCursor> m_marks;
  QList<QTextCursor> m_jumpList;
  int m_jumpIndex = 0;
  QList<QTextCursor> m_changeList;
  int m_changeIndex = 0;

  bool m_macroRecording = false;
  QChar m_macroRegister;
  QStringList m_macroKeys;
  QChar m_lastMacroRegister;

  QString m_lastSubPattern;
  QString m_lastSubReplacement;
  QString m_lastSubFlags;
  bool m_hasLastSub = false;
  QString m_lastExCommand;

  int m_tabStop = 4;
  int m_shiftWidth = 4;
  bool m_expandTab = true;
  bool m_ignoreCase = false;
  bool m_smartCase = false;
  bool m_wrapScan = true;
  bool m_incSearch = true;
  bool m_hlSearch = true;
  bool m_joinSpaces = false;
  bool m_clipboardUnnamed = false;
  bool m_autoIndent = false;
  int m_textWidth = 79;
  QString m_nrFormats = "bin,hex";

  mutable const QTextDocument *m_lineCacheDoc = nullptr;
  mutable int m_lineCacheLine = -1;
  mutable int m_lineCacheRevision = -1;
  mutable int m_lineCacheCount = -1;
  mutable QString m_lineCacheText;

  static const int kMaxHistory = 50;
};

#endif
