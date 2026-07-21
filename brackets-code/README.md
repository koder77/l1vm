# Brackets-Code Generator

A CLI tool for generating Brackets (L1VM) code from natural language prompts.

## Installation

```bash
# Clone and build
git clone https://github.com/your-org/brackets-code.git
cd brackets-code
make

# Or with Makefile
./makefile
```

## Usage

```bash
# Interactive mode
brackets-code

# One-shot mode
brackets-code "your prompt here"

# With validation (requires l1pre/l1com)
brackets-code --validate "your prompt here"

# Self-test
brackets-code --self-test

# Batch mode
brackets-code --batch prompts.txt

# Vector search
brackets-code --search "your query"

# Learn new pattern
brackets-code --learn my_code.l1com "keyword1" "keyword2" "description"
```

## Flags

### Output Options
- `--output <dir>`: Output directory for generated files
- `--dry-run`: Print filename only, no output
- `--verbose`: Show emitter selection scores

### Validation
- `--validate <prompt>`: Run l1pre preprocessing and l1com compilation
- `--l1vm-root <path>`: L1VM installation root

## Template Examples

```bash
brackets-code "Sum of 1 to 100"
```

Generates:
```c
func sum() {
    int total = 0;
    for (int i = 1; i <= 100; i++) {
        total += i;
    }
    return total;
}
```

## Performance

Benchmark target: 197 code generation blocks → 12-15 seconds

Current optimizations:
- Template matching with keyword counting
- Early termination for simple prompts
- Vector search integration
- Learned pattern caching

## License

GPLv3 or later

## Erweitertes .l1dsl Dateiformat

Die `.l1dsl` Dateien sind deklarative Regeln, die natürliche Sprache in L1VM-Code übersetzen.
Neben den grundlegenden Direktiven (`parser:`, `token:`, `code:` etc.) stehen 13 erweiterte
Direktiven zur Verfügung, um mächtigere und intelligentere Regeln zu erstellen.

### Übersicht aller Direktiven

#### Grundlegende Direktiven (bestehend)

| Direktiv | Beschreibung | Beispiel |
|---|---|---|
| `parser:` | Keywords für Prompt-Matching (Kommagetrennt) | `parser: "fibonacci, fib, fib sequence"` |
| `token:` | Eingabe-Variablen mit Typ | `token: int64 n, double x` |
| `result:` | Ausgabe-Variabel | `result: double y` |
| `include:` | L1VM Header-Datei (pre-processing) | `include: intr-func.l1h` |
| `include-post:` | Header nach den Haupt-Includes | `include-post: math-lib.l1h` |
| `var:` | Explizite Variablen-Deklaration | `var: int64 myvar 1 42` |
| `desc:` | Menschliche Beschreibung | `desc: "Berechnet Fibonacci-Zahl"` |
| `match:` | TaskProfile-Flags für exaktes Matching | `match: has_fib_seq` |
| `array:` | Array-Regel-Deklaration | `array: arr i int64` |
| `code:` | Beginn des Code-Blocks | `code:` |

#### Neue erweiterte Direktiven

| Direktiv | Beschreibung | Beispiel |
|---|---|---|
| `param:` | Reichhaltige Parameter mit Validierung | `param: int64 n "Anzahl" min=1 max=100 default=10` |
| `require:` | Externe Abhängigkeit (semantisch stark) | `require: math-lib.l1h` |
| `example:` | Beispiel-Prompts | `example: "fibonacci 10"` |
| `category:` | Hierarchische Kategorisierung | `category: math > sequences` |
| `version:` | Versions-Tracking | `version: 2.0.0` |
| `complexity:` | Komplexitätsstufe | `complexity: simple` |
| `alias:` | Zusätzliche Aliase | `alias: "fibo, fib seq"` |
| `test:` | Eingebaute Testfälle | `test: "fibonacci 10" expect: "55"` |
| `help:` | Erweiterter Hilfe-Text | `help: "Berechnet die n-te Fibonacci-Zahl"` |
| `validate:` | Validierungsregeln | `validate: no_division_by_zero` |
| `compose:` | Regel-Zusammensetzung | `compose: base-rule, print-rule` |
| `init:` | Initialisierungs-Code-Block | `init:` (vor Hauptcode) |
| `cleanup:` | Aufräum-Code-Block | `cleanup:` (nach Hauptcode) |

---

### Vollständiges Beispiel: fibonacci-smart.l1dsl

```l1dsl
// fibonacci-smart.l1dsl
parser: "fibonacci, fib, fib sequence"
alias: "fibo, fib seq, fibonacci number"
desc: "Berechnet die n-te Fibonacci-Zahl iterativ"
category: math > sequences
version: 2.0.0
complexity: simple
help: "Berechnet die n-te Fibonacci-Zahl iterativ. Parameter n gibt die Anzahl der Iterationen an."
match: has_fib_seq
param: int64 n "Anzahl der Iterationen" min=1 max=100 default=10
token: int64 n
result: int64 fib
include: intr-func.l1h
example: "fibonacci 10"
example: "fibonacci berechne 20"
test: "fibonacci 10" expect: "55"

init:
    (set const-int64 1 zero 0)
    (set const-int64 1 one 1)

code:
    (set int64 1 a 0)
    (set int64 1 b 1)
    (set int64 1 i 2)
    (set int64 1 c 0)
    (set int64 1 f 0)
    (for-loop)
    (((i n <=) f :=) f for)
        (a + b c :=)
        (b a :=)
        (c b :=)
        (i + one i :=)
    (next)
    (b :print_i !)
    (:print_n !)

cleanup:
    (zero :exit !)
```

---

### Erklärung der neuen Direktiven

#### `param:` — Reichhaltige Parameter-Definition

Erweitert `token:` um Metadaten für Validierung und Benutzerführung.

```l1dsl
param: int64 n "Anzahl der Iterationen" min=1 max=1000 default=10
param: string name "Ihr Name" default="Welt" required
```

**Syntax:** `param: type name [desc="..."] [min=X] [max=X] [default=X] [pattern=regex] [required]`

| Attribut | Beschreibung |
|---|---|
| `desc=` | Beschreibung des Parameters |
| `min=` | Minimaler Wert (int64/double) |
| `max=` | Maximaler Wert (int64/double) |
| `default=` | Standardwert |
| `pattern=` | Regex-Pattern (string) |
| `required` | Parameter ist obligatorisch |

---

#### `require:` — Externe Abhängigkeit

Wie `include:`, aber mit semantischer Bedeutung: Diese Datei ist zwingend erforderlich.

```l1dsl
require: math-lib.l1h
require: fann-lib.l1h
```

---

#### `example:` — Beispiel-Prompts

Definiert Beispiel-Prompts, die diese Regel auslösen sollten. Nützlich für
Dokumentation und zukünftiges Matching.

```l1dsl
example: "fibonacci 10"
example: "berechne fibonacci für 20"
example: "fib sequence 50"
```

---

#### `category:` — Hierarchische Kategorisierung

Ordnet die Regel einer Kategorie mit `>`-Separatoren zu.

```l1dsl
category: math > sequences
category: string > manipulation
category: data > sorting
```

---

#### `version:` — Versions-Tracking

Semantic Versioning für Regeln.

```l1dsl
version: 2.1.0
```

---

#### `complexity:` — Komplexitätsstufe

Hilft dem System, die passende Regel auszuwählen.

```l1dsl
complexity: simple     # Einfache Logik
complexity: medium     # Mittelkomplex
complexity: complex    # Hochkomplex
```

---

#### `alias:` — Zusätzliche Aliase

Neben `parser:` für weiteres Matching.

```l1dsl
alias: "fibo, fib seq, fibonacci number, fibonacci-folge"
```

---

#### `test:` — Eingebaute Testfälle

Testfälle direkt in der Regel definieren.

```l1dsl
test: "fibonacci 10" expect: "55"
test: "fibonacci 1" expect: "1"
test: "fibonacci 5" expect: "5"
```

---

#### `help:` — Erweiterter Hilfe-Text

Ausführliche Hilfe, die bei `--help` oder in der Interaktiven Shell angezeigt wird.

```l1dsl
help: "Berechnet die n-te Fibonacci-Zahl iterativ.\nVerwendung: fibonacci <zahl>\nBeispiel: fibonacci 10"
```

---

#### `validate:` — Validierungsregeln

Named Validierungs-Checks für den generierten Code.

```l1dsl
validate: no_division_by_zero, has_bounds_check
```

---

#### `compose:` — Regel-Zusammensetzung

Aktuelle Regel erweitert andere Regeln.

```l1dsl
compose: base-fibonacci, print-result
```

---

#### `init:` — Initialisierungs-Code

Code-Block, der **vor** dem Hauptcode (`code:`) ausgeführt wird.

```l1dsl
init:
    (set const-int64 1 zero 0)
    (set const-int64 1 one 1)
    (zero :math_init !)
```

Nützlich für:
- Konstanten definieren
- Bibliotheken initialisieren
- Ressourcen allozieren

---

#### `cleanup:` — Aufräum-Code

Code-Block, der **nach** dem Hauptcode (`code:`) ausgeführt wird.

```l1dsl
cleanup:
    (zero :exit !)
```

Nützlich für:
- Ressourcen freigeben
- Verbindungen schließen
- Programmaufräumung

---

### Verarbeitungsreihenfolge

Die Code-Generierung erfolgt in dieser Reihenfolge:

```
1. include: / require:     ← Header-Dateien
2. include-post:           ← Nach-Processing Headers
3. var:                    ← Variablen-Deklarationen
4. token: / param:         ← Parameter-Variablen
5. result:                 ← Ergebnis-Variabel
6. init:                   ← Initialisierungscode
7. code:                   ← Hauptcode
8. cleanup:                ← Aufräumcode
```

---

### Rückwärtskompatibilität

Alle 162 bestehenden `.l1dsl` Files in `dsl/` bleiben **vollständig unverändert**.
Die neuen Direktiven sind optional — bestehende Regeln funktionieren weiterhin genau wie zuvor.
