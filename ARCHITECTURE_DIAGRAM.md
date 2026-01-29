# Architecture Diagram

## MVC Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                         LightpadTreeView                         │
│                             (View)                               │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ Responsibilities:                                          │ │
│  │ - Display file tree                                        │ │
│  │ - Handle mouse events (right-click → context menu)         │ │
│  │ - Handle drag-and-drop events                              │ │
│  │ - Forward actions to controller                            │ │
│  │ - No business logic                                        │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ forwards actions
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                      FileDirTreeController                       │
│                          (Controller)                            │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ Responsibilities:                                          │ │
│  │ - Show input dialogs (file names, etc.)                    │ │
│  │ - Show confirmation dialogs                                │ │
│  │ - Validate user input                                      │ │
│  │ - Coordinate between view and model                        │ │
│  │ - Display error messages                                   │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ calls methods
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                        FileDirTreeModel                          │
│                            (Model)                               │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ Responsibilities:                                          │ │
│  │ - All file system operations                               │ │
│  │ - Clipboard management (copy/cut/paste)                    │ │
│  │ - Name conflict resolution                                 │ │
│  │ - Error detection and reporting                            │ │
│  │ - Emit signals for updates                                 │ │
│  │ - No UI dependencies                                       │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ emits signals
                              ↓
                    ┌──────────────────┐
                    │  modelUpdated()  │
                    │  errorOccurred() │
                    └──────────────────┘
```

## Signal Flow

```
User Action Flow:
1. User right-clicks in tree view
   ↓
2. View shows context menu
   ↓
3. User selects action (e.g., "New File")
   ↓
4. View calls controller->handleNewFile()
   ↓
5. Controller shows input dialog
   ↓
6. User enters file name
   ↓
7. Controller calls model->createNewFile()
   ↓
8. Model performs file operation
   ↓
9. Model emits modelUpdated() signal
   ↓
10. Signal connected to updateModel() in view
   ↓
11. View refreshes display
```

## Component Interaction Matrix

```
                    │  View  │ Controller │  Model  │
────────────────────┼────────┼────────────┼─────────┤
View                │   -    │   calls    │  uses   │
                    │        │            │ (read)  │
────────────────────┼────────┼────────────┼─────────┤
Controller          │  owns  │     -      │  calls  │
                    │        │            │         │
────────────────────┼────────┼────────────┼─────────┤
Model               │signals │  signals   │    -    │
                    │        │            │         │
────────────────────┴────────┴────────────┴─────────┘
```

## File Operations Flow

### New File Operation
```
User → View (context menu) → Controller (input dialog) 
  → Model (create file) → Emit modelUpdated() → View (refresh)
```

### Copy-Paste Operation
```
User (right-click, Copy) → View → Controller → Model (set clipboard)
User (right-click, Paste) → View → Controller → Model (paste file)
  → Emit modelUpdated() → View (refresh)
```

### Drag-and-Drop Operation
```
User (drag) → View (dragEnterEvent)
  ↓
User (drop) → View (dropEvent) → Model (rename/copy)
  → Emit modelUpdated() → View (refresh)
```

## Key Design Patterns

### 1. Model-View-Controller (MVC)
- Separation of presentation, business logic, and user interaction
- Clear boundaries between components
- Easy to test each component independently

### 2. Observer Pattern
- Model emits signals when state changes
- Controller and View listen to signals
- Loose coupling between components

### 3. Qt Parent-Child Memory Management
- All objects have proper parent-child relationships
- Automatic cleanup when parent is destroyed
- No manual memory management needed

### 4. Command Pattern (implicit)
- Each context menu action maps to a controller method
- Controller methods encapsulate the action logic
- Easy to add new actions
```
