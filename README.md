[README (2).md](https://github.com/user-attachments/files/33030757/README.2.md)
# C-SCI 991 — Scientific Calculator in Pure C

> Complete project documentation: architecture, structure, ports, build, internals, and known issues.
> Generated from a full read of every file in `c_scientific_calculator.zip`.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Ports & Networking](#2-ports--networking)
3. [Tech Stack & Dependencies](#3-tech-stack--dependencies)
4. [Project Structure](#4-project-structure)
5. [High-Level Architecture](#5-high-level-architecture)
6. [Code Map of `main.c`](#6-code-map-of-mainc)
7. [Data Structures & Global State](#7-data-structures--global-state)
8. [Calculation Engine](#8-calculation-engine)
9. [GUI Layer](#9-gui-layer)
10. [Input Handling & State Machine](#10-input-handling--state-machine)
11. [Build & Run Instructions](#11-build--run-instructions)
12. [Configuration Reference](#12-configuration-reference)
13. [Supported Features & Syntax](#13-supported-features--syntax)
14. [Error Handling Reference](#14-error-handling-reference)
15. [Verified Test Results](#15-verified-test-results)
16. [Known Issues & Limitations](#16-known-issues--limitations)
17. [How to Extend the Project](#17-how-to-extend-the-project)
18. [Git / GitHub Setup](#18-git--github-setup)

---

## 1. Project Overview

| Item | Value |
|---|---|
| **Name** | C-SCI 991 — Scientific Calculator |
| **Repository name (suggested)** | `c-scientific-calculator` |
| **Type** | Standalone native desktop GUI application |
| **Language** | C (C11 standard) |
| **GUI library** | [raylib](https://www.raylib.com) |
| **Source files** | 1 (`main.c`, 405 lines) |
| **Window size** | 470 × 820 px (fixed) |
| **Frame rate** | 60 FPS |
| **Architecture style** | Single-file, single-process, immediate-mode GUI |
| **Branding** | Casio-*inspired* look, but original — no Casio branding or assets |

**What it does:** a clickable, Casio-style scientific calculator. It parses typed or clicked expressions using a hand-written tokenizer + Shunting-Yard parser and evaluates them with a Reverse Polish Notation (RPN) evaluator. All of this is written in pure C with no parsing libraries.

---

## 2. Ports & Networking

### ✅ This project uses **NO ports**.

| Question | Answer |
|---|---|
| Does it open a TCP/UDP port? | **No** |
| Does it run a server (HTTP, WebSocket, etc.)? | **No** |
| Does it make outbound network requests? | **No** |
| Does it use a database? | **No** (no DB, no files read/written at runtime) |
| Does it need environment variables? | **No** |
| Does it need a `.env` file or API keys? | **No** |
| Does it use Docker / containers? | **No** (none included) |

It is a fully **offline, local, single-process** desktop program. The only "interfaces" it uses are:

- **Window + OpenGL context** (created by raylib via `InitWindow`)
- **Mouse + keyboard events** (read through raylib)

There is nothing to expose, forward, or firewall.

> If you later wrap this in a web or API layer (e.g., WebAssembly via Emscripten, or a REST wrapper around the engine), *that* is when ports would matter. Today: none.

---

## 3. Tech Stack & Dependencies

### Language & standard
- **C11** (`-std=c11` in the Makefile, `CMAKE_C_STANDARD 11` in CMake)

### Standard C headers used (`main.c` lines 1–8)

| Header | Used for |
|---|---|
| `raylib.h` | Window, drawing, input |
| `ctype.h` | `isdigit`, `isalpha`, `isspace`, `tolower` |
| `math.h` | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sqrt`, `log`, `log10`, `exp`, `pow`, `fabs`, `round`, `isfinite` |
| `stdbool.h` | `bool` |
| `stdio.h` | `snprintf` |
| `stdlib.h` | `strtod` |
| `string.h` | `strcmp`, `strcpy`, `strncpy`, `strcat`, `strlen`, `strchr` |

### External dependency

| Dependency | Version | Notes |
|---|---|---|
| **raylib** | `5.0` minimum per `CMakeLists.txt` | See the compatibility note below |
| C compiler | GCC or any C11 compiler | Makefile defaults to `gcc` |
| `make` | any | For the Makefile build |
| CMake | ≥ 3.16 | For the CMake build (optional) |

> ⚠️ **raylib version note:** the code calls `DrawRectangleRoundedLinesEx(...)` (main.c line ~389). To my knowledge, that function was added in a newer raylib release than 5.0 (around 5.5). The CMake file only demands 5.0, so if the build fails on that call, upgrade raylib. Please verify against your installed version.

### Linked libraries per platform

| Platform | Link flags (Makefile) |
|---|---|
| **Windows** | `-lraylib -lopengl32 -lgdi32 -lwinmm` |
| **Linux** | `-lraylib -lm -lpthread -ldl -lrt -lX11` |
| **CMake (all)** | `raylib`, `m`; plus `winmm` on Windows |
| **macOS** | Not handled by the Makefile (needs Cocoa/IOKit/OpenGL frameworks). Use CMake or add a macOS branch. |

---

## 4. Project Structure

```text
c_scientific_calculator/
├── .gitignore          # Git ignore rules (54 bytes)
├── CMakeLists.txt      # CMake build script (362 bytes)
├── Makefile            # GNU Make build script (352 bytes)
├── README.md           # Short user-facing readme (1,552 bytes)
└── main.c              # THE ENTIRE APPLICATION (17,462 bytes, 405 lines)
```

**Total: 5 files** (+ 1 directory), ~19.8 KB uncompressed.

### File-by-file details

| File | Purpose | Key contents |
|---|---|---|
| **`main.c`** | Whole application: math engine + GUI + input + main loop | Tokenizer, Shunting-Yard parser, RPN evaluator, raylib UI |
| **`Makefile`** | Quick build with GCC | Detects Windows via `$(OS)`; targets `all`, `clean` |
| **`CMakeLists.txt`** | Cross-platform build | `find_package(raylib 5.0 REQUIRED)`, target `c_scientific_calculator` |
| **`README.md`** | Short intro | Features, build steps, controls, example expressions |
| **`.gitignore`** | Keeps repo clean | `build/`, `*.exe`, `scientific_calculator`, `.vscode/`, `.DS_Store` |

### Build output names (differ by build system!)

| Build method | Output binary | Location |
|---|---|---|
| `make` (Linux) | `scientific_calculator` | project root |
| `make` (Windows) | `scientific_calculator.exe` | project root |
| CMake | `c_scientific_calculator` (`.exe` on Windows) | `build/` |

---

## 5. High-Level Architecture

The app is a **single translation unit** with three logical layers, all in `main.c`:

```text
┌──────────────────────────────────────────────────────────────┐
│                         main.c                               │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  LAYER 3 — GUI / Main Loop            (lines 271–404)  │  │
│  │  • InitWindow, 60 FPS loop, draw calls                 │  │
│  │  • Button grid construction & hit-testing              │  │
│  │  • Keyboard + mouse polling                            │  │
│  └───────────────────────┬────────────────────────────────┘  │
│                          │ calls                             │
│  ┌───────────────────────▼────────────────────────────────┐  │
│  │  LAYER 2 — Calculator State & Input Actions            │  │
│  │                                       (lines 233–269)  │  │
│  │  appendText() backspace() clearAll() doEvaluate()      │  │
│  │  Globals: expression, displayResult, statusMsg,        │  │
│  │           ansValue, degreeMode, justEvaluated          │  │
│  └───────────────────────┬────────────────────────────────┘  │
│                          │ calls                             │
│  ┌───────────────────────▼────────────────────────────────┐  │
│  │  LAYER 1 — Pure Math Engine (no GUI dependency)        │  │
│  │                                        (lines 21–231)  │  │
│  │  tokenize() → toRPN() → evalRPN()                      │  │
│  │  wrapped by evaluateExpression()                       │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                           │
                           ▼
                  raylib  →  OpenGL  →  OS window
```

### Evaluation pipeline (data flow)

```text
  User input (click / keyboard)
          │
          ▼
   appendText()  ──►  expression[256]   (plain C string, e.g. "sin(30)+2^3")
                              │
                      press "=" / Enter
                              ▼
                        doEvaluate()
                              │
                              ▼
                    evaluateExpression()
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
    tokenize()            toRPN()               evalRPN()
  string → Token[]    infix → postfix       postfix → double
  (max 256 tokens)   (Shunting-Yard)        (value stack)
                              │
                              ▼
            success: ansValue = v; displayResult = "%.12g"; status "OK"
            failure: displayResult = "Error"; statusMsg = <reason>
```

### Design characteristics

- **Immediate-mode GUI:** there is no widget tree. Every frame, the code reads input, updates state, then redraws everything.
- **No dynamic memory:** no `malloc`/`free`; all buffers are fixed-size (stack or static).
- **Global state:** six `static` globals hold all calculator state (no structs for state, no heap).
- **Zero file/network I/O.**
- **Engine is separable:** Layer 1 only depends on `degreeMode` and `ansValue` globals (and `Token`), not on raylib. It can be extracted into its own `.c/.h` pair with minimal work.

---

## 6. Code Map of `main.c`

| Lines | Section | Description |
|---|---|---|
| 1–8 | Includes | raylib + standard C headers |
| 9–12 | `#define`s | `SCREEN_W`, `SCREEN_H`, `MAX_EXPR`, `MAX_TOKENS` |
| 14–19 | Math constant fallbacks | Defines `M_PI`, `M_E` if the platform lacks them (MSVC/strict C11) |
| 21–27 | `TokenType` enum | `TOK_NUMBER, TOK_OP, TOK_FUNC, TOK_LPAREN, TOK_RPAREN` |
| 29–33 | `Token` struct | `type`, `value`, `text[16]` |
| 35–40 | `Button` struct | `rect`, `label`, `insert`, `kind` |
| 42–47 | Global state | `degreeMode`, `expression`, `displayResult`, `statusMsg`, `ansValue`, `justEvaluated` |
| 49–50 | `toRad`, `fromRad` | Angle conversion respecting DEG/RAD mode |
| 52–59 | `precedence()` | Operator precedence table |
| 61–63 | `rightAssociative()` | `^` and unary minus |
| 65–69 | `isFunctionName()` | Whitelist of 12 function names |
| 71–129 | `tokenize()` | String → tokens (numbers, constants, functions, operators, parens) |
| 131–167 | `toRPN()` | Shunting-Yard: infix → postfix |
| 169–172 | `factorial()` | Integer factorial with domain checks |
| 174–223 | `evalRPN()` | Stack evaluator with all math + error checks |
| 225–231 | `evaluateExpression()` | Public entry point of the engine |
| 233–241 | `appendText()` | Add text to the expression (with auto-clear logic) |
| 243–246 | `backspace()` | Delete last character |
| 248–253 | `clearAll()` | Reset expression, result, status |
| 255–269 | `doEvaluate()` | Run engine, update display/Ans/status |
| 271 | `withAlpha()` | Color helper |
| 273–279 | `drawTextFit()` | Auto-shrink text to fit a rectangle |
| 281–404 | `main()` | Window init, button grid, main loop (input → update → draw) |
| 310–322 | Button grid builder | Builds the 41 buttons |
| 324 | Main loop start | `while (!WindowShouldClose())` |
| 352–400 | Draw section | `BeginDrawing()` … `EndDrawing()` |

---

## 7. Data Structures & Global State

### `Token`
```c
typedef struct {
    TokenType type;   // TOK_NUMBER | TOK_OP | TOK_FUNC | TOK_LPAREN | TOK_RPAREN
    double    value;  // numeric value (for TOK_NUMBER)
    char      text[16]; // operator / function name / display text
} Token;
```

### `Button`
```c
typedef struct {
    Rectangle   rect;    // position + size on screen
    const char *label;   // text drawn on the key
    const char *insert;  // text appended to expression on click ("" for actions)
    int         kind;    // 0 normal, 1 function, 2 action, 3 equals, 4 accent (unused)
} Button;
```

### Global state variables

| Variable | Type | Initial | Purpose |
|---|---|---|---|
| `degreeMode` | `bool` | `true` | `true` = DEG, `false` = RAD |
| `expression` | `char[256]` | `""` | Current input string |
| `displayResult` | `char[64]` | `"0"` | Result line shown on LCD |
| `statusMsg` | `char[64]` | `"DEG"` | Status/error line under the LCD |
| `ansValue` | `double` | `0.0` | Last successful result (used by `Ans`) |
| `justEvaluated` | `bool` | `false` | `true` right after a successful `=` |

---

## 8. Calculation Engine

### 8.1 Stage 1 — Tokenizer (`tokenize`)

Scans the string left-to-right and classifies each piece:

| Input pattern | Becomes |
|---|---|
| Digits / `.` | `TOK_NUMBER` via `strtod` |
| `pi` | `TOK_NUMBER` = π |
| `e` | `TOK_NUMBER` = e |
| `ans` (case-insensitive) | `TOK_NUMBER` = `ansValue` |
| Known function name | `TOK_FUNC` |
| Unknown word | Error `Unknown: <name>` |
| `(` / `)` | `TOK_LPAREN` / `TOK_RPAREN` |
| `+ - * / ^ !` | `TOK_OP` |
| Anything else | Error `Unexpected '<c>'` |

**Details**
- Whitespace is skipped.
- Names are lowercased (`SIN`, `Sin`, `sin` all work) and capped at 15 letters.
- **Unary minus:** a `-` becomes `u-` when the previous token is an operator, `(`, or a function (or at the start).
- Limit: **256 tokens**.

### 8.2 Stage 2 — Shunting-Yard (`toRPN`)

Converts infix to postfix.

**Operator precedence table**

| Operator | Precedence | Associativity |
|---|---|---|
| `!` (factorial) | 6 (highest) | left |
| `u-` (unary minus) | 5 | **right** |
| `^` (power) | 4 | **right** |
| `*`, `/` | 3 | left |
| `+`, `-` | 2 | left |

- Functions are pushed on the stack and popped to output after their closing `)`.
- Mismatched parentheses produce `Mismatched parentheses`.

### 8.3 Stage 3 — RPN Evaluator (`evalRPN`)

Uses a `double[256]` value stack.

**Supported operators**

| Op | Behavior |
|---|---|
| `+ - * /` | Standard; `/` errors if \|divisor\| < 1e-15 |
| `^` | `pow(a, b)` |
| `u-` | Negation |
| `!` | Factorial; integer only, `0 ≤ x ≤ 170` (tolerance 1e-10) |

**Supported functions (12)**

| Function | Implementation | Angle-aware? | Domain check |
|---|---|---|---|
| `sin(x)` | `sin(toRad(x))` | ✅ input | — |
| `cos(x)` | `cos(toRad(x))` | ✅ input | — |
| `tan(x)` | `tan(toRad(x))` | ✅ input | — |
| `asin(x)` | `fromRad(asin(x))` | ✅ output | NaN → `Math error` |
| `acos(x)` | `fromRad(acos(x))` | ✅ output | NaN → `Math error` |
| `atan(x)` | `fromRad(atan(x))` | ✅ output | — |
| `sqrt(x)` | `sqrt(x)` | — | `x < 0` → `sqrt domain error` |
| `ln(x)` | `log(x)` | — | `x ≤ 0` → `ln domain error` |
| `log(x)` | `log10(x)` | — | `x ≤ 0` → `log domain error` |
| `exp(x)` | `exp(x)` | — | overflow → `Math error` |
| `abs(x)` | `fabs(x)` | — | — |
| `inv(x)` | `1.0 / x` | — | \|x\| < 1e-15 → `Division by zero` |

**Constants:** `pi` (3.14159…), `e` (2.71828…), `Ans` (last result).

Any result that is not finite (`inf`/`NaN`) becomes `Math error`.

### 8.4 Result formatting (`doEvaluate`)
- Values with \|v\| < 1e-13 are snapped to `0` (cleans up float noise like `sin(180)`).
- Displayed with `printf("%.12g")` — 12 significant digits.
- `ansValue` stores the **full-precision** value (before display formatting, but after the near-zero snap).

---

## 9. GUI Layer

### 9.1 Window setup
```c
SetConfigFlags(FLAG_MSAA_4X_HINT);          // 4x anti-aliasing
InitWindow(470, 820, "C Scientific Calculator");
SetTargetFPS(60);
```
The window is a fixed 470×820 px and is not resizable.

### 9.2 Screen layout (top to bottom)

| Region | Coordinates (x, y, w, h) | Contents |
|---|---|---|
| Outer body (dark border) | `25, 18, 420, 784` | Rounded rectangle |
| Inner body | `31, 24, 408, 772` | Rounded rectangle |
| Header panel | `50, 42, 370, 74` | Gradient; "C-SCI", "991 PRO", "NATURAL-V.P.A.M." |
| Solar-panel decoration | `290, 50, 112, 45` | Brown rectangles + "2nd edition" text |
| LCD frame | `52, 130, 366, 132` | Black rounded rect |
| LCD screen | `58, 136, 354, 120` | Green-gray background |
| Mode label | `66, 142` | "DEG" / "RAD" |
| "Math" label | `356, 142` | static text |
| Expression line | `66, 163, 338, 42` | Font 25, left-aligned, auto-shrink |
| Result line | `66, 204, 338, 38` | Font 31, right-aligned, auto-shrink |
| Status message | `58, 271` | Yellow, font 12 |
| Footer tagline | `58, 292` | "SCIENTIFIC CALCULATOR • PURE C + RAYLIB" |
| Button grid | starts `45, 335` | 6 columns × 7 rows |
| Keyboard hint | `38, 781` | Font 9 |

### 9.3 Button grid

**Geometry:** `startX=45`, `startY=335`, `gap=7`, `width=58`, `height=48`
→ grid spans x: 45–428, y: 335–713.
**Total buttons: 41** (42 cells; the last cell is skipped because the wide `=` spans two columns).

| Row | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 |
|---|---|---|---|---|---|---|
| 1 | SHIFT | ALPHA | DEG/RAD | `(` | `)` | DEL |
| 2 | sin | cos | tan | sqrt | x^y | AC |
| 3 | asin | acos | atan | ln | log | 1/x |
| 4 | 7 | 8 | 9 | `/` | pi | e |
| 5 | 4 | 5 | 6 | `*` | abs | `!` |
| 6 | 1 | 2 | 3 | `-` | Ans | exp |
| 7 | 0 | `.` | +/- | `+` | **`=`** (double width, spans cols 5–6) | |

**What each button inserts**

| Label | Inserts | | Label | Inserts |
|---|---|---|---|---|
| sin / cos / tan | `sin(` / `cos(` / `tan(` | | x^y | `^` |
| asin / acos / atan | `asin(` / `acos(` / `atan(` | | 1/x | `inv(` |
| sqrt | `sqrt(` | | +/- | `-` |
| ln / log / exp / abs | `ln(` / `log(` / `exp(` / `abs(` | | pi / e / Ans | `pi` / `e` / `Ans` |

**Special (action) buttons:** `AC` (clear all), `DEL` (backspace), `DEG/RAD` (toggle), `=` (evaluate), `SHIFT` and `ALPHA` (**cosmetic only** — they just show a hint message in the status line; see [limitations](#16-known-issues--limitations)).

### 9.4 Button colors

| Button type | Color (RGBA) |
|---|---|
| Normal (digits) | `(73,75,80)` |
| Function (`kind==1`) | `(53,55,60)` |
| AC | `(84,120,52)` green |
| DEL | `(93,135,57)` green |
| `=` | `(81,98,145)` blue |
| SHIFT / ALPHA | `(47,49,54)` with yellow / red label |
| Hover | base color brightened by 12% |
| Border | black at alpha 90, 1 px |

Labels longer than 5 characters use font size 13; others use 20.

### 9.5 Main loop order (each frame)
1. Read mouse position + left-click state
2. Drain `GetCharPressed()` queue → append valid characters
3. Handle `Enter`, `Backspace`, `Esc`
4. Hit-test every button against the click → perform its action
5. `BeginDrawing()` → draw body, header, LCD, status, buttons, footer → `EndDrawing()`

---

## 10. Input Handling & State Machine

### 10.1 Controls

| Input | Action |
|---|---|
| Left-click on a key | Perform the key's action / insert its text |
| `0–9`, `.`, `+`, `-`, `*`, `/`, `^`, `(`, `)` | Typed into the expression |
| `Enter` / Numpad `Enter` | Evaluate |
| `Backspace` | Delete last character |
| `Esc` | Clear all (⚠️ see issue #1 in limitations) |

**Not accepted from the keyboard:** letters (so `sin`, `pi`, `Ans` etc. must be clicked) and `!`. Only characters in `0123456789.+-*/^()` are accepted.

### 10.2 The `justEvaluated` behavior

After a successful `=`, the next input is treated differently:

| Next input starts with… | Effect |
|---|---|
| a digit, `.`, or a letter (includes `sin(`, `pi`, `e`, `Ans`) | **Expression is cleared first**, then the text is added (starts a fresh calculation) |
| anything else (`+ - * / ^ ( ) !`) | Flag is reset; text is **appended to the old expression string** |

Because the new operator is appended to the *old expression text* (not to `Ans`), `2+3` → `=` → `*2` gives `2+3*2 = 8`, not `10`. To continue from the result, press the `Ans` key first.

### 10.3 State transitions

```text
               ┌──────────────── AC / Esc ────────────────┐
               ▼                                          │
        ┌────────────┐   type / click    ┌────────────┐   │
  ───►  │   EDITING  │ ───────────────►  │   EDITING  │───┤
        │ (justEval  │                   │            │   │
        │   = false) │                   └─────┬──────┘   │
        └────────────┘                         │ "=" / Enter
                                               ▼
                          ┌─────────── doEvaluate() ───────────┐
                          │ success                    failure │
                          ▼                                    ▼
                 ┌──────────────────┐                ┌──────────────────┐
                 │  RESULT SHOWN    │                │   ERROR SHOWN    │
                 │ justEvaluated=   │                │ justEvaluated=   │
                 │ true, status OK  │                │ false, status =  │
                 │ ansValue updated │                │ error message    │
                 └──────────────────┘                └──────────────────┘
```

---

## 11. Build & Run Instructions

### 11.1 Windows (recommended: MSYS2 UCRT64)

```bash
# 1. Install MSYS2, then open the "MSYS2 UCRT64" terminal
# 2. Install toolchain + raylib
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-raylib make

# 3. Build (inside the project folder)
make

# 4. Run
./scientific_calculator.exe
```

### 11.2 Linux / Ubuntu

```bash
# Install a compiler, make, and X11 dev headers (needed for -lX11)
sudo apt update
sudo apt install build-essential libx11-dev

# Install raylib: from your package manager if available, or build from raylib.com / GitHub
# Then:
make
./scientific_calculator
```

### 11.3 CMake (any platform with raylib installed)

```bash
cmake -S . -B build
cmake --build build
./build/c_scientific_calculator          # Linux/macOS
.\build\c_scientific_calculator.exe      # Windows
```

### 11.4 Compiler flags used by the Makefile

| Flag | Meaning |
|---|---|
| `-std=c11` | C11 standard |
| `-O2` | Optimization level 2 |
| `-Wall -Wextra` | Extra warnings |

Override example: `make CFLAGS="-std=c11 -O0 -g -Wall -Wextra"` for a debug build.

### 11.5 Makefile targets

| Target | Action |
|---|---|
| `make` / `make all` | Compile `main.c` directly into the executable |
| `make clean` | Delete `scientific_calculator` and `scientific_calculator.exe` |

The Makefile has no separate object files and no `.PHONY` declaration (harmless here, since no files named `all` or `clean` exist).

### 11.6 System requirements

| Requirement | Detail |
|---|---|
| OS | Windows, Linux (macOS only via CMake) |
| Graphics | OpenGL 3.3-capable GPU/driver (raylib default) |
| Display | At least 820 px of vertical screen space |
| Network | None |
| Disk | A few MB for the binary |

---

## 12. Configuration Reference

All configuration is compile-time (constants in `main.c`):

| Constant | Value | Where | Effect |
|---|---|---|---|
| `SCREEN_W` | 470 | line 9 | Window width |
| `SCREEN_H` | 820 | line 10 | Window height |
| `MAX_EXPR` | 256 | line 11 | Max expression length (255 usable chars) |
| `MAX_TOKENS` | 256 | line 12 | Max tokens per expression and stack depth |
| `degreeMode` | `true` | line 42 | Starts in DEG mode |
| Target FPS | 60 | `main()` | Frame cap |
| Antialiasing | MSAA 4× | `main()` | Config flag |
| Display precision | `%.12g` | `doEvaluate()` | 12 significant digits |
| Near-zero snap | `1e-13` | `doEvaluate()` | Values below become `0` |
| Div-by-zero threshold | `1e-15` | `evalRPN()` | Divisor/`inv` guard |
| Factorial max | `170` | `factorial()` | `171!` overflows `double` |
| Factorial integer tolerance | `1e-10` | `factorial()` | How close to an integer |

**Runtime config files / env vars / CLI args:** none.

---

## 13. Supported Features & Syntax

### Features
- Fully clickable calculator UI + keyboard input
- Operators: `+  -  *  /  ^`, unary minus, factorial `!`
- Parentheses (nested)
- Trig: `sin cos tan asin acos atan` (DEG/RAD aware)
- `sqrt  ln  log  exp  abs`, reciprocal (`1/x` key → `inv(`)
- Constants: `pi`, `e`
- `Ans` — last successful result
- DEG / RAD toggle
- Error messages shown in the status line

### Example expressions (all verified — see §15)

```text
sin(30)+cos(60)      →  1
sqrt(144)+5^2        →  37
log(1000)            →  3
5!/(3!*2!)           →  10
Ans*2                →  (last result × 2)
```

---

## 14. Error Handling Reference

On error the LCD result shows `Error` and the status line shows the reason.

| Message | Cause |
|---|---|
| `Enter an expression` | Empty expression |
| `Too many tokens` | More than 256 tokens |
| `Bad number` | Number parse failure |
| `Unknown: <name>` | Unrecognized word |
| `Unexpected '<c>'` | Invalid character |
| `Mismatched parentheses` | Unbalanced `(` / `)` |
| `Missing operand` | Operator without enough values |
| `Missing function argument` | Function with nothing to apply to |
| `Division by zero` | `/ 0` or `inv(0)` |
| `Factorial domain error` | Negative, non-integer, or > 170 |
| `sqrt domain error` | `sqrt` of a negative |
| `ln domain error` / `log domain error` | Argument ≤ 0 |
| `Math error` | Non-finite result (`inf`/`NaN`, e.g. `asin(2)`) |
| `Invalid expression` | Leftover values on the stack (e.g. `2pi`, `5!-3`) |

---

## 15. Verified Test Results

The project ships **no tests**. To check my reading of the code, I compiled the engine portion (lines 1–231) on its own and ran these expressions:

| Expression | Result |
|---|---|
| `sin(30)+cos(60)` | `1` ✅ |
| `sqrt(144)+5^2` | `37` ✅ |
| `log(1000)` | `3` ✅ |
| `5!/(3!*2!)` | `10` ✅ |
| `2^3^2` | `512` ✅ (right-associative) |
| `0!` | `1` ✅ |
| `sin 30` (no parens) | `0.5` ✅ works |
| `abs(-4)` | `4` ✅ |
| `inv(4)` | `0.25` ✅ |
| `1e3+1` | `1001` (scientific notation parses via `strtod`) |
| `1/0` | Error: `Division by zero` ✅ |
| `(1+2` | Error: `Mismatched parentheses` ✅ |
| `171!` | Error: `Factorial domain error` ✅ |
| `asin(2)` | Error: `Math error` ✅ |
| `-2^2` | `4` ⚠️ (see limitation #3) |
| `5!-3` | Error: `Invalid expression` ❌ (bug, see #2) |
| `2pi` | Error: `Invalid expression` (no implicit multiply) |
| `2(3)` | Error: `Invalid expression` (no implicit multiply) |

> I did not run the GUI itself (it needs a display), so GUI-related items below are based on reading the code.

---

## 16. Known Issues & Limitations

Ordered roughly by importance.

| # | Issue | Details | Suggested fix |
|---|---|---|---|
| 1 | **`Esc` likely closes the app** | raylib's default exit key is `Esc`, and the code never calls `SetExitKey(KEY_NULL)`. So `WindowShouldClose()` probably returns true on `Esc` before/while `clearAll()` runs. README says "Esc: clear". *(Inferred from code; GUI not run.)* | Call `SetExitKey(KEY_NULL);` after `InitWindow` |
| 2 | **Subtraction after factorial fails** | `5!-3` → `Invalid expression`. After `!`, the tokenizer's `prev` is `TOK_OP`, so the `-` is wrongly treated as unary. | Track the previous token as "value-like" after `!` so `-` is binary |
| 3 | **`-2^2 = 4`, not `-4`** | Unary minus (prec 5) binds tighter than `^` (prec 4). Many calculators/math conventions give `-4`. | Give `u-` lower precedence than `^` |
| 4 | **No implicit multiplication** | `2pi`, `2(3)`, `3sin(30)` all error. | Insert `*` in the tokenizer between value-like and value-start tokens |
| 5 | **Operator after `=` appends to the old expression** | `2+3`, `=`, `*2` → `2+3*2 = 8` (not `10`) | If `justEvaluated` and the input is an operator, start with `Ans` |
| 6 | **Keyboard can't type letters or `!`** | `sin`, `pi`, `e`, `Ans`, `!` require clicking. Contradicts the spirit of the README examples. | Allow letters and `!` in the `GetCharPressed()` filter |
| 7 | **SHIFT and ALPHA do nothing** | They only show a status hint. Inverse trig keys are already on the main grid. | Implement a shift layer, or remove the keys |
| 8 | **`+/-` just appends `-`** | It doesn't toggle the sign of the current number. | Implement real sign toggling |
| 9 | **raylib version mismatch** | CMake requires 5.0, but `DrawRectangleRoundedLinesEx` likely needs a newer release (see §3). | Raise the CMake minimum version, or use `DrawRectangleRoundedLines` |
| 10 | **Makefile is Windows/Linux only** | No macOS branch (needs `-framework Cocoa -framework IOKit -framework OpenGL`). | Add a `uname -s` check |
| 11 | **Minor compiler warning** | `keyLight` is declared but never used → `-Wunused-variable`. `kind == 4` (accent) is also never used. | Remove unused items |
| 12 | **No tests, no CI** | | Add engine unit tests (see §17) |
| 13 | **No history / memory keys** | Only a single `Ans` value. No M+/M−, no history list. | Future feature |
| 14 | **Fixed window** | 470×820 and not resizable; may not fit small laptop screens. | Make it resizable or scale the UI |

---

## 17. How to Extend the Project

### Add a new function (e.g. `cbrt`)
1. `isFunctionName()` (line 65): add `"cbrt"` to the array.
2. `evalRPN()` function chain (line ~210): add
   ```c
   else if (strcmp(t.text, "cbrt") == 0) r = cbrt(x);
   ```
3. `labels[][3]` table in `main()`: add a button `{"cbrt","cbrt(","f"}` (and increase the grid / the `buttons[48]` capacity if needed).

### Add a new constant (e.g. `phi`)
In `tokenize()` (line ~88), add another `else if (strcmp(name, "phi") == 0)` branch that creates a `TOK_NUMBER`.

### Add a new binary operator (e.g. `%` modulo)
1. Add `%` to the `strchr("+-*/^!", ...)` set in `tokenize()`.
2. Add it to `precedence()` (same level as `*` and `/`).
3. Handle it in the binary-operator block of `evalRPN()` with `fmod(a, b)`.

### Suggested refactor (to make it testable)
```text
src/
├── engine.c / engine.h    # tokenize, toRPN, evalRPN, evaluateExpression
├── ui.c / ui.h            # drawing + buttons
├── main.c                 # window + loop
tests/
└── test_engine.c          # plain asserts, no raylib required
```
Pass `degreeMode` and `ansValue` into the engine as parameters instead of reading globals.

### Suggested test plan
Use the table in §15 as the first test cases. Add cases for each fix in §16.

---

## 18. Git / GitHub Setup

**`.gitignore` contents**
```text
build/
*.exe
scientific_calculator
.vscode/
.DS_Store
```

**Suggested repository name:** `c-scientific-calculator`

**Suggested topics:** `c`, `raylib`, `calculator`, `scientific-calculator`, `gui`, `desktop-app`, `expression-parser`

**Quick start**
```bash
git init
git add .
git commit -m "Initial commit: C scientific calculator (raylib)"
git branch -M main
git remote add origin https://github.com/<your-username>/c-scientific-calculator.git
git push -u origin main
```

**Suggested additions**
- `LICENSE` (none is present, so decide one, e.g. MIT)
- Screenshot in the README
- GitHub Actions workflow to build on Linux and Windows
- A `tests/` folder (see §17)

---

*End of documentation.*
