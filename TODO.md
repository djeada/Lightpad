# Lightpad IDE - Missing Features for Full-Fledged Modern IDE

## Current State Summary
Lightpad has: Vim mode, basic LSP (completion/hover/definition/references/diagnostics), syntax highlighting (C++/JS/Python), file tree, plugin system, themes, terminal, find/replace, run/format templates, accessibility support, **multi-cursor editing**, **command palette**, **code folding**, **problems panel**.

---

## ✅ RECENTLY IMPLEMENTED (Quick Wins)

### Multi-Cursor Editing
- [x] Add cursor above/below (Ctrl+Alt+Up/Down)
- [x] Add cursor at next occurrence (Ctrl+D)
- [x] Add cursors to all occurrences (Ctrl+Shift+L)
- [x] Multi-cursor typing and deletion
- [x] Escape to clear extra cursors
- [x] Column/box selection (Alt+Shift+Drag)
- [ ] Multi-cursor undo/redo
- [x] Split selection into lines (Ctrl+Shift+I)

### Command Palette
- [x] Fuzzy command search (Ctrl+Shift+P)
- [x] Auto-register commands from menus
- [x] Keyboard shortcut display
- [x] Recent commands
- [x] File quick open (Ctrl+P)
- [x] Go to symbol (Ctrl+Shift+O)
- [x] Go to line (Ctrl+G)

### Code Folding
- [x] Fold/unfold current block (Ctrl+Shift+[/])
- [x] Fold all / Unfold all
- [x] Brace-based folding ({})
- [x] Indent-based folding (Python)
- [x] Fold level (fold to level 1, 2, 3...)
- [ ] Folding indicators in gutter (click to fold)
- [ ] Custom fold regions (#region/#endregion)
- [ ] Fold comments
- [ ] Persistent fold state

### Problems Panel
- [x] Problems panel with tree view (Ctrl+Shift+M)
- [x] Filter by severity (Errors, Warnings, Info)
- [x] Click to navigate to problem
- [x] File grouping
- [x] Color-coded by severity
- [x] Problem count in status bar
- [x] Problem count per file in tree
- [x] Auto-refresh on save

---

## 🔴 CRITICAL MISSING FEATURES

### 1. Debugger Integration
- [x] Debug Adapter Protocol (DAP) client implementation
- [x] Breakpoint system (set, toggle, conditional, logpoints)
- [ ] Breakpoint gutter in editor
- [x] Debug toolbar (Step Over, Step Into, Step Out, Continue, Pause, Stop)
- [x] Variables panel (local, watch, registers)
- [x] Call stack panel
- [x] Debug console/REPL
- [ ] Inline variable values during debug
- [x] Exception breakpoints
- [x] Multi-target debugging (DebugSessionManager)
- [x] Debug configurations (launch.json equivalent via DebugConfigurationManager)
- [x] Attach to process
- [x] Remote debugging support
- [x] Watch expressions (WatchManager)
- [x] Data breakpoints

### 2. Git/Version Control Integration
- [x] Git status in file tree (modified, staged, untracked icons)
- [ ] Git diff gutter in editor (inline +/- indicators)
- [x] Source control panel
- [x] Stage/unstage files
- [x] Commit with message
- [x] Branch management (create, checkout, delete, merge)
- [ ] Git blame annotations
- [ ] Git history/log viewer
- [ ] Conflict resolution UI
- [x] Stash management
- [x] Remote operations (push, pull, fetch)
- [ ] GitHub/GitLab integration
- [ ] Pull request viewing

### 3. Split Editor/Views
- [x] Horizontal split
- [x] Vertical split
- [ ] Grid layout (2x2, etc.)
- [ ] Drag tabs between split groups
- [ ] Synchronized scrolling option
- [x] Focus management between splits
- [ ] Open to side action

---

## 🟠 HIGH PRIORITY FEATURES

### 5. Enhanced LSP Features
- [x] Signature help (function parameter hints)
- [x] Document symbols (outline view)
- [ ] Workspace symbols search
- [x] Rename symbol (project-wide)
- [ ] Code actions (quick fixes, refactorings)
- [ ] Inlay hints (type annotations, parameter names)
- [ ] Document formatting request via LSP
- [ ] Range formatting
- [ ] Code lens
- [ ] Semantic tokens (semantic highlighting)
- [ ] Folding ranges from LSP
- [ ] Selection ranges
- [ ] Call hierarchy (incoming/outgoing calls)
- [ ] Type hierarchy
- [ ] Linked editing ranges

### 6. Code Folding
- [ ] Fold/unfold regions
- [ ] Fold all / Unfold all
- [ ] Fold level (fold to level 1, 2, 3...)
- [ ] Folding indicators in gutter
- [ ] Custom fold regions (#region/#endregion)
- [ ] Fold comments
- [ ] Fold imports
- [ ] Persistent fold state

### 7. Minimap
- [x] Scrollable minimap sidebar
- [x] Syntax highlighting in minimap
- [x] Current viewport indicator
- [x] Click to scroll
- [ ] Search results highlighting in minimap
- [ ] Git diff highlighting in minimap
- [ ] Slider for minimap zoom

### 8. Command Palette
- [ ] Fuzzy command search (Ctrl+Shift+P)
- [ ] Recent commands
- [ ] Keyboard shortcut display
- [ ] File quick open (Ctrl+P)
- [ ] Go to symbol (Ctrl+Shift+O)
- [ ] Go to line (Ctrl+G)
- [ ] Extensible command registration

### 9. File Search & Navigation
- [ ] Global file search (fuzzy)
- [ ] Project-wide text search (ripgrep integration)
- [ ] Search results panel
- [ ] Search in files with filters (include/exclude)
- [ ] Regex search with groups
- [ ] Search and replace in files
- [x] Recent files list
- [x] Breadcrumb navigation
- [ ] Go to definition in peek window
- [ ] Go back/forward navigation stack

### 10. Problems/Diagnostics Panel
- [ ] Dedicated problems panel
- [ ] Filter by severity (Error, Warning, Info, Hint)
- [ ] Filter by file/source
- [ ] Click to navigate to problem
- [ ] Problem count in status bar
- [ ] Problem count per file in tree
- [ ] Auto-refresh on save
- [ ] Clear problems action

---

## 🟡 MEDIUM PRIORITY FEATURES

### 11. Workspace/Project Management
- [ ] Workspace files (.lightpad-workspace)
- [ ] Multi-root workspaces
- [ ] Project-specific settings
- [ ] Workspace trust
- [ ] Recent workspaces
- [ ] Workspace switching

### 12. Extensions/Plugins Marketplace
- [ ] Plugin repository/registry
- [ ] Search & browse plugins
- [ ] Install/uninstall from UI
- [ ] Plugin updates
- [ ] Plugin ratings/reviews
- [ ] Plugin dependencies resolution
- [ ] Plugin settings UI

### 13. Integrated Terminal Improvements
- [ ] Multiple terminal instances
- [ ] Split terminals
- [ ] Terminal tabs
- [ ] Different shell profiles
- [ ] Terminal links detection
- [ ] Copy/paste improvements
- [ ] Scrollback buffer configuration
- [ ] Terminal theming
- [ ] Send text to terminal
- [ ] Task integration

### 14. Snippets System
- [ ] User-defined snippets
- [ ] Language-specific snippets
- [ ] Snippet placeholders with tabstops
- [ ] Variable substitution ($1, $2, ${name})
- [ ] Choice placeholders
- [ ] Snippet transformation
- [ ] Snippet importer (from VS Code)

### 15. Code Actions & Refactoring
- [ ] Extract method/function
- [ ] Extract variable
- [ ] Extract constant
- [ ] Inline variable
- [ ] Move to new file
- [ ] Generate getters/setters
- [ ] Implement interface
- [ ] Add missing imports
- [ ] Organize imports
- [ ] Remove unused imports
- [ ] Quick fix suggestions

### 16. Code Intelligence
- [ ] Type information on hover (richer)
- [ ] Documentation preview
- [ ] Parameter info popup
- [ ] Quick documentation (F1)
- [ ] Expression evaluation
- [ ] Semantic highlighting beyond syntax
- [ ] Find all usages
- [ ] Peek definition
- [ ] Peek references

### 17. Language Support Expansion
- [ ] TypeScript
- [ ] Rust
- [ ] Go
- [ ] Java
- [ ] C#
- [ ] Ruby
- [ ] PHP
- [ ] Swift
- [ ] Kotlin
- [ ] HTML/CSS
- [ ] JSON/YAML/TOML
- [ ] Markdown preview
- [ ] SQL
- [ ] Shell scripts
- [ ] Dockerfile
- [ ] Language auto-detection

---

## 🟢 NICE-TO-HAVE FEATURES

### 18. Editor Enhancements
- [ ] Soft wrap toggle
- [ ] Word wrap at column
- [x] Show whitespace characters
- [ ] Show indent guides
- [ ] Rainbow brackets
- [ ] Bracket pair colorization
- [ ] Sticky scroll (sticky headers)
- [ ] Smooth scrolling
- [ ] Cursor smooth caret animation
- [ ] Column ruler(s)
- [ ] Zen mode (distraction-free)
- [ ] Diff editor (side-by-side comparison)
- [ ] Inline diff view
- [ ] Read-only mode
- [ ] Linked files (follow symlinks)

### 19. Search Improvements
- [ ] Search history
- [ ] Search scopes (selection, file, folder, project)
- [ ] Preserve case in replace
- [ ] Regular expression builder helper
- [ ] Multi-line search
- [ ] Search exclusions (.gitignore aware)

### 20. Session Management
- [ ] Restore open tabs on startup
- [ ] Session save/load
- [ ] Multiple sessions
- [ ] Crash recovery (autosave)
- [ ] File change watcher (external modifications)
- [ ] Hot reload files

### 21. Productivity Features
- [ ] Auto-save
- [ ] Format on save
- [ ] Format on paste
- [ ] Trim trailing whitespace
- [ ] Insert final newline
- [ ] Emmet support (HTML/CSS)
- [ ] Lorem ipsum generator
- [ ] Multiple clipboards/clipboard history
- [ ] Compare with clipboard
- [ ] Sort lines
- [ ] Transform text (uppercase, lowercase, title case)
- [ ] Join lines
- [ ] Comment toggling improvements

### 22. UI/UX Improvements
- [ ] Customizable toolbar
- [ ] Customizable keyboard shortcuts UI
- [ ] Activity bar (vertical sidebar)
- [ ] Sidebar toggle
- [ ] Panel toggle
- [ ] Status bar customization
- [ ] Icon themes
- [ ] File icons in tabs and tree
- [ ] Tab reordering via drag
- [ ] Tab pinning
- [ ] Tab preview (single-click vs double-click)
- [ ] Notifications system
- [ ] Progress indicators
- [ ] Command history

### 23. Remote Development
- [ ] SSH remote editing
- [ ] Docker container editing
- [ ] WSL integration
- [ ] Remote file browser
- [ ] Remote terminal

### 24. Collaboration
- [ ] Live share / pair programming
- [ ] Shared cursors
- [ ] Chat integration
- [ ] Code review tools

### 25. AI/ML Features
- [ ] AI code completion (Copilot-style)
- [ ] Code explanation
- [ ] Generate code from comments
- [ ] Chat with codebase
- [ ] Intelligent refactoring suggestions

### 26. Build System Integration
- [ ] Task runner
- [ ] Build tasks configuration
- [ ] Problem matchers (parse build output)
- [ ] Pre/post build hooks
- [ ] CMake integration
- [ ] Make integration
- [ ] npm/yarn scripts integration

### 27. Testing Integration
- [ ] Test explorer panel
- [ ] Run tests from editor
- [ ] Test coverage visualization
- [ ] Test debugging
- [ ] Test result history
- [ ] Re-run failed tests

### 28. Documentation
- [ ] In-editor documentation browser
- [ ] Quick docs lookup
- [ ] External docs linking
- [ ] API reference integration

---

## 📊 Priority Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| Debugger | Very High | High | P0 |
| Git Integration | Very High | High | P0 |
| Multi-Cursor | High | Medium | P0 |
| Split Views | High | Medium | P0 |
| Enhanced LSP | High | Medium | P1 |
| Command Palette | High | Low | P1 |
| Code Folding | Medium | Low | P1 |
| Minimap | Medium | Medium | P1 |
| File Search | High | Medium | P1 |
| Problems Panel | Medium | Low | P1 |
| Snippets | Medium | Low | P2 |
| Refactoring | High | High | P2 |
| More Languages | Medium | Medium | P2 |
| Session Restore | Medium | Low | P2 |
| Auto-save | Low | Low | P3 |
| Remote Dev | High | Very High | P3 |
| AI Features | High | Very High | P3 |
| Live Share | Medium | Very High | P4 |

---

## Implementation Order Recommendation

### Phase 1: Core IDE (P0) ⏱️
1. Multi-cursor editing
2. Split views
3. Git integration (basic: status, diff gutter, commit)
4. DAP debugger (basic: breakpoints, step, variables)

### Phase 2: Enhanced Experience (P1)
5. Command palette with fuzzy search
6. Code folding
7. Problems panel
8. Enhanced LSP (rename, code actions, signature help)
9. Project-wide search
10. Minimap

### Phase 3: Polish & Productivity (P2)
11. Snippets system
12. More language support
13. Refactoring tools
14. Session management
15. Plugin marketplace

### Phase 4: Advanced (P3+)
16. Testing integration
17. Build system integration
18. Remote development
19. AI features
20. Collaboration tools

---

## Notes

**Already Well Implemented:**
- ✅ VIM mode (comprehensive)
- ✅ Basic LSP (completion, hover, go-to-def, references, diagnostics)
- ✅ Syntax highlighting (extensible)
- ✅ File tree (full CRUD operations)
- ✅ Plugin architecture
- ✅ Theme system
- ✅ Terminal integration
- ✅ Find/Replace
- ✅ Run/Format templates
- ✅ Accessibility features
- ✅ Async worker infrastructure

**Tech Debt to Address:**
- [ ] Save preferences in home directory (from README TODO)
- [ ] Switch to QProcess for multiprocessing (from README TODO)
- [ ] Custom highlighting rules UI (from README TODO)
- [ ] LSP not integrated into UI (just client exists)
- [ ] Formatter is placeholder stub
