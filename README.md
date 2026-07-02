# Stormworks MC Simulator

A desktop application for editing and simulating Stormworks microcontroller logic outside of the game. Built with C++17 and Qt6.

![Stormworks MC Simulator](docs/screenshot.png)

---

## Features

### Node Graph Editor
- Visual node graph canvas with a **40×40 pixel grid** that all nodes snap to
- **Pan** with middle-mouse drag, **zoom** with scroll wheel
- **Right-click** anywhere on the canvas to add nodes via a categorized context menu
- **Left-click drag** from an output port to wire nodes together
- **Left-click** an occupied input port to detach and rewire an existing connection
- Wire colors indicate signal type: 
- **red** = bool
- **green** = number
- **purple** = composite
- Video and audio have not been implemented yet.
- **Delete/Backspace** key removes the selected node along with all its connections
- Right-click a node to delete it directly from the context menu

### Live Simulation
- Tick-based simulation engine evaluating nodes in **topological order**.
- **Space** = step one tick.
- **Enter** = start/stop continuous simulation.
- Tick rate configurable via Simulation menu (default 10 tps).
- I/O panel on the right updates live with current input/output values.
- Inner display rectangle on each node shows **live port state** — bool circles fill red when true, number ports show current value.

### File Support
- **Open** any Stormworks microcontroller `.xml` file (File → Open or Ctrl+O).
- **Save** back to the original file (Ctrl+S) — preserves all vehicle metadata, non-MC content, and unknown node types.
- **Save As** to export to a new file (Ctrl+Shift+S).
- Files saved from this app load correctly back into the Stormworks game as of the last major update.
- Node positions are zeroed on save as I have not been able to figure out Stormworks node positioning yet.

---

## Supported Node Types

### I/O (Bridge Nodes)
| Node | Description |
|------|-------------|
| Bool Input | Boolean signal coming into the MC from the vehicle |
| Bool Output | Boolean signal going out from the MC to the vehicle |
| Number Input | Number signal coming into the MC — configurable slider range and step |
| Number Output | Number signal going out from the MC |
| Number Literal | Constant number value — configurable via ⚙ |

### Logic Gates
| Node | Type ID | Description |
|------|---------|-------------|
| NOT | (no type) | Logical invert |
| AND | 1 | True when all inputs true |
| OR | 2 | True when any input true |
| XOR | 3 | True when inputs differ |
| NAND | 4 | NOT AND |
| NOR | 5 | NOT OR |

### Math
| Node | Type ID | Description |
|------|---------|-------------|
| Add | 6 | A + B |
| Subtract | 7 | A − B |
| Multiply | 8 | A × B |
| Divide | 9 | A ÷ B (IsError output goes true on divide-by-zero) |
| Clamp | 10 | Clamp value to [min, max] — bounds configurable via ⚙ |
| Abs | 11 | Absolute value |
| Min | 12 | Minimum of A and B |
| Max | 13 | Maximum of A and B |
| Modulo | 14 | A mod B |
| Number Literal | 15 | Constant value — configurable via ⚙ |
| Greater Than | 17 | A > B → bool |
| Less Than | 18 | A < B → bool |
| Equal To | 42 | \|A − B\| ≤ tolerance → bool — tolerance configurable via ⚙ |

### Composite
| Node | Type ID | Description |
|------|---------|-------------|
| Composite Input | 4 (bridge) | Composite wire coming into the MC — per-channel index and type configurable in I/O panel |
| Composite Read (Number) | 31 | Reads one number channel from a composite wire — channel index configurable via ⚙ |
| Composite Read (Bool) | 29 | Reads one bool channel from a composite wire — channel index configurable via ⚙ |
| Composite Write (Number) | 40 | Writes number channels onto a composite wire — channel count and start index configurable via ⚙ |
| Composite Write (Bool) | 41 | Writes bool channels onto a composite wire — channel count and start index configurable via ⚙ |
| Composite Packer | — | Packs individual channels into a composite wire — channel count configurable via ⚙ |

### Stateful
| Node | Type ID | Description |
|------|---------|-------------|
| Up/Down Counter | 37 | Counts up/down by a configurable step, optional min/max clamp, configurable reset value |
| TON Timer | 49 | Output goes true after Enable has been true for ≥ Duration. Resets when Enable goes false |
| TOF Timer | 50 | Output goes true while elapsed < Duration after Enable goes false |
| RTO Timer | 51 | Retentive TON — accumulates time across Enable pulses, resets only via Reset input |
| RTF Timer | 52 | Retentive TOF — true while elapsed < Duration, resets only via Reset input |
| PID Controller | 23 | PID control loop — Kp/Ki/Kd configurable via ⚙, resets integral on disable |
| PID Advanced | 39 | PID control loop with Kp/Ki/Kd as live number input ports |

---

## I/O Panel

The panel on the right side of the window shows all MC edge ports:

**Inputs:**
- **Bool inputs** — toggle button (click to flip ON/OFF).
- **Number inputs** — spinbox + slider with configurable step size (shown below the slider).
- **Composite inputs** — inline channel count, per-channel index, type (bool/number), and value control.
- **Video inputs** - not implemented yet.
- **Audio inputs** - not implemented yet.

**Outputs:**
- **Bool outputs** — indicator LED (green = ON, dark red = OFF)
- **Number outputs** — live readout in cyan monospace text
- **Composite outputs** - not implemented yet.
- **Video outputs** - not implemented yet.
- **Audio outputs** - not implemented yet.

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
│   │   ├── Node.h / Node.cpp          # Base node class, NodeType enum, factory
│   │   ├── Connection.h               # Wire endpoint struct
│   │   ├── Microcontroller.h / .cpp   # Graph engine: tick, load, save, XML
│   │   └── nodes/
│   │       ├── IONodes.h              # Bool/Number Input/Output, NumberLiteral
│   │       ├── LogicNodes.h           # AND, OR, NOT, XOR, NAND, NOR
│   │       ├── MathNodes.h            # Add, Subtract, Multiply, Divide, Clamp, etc.
│   │       ├── CompositeNodes.h       # Composite Input/Read/Write/Packer
│   │       └── StatefulNodes.h        # Counter, Timers, PID
│   └── gui/
│       ├── MainWindow.h / .cpp        # Main window, menus, file I/O, sim control
│       ├── NodeGraphView.h / .cpp     # Canvas: grid, pan/zoom, wire dragging, context menu
│       ├── NodeGraphItem.h / .cpp     # Per-node visual: drawing, gate symbols, settings dialogs
│       ├── ConnectionItem.h / .cpp    # Wire visual (bezier curve, color by type)
│       └── IOPanel.h / .cpp           # Right-side I/O controls panel
├── resources.qrc                      # Qt resource file (icons)
├── src/gui/icons/
│   ├── up_arrow.png                   # Spinbox up arrow icon
│   └── down_arrow.png                 # Spinbox down arrow icon
└── CMakeLists.txt
```

---

## Known Limitations

- **I/O bridge nodes** (Bool/Number Input/Output, Composite Input) can only be edited in-place — adding or deleting them from the simulator will not be reflected when saving, since their corresponding vehicle-edge entries in the XML are not modelled. Use Stormworks' own MC editor to add/remove these.
- Node positions are not preserved on save (all written as 0,0).
- The expression/function evaluator node (type 45) is not yet supported.
- Memory/register nodes are not yet supported.

---

## Credits

Built by Brandon Staley under the **Staley Engineering** brand.
