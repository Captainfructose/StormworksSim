# Stormworks MC Simulator

A desktop application for editing and simulating Stormworks microcontroller logic outside of the game. Built with C++17 and Qt6.

The editor is styled after the in-game microcontroller editor: a teal board with a grid, white rounded nodes with grey display panels, and straight wires with direction arrows.

---

## Features

### Node Graph Editor
- Visual node graph canvas with a **40×40 pixel grid** that all nodes snap to
- A **fixed-size board** (80 × 48 cells) like the game's editor: the grid stops at the board's edge, nodes can't be dragged off it, and panning and zoom are limited to it. The size is set by `kBoard…` constants in `NodeGraphView.h`.
- **Pan** with middle-mouse drag, **zoom** with scroll wheel
- **Adding nodes**, three ways — all built from one shared node list (`NodeCatalog.h`):
  - **Right-click** the canvas → Add Node: the node appears where you clicked
  - **Add Node** in the menu bar: the node appears in the middle of the view
  - **Tab** opens a **palette** of node tiles grouped by category, like the game's: click a tile to add it where the mouse was. A search box filters as you type (**Enter** adds the first match), and **Esc** or a click on the canvas closes it.
- **Left-click drag** from an output port to wire nodes together
- **Left-click** an occupied input port to detach and rewire an existing connection
- Wire colors indicate signal type, and only matching types can be connected:
  - **red** = bool
  - **green** = number
  - **purple** = composite
- Wires are straight lines with small direction arrows, drawn on top of nodes
- Input ports are **rings** on the left edge of a node; output ports are **filled discs** on the right edge
- **Delete/Backspace** key removes the selected node along with all its connections
- Right-click a node to delete it directly from the context menu
- Nodes with settings have a **⚙** button in the header
- Hover a port to see its **full label and type** in a tooltip (labels on the node itself are kept short)
- Video and audio have not been implemented yet.

### Node Appearance
- Nodes are **4 grid squares wide**, like in the game. Each is a white rounded block with a small grey **icon** (a short glyph such as `&`, `+`, `Δ` or `fx`) and a **bold** title at the top left. There is no header strip.
- A grey **display panel** in the middle shows live state: bool indicators fill when true, number ports show their current value. The panel is normally 86 px wide and **shrinks automatically** (to at least 50 px) when a port label needs more room; labels that still don't fit are shortened with "…".
- **Math nodes show their live equation on one line** in the panel instead of separate values, for example `3 + 4 = 7`, `3 > 2 → ON`, or `0 ≤ 12.5 ≤ 100 → ON` for Threshold. Values are **bold**, operators plain; the font shrinks to fit.
- Property nodes show their **property name** above their current value in the panel
- **SR Latch** and **JK Flip-Flop** use the game's layout more closely: icon with the node's letters, labelled ports, and no display panel
- I/O nodes show their **connector label** as the title when one is set

### Live Simulation
- Tick-based simulation engine evaluating nodes in **topological order**.
- **Space** = step one tick.
- **Enter** = start/stop continuous simulation.
- Tick rate configurable via Simulation menu (default 10 tps).
- I/O panel on the right updates live with current input/output values.

### Composite Signals
A composite wire carries **32 number channels and 32 bool channels**, numbered 1–32, like in Stormworks. Number and bool channels are separate namespaces.
- **Composite Input** builds a composite from the channels set up in the I/O panel.
- **Composite Read** nodes pick one channel out of a composite.
- **Composite Write** nodes overlay their channels onto an incoming composite and leave the other channels untouched.
- **Binary to Number / Number to Binary** convert between the 32 bool channels and a signed 32-bit number (channel 1 = least significant bit).
- **Composite Switchbox** selects between two whole composites.
- **Lua Script** nodes keep and save their script but do **not** run it (see Known Limitations).

### File Support
- **Open** any Stormworks microcontroller `.xml` file (File → Open or Ctrl+O).
- **Save** back to the original file (Ctrl+S) — preserves all vehicle metadata, non-MC content, and unknown node types.
- **Save As** to export to a new file (Ctrl+Shift+S).
- A microcontroller built from scratch (nothing loaded) can be saved too — it starts from the empty skeleton Stormworks writes for a new microcontroller.
- Files saved from this app load correctly in the Stormworks game.
- **Node positions are saved.** Positions are converted back to Stormworks grid units (rounded to 0.25) so layouts carry over to the game.
- **I/O connectors are fully saved:** new Bool/Number/Composite Input and Output nodes are written (component entry plus edge connector, in the first free board slot — the board grows by one row when full), and deleted ones are removed from both places.
- **Connector labels** are loaded from and saved to the file. Edit one from the node's ⚙ dialog.
- Wires from a node's **second or later output** (for example the `!Q` output of an SR Latch) are loaded and saved using the file's `node_index` attribute.
- Lua scripts keep their line breaks and tabs through load and save.

---

## Supported Node Types

The Type ID is the number Stormworks writes for the node in the file's `<components>` section.

### I/O (Bridge Nodes)
These are the microcontroller's connections to the vehicle. Each has a **label** (⚙) shown in Stormworks.

|       Node       | Bridge type |                              Description                              |
|------------------|-------------|-----------------------------------------------------------------------|
| Bool Input       | (none)      | Boolean signal coming into the MC from the vehicle                    |
| Bool Output      | 1           | Boolean signal going out from the MC to the vehicle                   |
| Number Input     | 2           | Number signal coming into the MC — configurable slider range and step |
| Number Output    | 3           | Number signal going out from the MC                                   |
| Composite Input  | 4           | Composite wire coming into the MC — channels set up in the I/O panel  |
| Composite Output | 5           | Composite wire going out from the MC                                  |

### Logic
|       Node       | Type ID |                       Description                        |
|------------------|---------|----------------------------------------------------------|
| NOT              | (none)  | Logical invert                                           |
| AND              | 1       | True when all inputs true                                |
| OR               | 2       | True when any input true                                 |
| XOR              | 3       | True when inputs differ                                  |
| NAND             | 4       | NOT AND                                                  |
| NOR              | 5       | NOT OR                                                   |
| Constant On      | 16      | Always true                                              |
| SR Latch         | 24      | Set / Reset inputs → Q and !Q (Set wins if both on)      |
| JK Flip-Flop     | 25      | J sets, K resets, both toggle each tick → Q and !Q       |
| Capacitor        | 26      | Charge / discharge times (seconds) — on when full, off when empty |
| Push to Toggle   | 28      | Output flips each time the input turns on                |
| Toggle to Push   | 48      | Output is on for one tick whenever the input changes     |
| Bool Function (4)| 46      | Expression over bool inputs x, y, z, w                   |
| Bool Function (8)| 47      | Expression over bool inputs x, y, z, w, a, b, c, d       |

### Math
|        Node         | Type ID |                             Description                              |
|---------------------|---------|----------------------------------------------------------------------|
| Add                 | 6       | A + B                                                                |
| Subtract            | 7       | A − B                                                                |
| Multiply            | 8       | A × B                                                                |
| Divide              | 9       | A ÷ B (the Err output goes true on divide-by-zero)                   |
| Clamp               | 11      | Clamp value to [min, max] — bounds configurable via settings         |
| Threshold           | 12      | True while min ≤ value ≤ max — bounds configurable via settings      |
| Abs                 | 14      | Absolute value                                                       |
| Constant Number     | 15      | Constant value — configurable via settings                           |
| Greater Than        | 17      | A > B → bool                                                         |
| Less Than           | 18      | A < B → bool                                                         |
| Numerical Switchbox | 22      | On value / Off value / Switch → outputs the value the switch selects |
| Delta               | 35      | Change in the input since the previous tick                          |
| Modulo              | 38      | A mod B                                                              |
| Equal To            | 42      | \|A − B\| ≤ tolerance → bool — tolerance configurable via settings  |

### Function (expression evaluator)
|     Node        | Type ID |                       Description                        |
|-----------------|---------|----------------------------------------------------------|
| Function (1)    | 45      | Expression over x                                        |
| Function (3)    | 10      | Expression over x, y, z                                  |
| Function (8)    | 36      | Expression over x, y, z, w, a, b, c, d                   |

The expression is edited in the node's ⚙ dialog, which lists the supported operators and functions.

### Composite
|           Node           | Type ID |                                    Description                                      |
|--------------------------|---------|-------------------------------------------------------------------------------------|
| Composite Read (Number)  | 31      | Reads one number channel — channel index configurable via settings                  |
| Composite Read (Bool)    | 29      | Reads one bool channel — channel index configurable via settings                    |
| Composite Write (Number) | 40      | Writes number channels onto a composite — channel count and start index configurable|
| Composite Write (Bool)   | 41      | Writes bool channels onto a composite — channel count and start index configurable  |
| Composite Switchbox      | 53      | Composite On / Composite Off / Switch → outputs the composite the switch selects    |
| Number to Binary         | 54      | Number → the 32 bool channels of a composite (bits of a signed 32-bit integer)      |
| Binary to Number         | 55      | The 32 bool channels of a composite → number                                        |
| Lua Script               | 56      | Composite in/out with a script — stored and saved, **not executed**                 |
| Composite Packer         | —       | Simulator-only helper: packs number inputs Ch1..ChN into one composite              |

### Stateful
|      Node       | Type ID |                                       Description                                         |
|-----------------|---------|-------------------------------------------------------------------------------------------|
| Memory Register | 13      | Set stores the Value input, Reset clears it                                               |
| Up/Down Counter | 37      | Counts up/down by a configurable step, optional min/max clamp, configurable reset value   |
| Blinker         | 27      | While Control is on, output is on for the on time then off for the off time (0–5 seconds) |
| TON Timer       | 49      | Output goes true after Enable has been true for ≥ Duration. Resets when Enable goes false |
| TOF Timer       | 50      | Output goes true while elapsed < Duration after Enable goes false                         |
| RTO Timer       | 51      | Retentive TON — accumulates time across Enable pulses, resets only via Reset input        |
| RTF Timer       | 52      | Retentive TOF — true while elapsed < Duration, resets only via Reset input                |
| PID Controller  | 23      | PID control loop — Kp/Ki/Kd configurable via settings, resets integral on disable         |
| PID Advanced    | 39      | PID control loop with Kp/Ki/Kd as live number input ports                                 |

### Property
|      Node        | Type ID |                                   Description                                    |
|------------------|---------|----------------------------------------------------------------------------------|
| Property Dropdown| 20      | Named list of label/value options with one selected — outputs the selected value |
| Property Slider  | 19      | Named number with a minimum, maximum, step and value                             |
| Property Toggle  | 33      | Named on/off switch                                                              |
| Property Number  | 34      | Named number value                                                               |
| Property Text    | 58      | Named text property (name only; it has no ports)                                 |
| Tooltip Number   | 43      | Shows a number input in the vehicle tooltip under a label                        |
| Tooltip On/Off   | 44      | Shows a bool input in the tooltip as one of two texts                            |

---

## I/O Panel

The panel on the right side of the window shows all MC edge ports. Each row uses the connector's label if it has one:

**Inputs:**
- **Bool inputs** — toggle button (click to flip ON/OFF).
- **Number inputs** — spinbox + slider with configurable step size (shown below the slider).
- **Composite inputs** — inline channel count, per-channel index, type (bool/number), and value control.
- **Video inputs** - not implemented yet.
- **Audio inputs** - not implemented yet.

**Outputs:**
- **Bool outputs** — indicator LED (green = ON, dark red = OFF)
- **Number outputs** — live readout in cyan monospace text
- **Composite outputs** - not shown in the panel (a composite has no single value).
- **Video outputs** - not implemented yet.
- **Audio outputs** - not implemented yet.

The panel is rebuilt when a file is loaded, so a connector label edited in a node's ⚙ dialog shows there the next time the microcontroller is loaded.

---

## Build Instructions

### Requirements
- **Qt 6.x** with Widgets and Xml components  
  (tested with Qt 6.11.1 msvc2022_64)
- **CMake 3.20+**
- **MSVC 2022** (Windows) or a C++17-capable compiler

### Windows (MSVC via Developer Command Prompt)

```bat
cd StormworksSim
mkdir build_msvc
cd build_msvc
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
cmake --build . --config Debug
```

After building, deploy Qt DLLs:
```bat
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe Debug\StormworksSim.exe
```

Then run:
```bat
Debug\StormworksSim.exe
```

If a rebuild fails with `LNK1168: cannot open ... StormworksSim.exe for writing`, close the running app first.

### VS Code (with CMake Tools extension)

1. Add to `.vscode/settings.json`:
```json
{
  "cmake.configureArgs": [
    "-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64"
  ]
}
```
2. **CMake: Select a Kit** → Visual Studio Build Tools 2022 - amd64
3. **CMake: Build** (Shift+F7)

---

## Project Structure

```
StormworksSim/
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── Node.h / Node.cpp          # Base node class, Port/CompositeData, NodeType enum, factory
│   │   ├── Connection.h               # Wire endpoint struct
│   │   ├── Microcontroller.h / .cpp   # Graph engine: tick, load, save, XML
│   │   └── nodes/
│   │       ├── IONodes.h              # Bool/Number Input/Output, Constant Number, Property/Tooltip nodes
│   │       ├── LogicNodes.h           # AND, OR, NOT, XOR, NAND, NOR, Constant On, SR, JK, Pulse nodes
│   │       ├── MathNodes.h            # Add, Subtract, Multiply, Divide, Clamp, Threshold, Delta, Switchbox, etc.
│   │       ├── CompositeNodes.h       # Composite Input/Output/Read/Write/Switchbox, Binary, Lua, Packer
│   │       ├── StatefulNodes.h        # Counter, Memory Register, Blinker, Capacitor, Timers, PID
│   │       └── FunctionNodes.h        # Function / Bool Function expression evaluators
│   └── gui/
│       ├── MainWindow.h / .cpp        # Main window, menus, file I/O, sim control
│       ├── NodeGraphView.h / .cpp     # Canvas: board, grid, pan/zoom, wire dragging, Add Node menus
│       ├── NodeCatalog.h              # The shared list of addable nodes, by category
│       ├── NodePalette.h / .cpp       # Tab palette: searchable tiles of node types
│       ├── NodeGraphItem.h / .cpp     # Per-node visual: drawing, gate symbols, settings dialogs
│       ├── ConnectionItem.h / .cpp    # Wire visual (straight line with arrows, color by type)
│       └── IOPanel.h / .cpp           # Right-side I/O controls panel
├── resources.qrc                      # Qt resource file (icons)
├── src/gui/icons/
│   ├── up_arrow.png                   # Spinbox up arrow icon
│   └── down_arrow.png                 # Spinbox down arrow icon
└── CMakeLists.txt
```

---

## Known Limitations

- **Lua scripts are not run.** A Lua Script node keeps, edits and saves its script, but its output composite stays empty. Running scripts would need an embedded Lua interpreter and the Stormworks `input`, `output` and `property` functions; `onDraw` and `screen` drawing cannot work without a monitor.
- **Video and audio** signals are not implemented.
- **Property Text** saves only its name; where the game stores its text value is not known yet.
- **Property Toggle** stores its on/off state as `v="true"` (absent when off) based on sample files; other property nodes' attribute names were also taken from samples.
- **Binary conversions** treat channel 1 as the least significant bit of a signed 32-bit integer — check this against the game.
- **Blinker** new nodes default to 0.5 s on / 0.5 s off; the in-game defaults are not confirmed.
- **Board size:** the 80 × 48 board is an estimate, not the game's exact work-area size.
- **Palette icons** are the same short text glyphs the nodes use, not the game's drawn icons.
- Deleting I/O nodes does not shrink the board, and the I/O panel does not refresh when a connector label is renamed.
- A few Stormworks nodes are still missing; each needs a sample file to get its type ID and settings.
- The simulator's Composite Packer is not a Stormworks node and is not written to saved files.

---

## Credits

Built by Brandon Staley under the **Staley Engineering** brand.
