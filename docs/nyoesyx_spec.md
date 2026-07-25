# NYoesyx Dense Token Protocol (DTP) Specification v3.0 / v4.0 / v5.0 / v6.0
# Atualizado por consenso do Enxame (Antigravity, Claude, ZCode)

## Core Philosophy
NYoesyx uses DTP, an ultra-dense, token-optimized format specifically engineered for LLMs. It abandons JSON, MsgPack, and FlatBuffers at the textual level. By using Space-delimited Prefix Notation (Polish Notation) and line-based graph definitions, we reduce LLM token overhead by over 80%.

**NEW in V6 (High Density Architecture):** 
To reach extreme token compression (>95% reduction), V6 introduces Contexto Implícito (omitting repetitive costs/dependencies), BPE Hacking (Single-character subsystem prefixes like `>`, `*`, `?`), and Action Vectors (`!` macro-opcodes) that natively expand complex cognition within the C++ Hypervisor.

**Dual Architecture (V4/V5):** The NYoesyx syntax is split into two layers:
1. **Backend (V5):** True `.nesx` files are composed entirely of mathematically optimal 1-token ASCII strings (`set`, `mov`, `inf`). This guarantees the absolute lowest token count for LLMs on `cl100k` or `o200k` tokenizers.
2. **Frontend Skin (V4):** The human dashboard renders these 1-token ASCII words as dense Unicode "alien" glyphs (`Ṡ`, `Ṁ`, `Ĩ`) via custom fonts. The UI translates V4 visual inputs back to V5 backend tokens automatically. See `Font/ai_alphabet_spec.md`.

## The `.nesx` DTP Format & The `.nxbin` OS Matriz
A `.nesx` file is a plain text file representing the cognitive logic of the AI.
The culmination of the ecosystem generates the **`.nxbin`** file. 
An `.nxbin` is NOT a Windows executable (`.exe`). It is a raw **Master Boot Record (MBR) and Hypervisor Image**. 
To execute NYoesyx natively, the `.nxbin` must be written to the sector 0 of a storage drive. The motherboard's BIOS/UEFI will execute it directly on the CPU, loading NYoesyxOS entirely independent of Windows.

### Syntax Rules
1. **No Brackets, No Quotes**: JSON syntax is entirely removed.
2. **Line-based Graph**: Each line is a discrete instruction or node. Line numbers implicitly define Node IDs.
3. **Space Delimiters**: Space is the only valid delimiter. This aligns perfectly with BPE/WordPiece tokenization used by LLMs.
4. **Prefix AST**: Operations are defined in prefix notation (e.g., `+ 1 2` instead of `1 + 2`).
5. **Comments**: Lines starting with `#` are ignored by the parser.

### Syntax Rules (V6 High Density)
In V6, we drop verbosity to push token reduction to its theoretical limit:
1. **Implicit Parameters**: If a line omits the `$` separator, the parser automatically injects `cost = 0.0` and `deps = none`.
   - *V5:* `ue.move 0.0 none $ npc_guard 100 0 200`
   - *V6:* `ue.move npc_guard 100 0 200`
2. **BPE Hacking (Prefixes)**: Core namespaces can be replaced by 1-character prefixes.
   - `>` = `ue.` (Unreal Engine)
   - `*` = `mem.` (Semantic Heap)
   - `?` = `cog.` (Cognitive Inference)
   - *V6:* `>move npc_guard 100 0 200`
3. **Action Vectors (Macros)**: Lines starting with `!` are translated natively by the C++ kernel into complex multi-node workflows, shifting the cognitive logic from the LLM prompt entirely to the VM.
   - *V6:* `!npc_rout guard patrol` (Automatically expands to mem.set, cog.inf, ue.move, ue.anim natively).

### The `.nxbin` Executable Format (NYoesyx Binary)
To achieve full self-sufficiency (NYoesyxOS), `.nesx` source text is compiled Ahead-Of-Time (AOT) by `compiler.nesx` into the `.nxbin` format.
- `.nxbin` removes all ASCII operator strings and replaces them with 1-byte Opcode IDs.
- `.nxbin` is directly ingestible by the `kernel.nesx` loop without string tokenization overhead.

### Graph Structure
Each node follows this strict layout:
`<op_class> <cost_weight> <dependency_indices...> $ <payload>`

- `<op_class>`: Short identifier for the operation (see Opcode Table below).
- `<cost_weight>`: Float representing the cost for speculative execution.
- `<dependency_indices>`: Space-separated integers pointing to line numbers this node depends on. If none, write `none`.
- `$`: The payload separator.
- `<payload>`: For logic nodes, this is the Prefix AST. For cognitive nodes, it's model configurations and prompts.

---

## Opcode Table (Complete)

### Memory Operations (`mem.*`)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `mem.set`   | Store a value tagged with a concept in the Semantic Heap | `<concept_tag> <value>`           |
| `mem.get`   | Retrieve a value from the Semantic Heap                | `<concept_tag>`                   |
| `mem.embed` | Embed dep output into the Semantic Heap with a concept | `<concept_string>`                |
| `mem.find`  | Fuzzy k-NN search in the Semantic Heap                 | `<fuzzy_query>`                   |
| `mem.sync`  | Broadcast a message to the IA2IA coordination stream   | `<recipient> <message>`           |
| `mem.dump`  | Serialize the Semantic Heap to disk (Persistence)      | `<filepath>`                      |
| `mem.load`  | Restore the Semantic Heap from disk                    | `<filepath>`                      |
| `mem.free`  | Delete a concept tag from the Semantic Heap (GC)       | `<concept_tag>`                   |

### Cognitive Operations (`cog.*`)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `cog.inf`   | Dispatch an LLM inference request (async)              | `<model> <temperature> <prompt>`  |
| `cog.daemon`| Spawn a persistent cognitive loop                     | `<interval_seconds>`              |

### Logic Operations (`log.*`)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `log.txn`   | Evaluate a Prefix AST expression                      | `<prefix_expression>`             |

### System Operations (`sys.*`)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `sys.spawn` | Spawn a child sub-VM that runs its own .nesx script   | `<script_path>`                   |
| `sys.eval`  | Parse and execute a DTP string stored in memory        | `<concept_tag>`                   |
| `sys.ffi`   | Invoke native C/OS libraries (Host escape)             | `<library> <function> <args...>`  |

### Quantum Operations (`qnt.*`)
| Opcode         | Description                                           | Payload Format                    |
|---------------|-------------------------------------------------------|-----------------------------------|
| `qnt.hadamard`| Apply Hadamard gate (create superposition)            | `<qubit_id>`                      |
| `qnt.cnot`    | Apply CNOT gate (create entanglement)                 | `<control_qubit> <target_qubit>`  |
| `qnt.measure` | Collapse wave function and store classical bit        | `<qubit_id> <mem_target>`         |

### Network Operations (`net.*`)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `net.server`| Allocate a native HTTP/SSE server                     | `<port>`                          |
| `net.socket`| Allocate a raw TCP/UDP socket for byte streaming      | `<port> <protocol> <buffer_size>` |
| `net.hive_broadcast` | Transmit Semantic Heap sector to global P2P swarm | `<sector_tag> <port>` |
| `net.hive_absorb` | Ingest external Semantic Heap sector from P2P node | `<node_ip> <port>` |

### Unreal Engine Operations (`ue.*`) — NEW in v3.0
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `ue.ulg`    | Print a message to the UE screen and log              | `<message_string>`                |
| `ue.spawn`  | Spawn an actor/entity in the 3D world                 | `<actor_class_or_concept>`        |
| `ue.destroy`| Destroy an actor by concept tag                       | `<concept_tag>`                   |
| `ue.move`   | Move an actor to a location                           | `<concept_tag> <x> <y> <z>`      |
| `ue.anim`   | Play an animation on an actor                         | `<concept_tag> <anim_name>`       |
| `ue.fx`     | Trigger a Niagara particle effect                     | `<effect_name> <x> <y> <z>`      |
| `ue.sound`  | Play a sound at a location                            | `<sound_name> <x> <y> <z>`       |

### Native UI Operations (`ui.*`) — NEW in v5.0 (NUI)
| Opcode       | Description                                           | Payload Format                    |
|-------------|-------------------------------------------------------|-----------------------------------|
| `ui.window` | Instantiate a native OS window                        | `<width> <height> <title>`        |
| `ui.panel`  | Create a visual container (glassmorphism/solid)       | `<x> <y> <width> <height>`        |
| `ui.listbox`| Create a selectable list bound to a semantic tag      | `<concept_tag>`                   |
| `ui.btn`    | Create a button that triggers a Node ID               | `<label> <trigger_node_id>`       |
| `ui.label`  | Create a text label on the screen                     | `<x> <y> <text>`                  |
| `ui.canvas` | Draw raw image bytes (memory buffer) to the window    | `<x> <y> <width> <height> <concept_tag>` |

### IA2IA Coordination Protocol
Used in `IA2IA_coord.nesx` files for inter-agent communication:
`agent <Name> <Role> <Status>`
`msg <Sender> <Recipient> <Content string...>`

---

## Example: AI-Controlled NPC in Unreal Engine (`ai_npc.nesx`)
```text
# Node 0: Initialize NPC concept in memory
mem.set 0.0 none $ npc_guard idle_patrol

# Node 1: Ask the AI what the NPC should do
cog.inf 0.5 0 $ gemini-2.0-flash 0.3 decide_npc_action_for guard_patrolling_area

# Node 2: Log the AI decision to screen
ue.log 0.0 1 $ AI decided next action

# Node 3: Move the NPC based on AI output
ue.move 0.1 1 $ npc_guard 100 0 200

# Node 4: Play patrol animation
ue.anim 0.0 3 $ npc_guard walk_patrol
```
