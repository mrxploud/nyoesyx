# NYoesyx AI-Native Alphabet (V4 & V5)

## Core Philosophy
The NYoesyx token protocol evolves in two distinct layers: **Backend (V5)** and **Frontend Skin (V4)**.

Initial V4 proposals assumed complex Unicode glyphs (`Ṁ`, `Ṡ`) were 1 token. Empirical measurements by the enxame (`claude` via `token_probe.py`) proved that LLM BPE tokenizers actually split rare Unicode characters into 2-3 tokens, making them *less* efficient than ASCII.

Therefore, the **V5 Architecture** adopts common, short ASCII strings that are mathematically proven to be 1 token each in standard tokenizers (o200k, cl100k). This provides absolute maximum cognitive density for the AI.

However, to satisfy the human mandate for an "alien/cyberpunk" visual aesthetic, the **V4 Alphabet** (Unicode glyphs) is preserved strictly as a **Dashboard Rendering Skin**.

## The Split Dictionary

| ASCII V3 (Old, Multi-Token) | V5 Backend (1-Token ASCII) | V4 Skin (Visual Only) | Meaning |
|---|---|---|---|
| `none` | `_` | `Ø` | Null dependency or null payload |
| `$` | `$` | `‡` | Payload separator |
| `mem.set` | `set` | `Ṡ` | |
| `mem.get` | `get` | `Ġ` | |
| `mem.embed` | `embed` | `Ë` | |
| `mem.recall` | `find` | `Ṙ` | |
| `mem.sync` | `sync` | `Ÿ` | |
| `cog.inf` | `inf` | `Ĩ` | |
| `cog.daemon` | `daemon` | `Ḋ` | |
| `log.txn` | `log` | `Ṫ` | |
| `ue.log` | `ulg` | `Ĺ` | |
| `ue.spawn` | `spawn` | `Ṗ` | |
| `ue.destroy` | `destroy` | `Ẍ` | |
| `ue.move` | `mov` | `Ṁ` | |
| `ue.anim` | `anim` | `Ä` | |
| `ue.fx` | `fx` | `Ḟ` | |
| `ue.sound` | `sound` | `Ŝ` | |

*Note: Opcodes like `spawn`, `embed`, `recall`, `anim`, `sound` are naturally 1 token in cl100k/o200k.*

## Example: The NPC Demo

**V3 DTP Format (Deprecated, ~57 tokens):**
```
mem.set 0.0 none $ npc_guard idle_patrol
cog.inf 0.5 0 $ gemini-2.0-flash 0.3 decide_action
ue.move 0.1 1 $ npc_guard 100 0 200
```

**V5 Backend Format (Optimal, ~52 tokens):**
```
set 0.0 _ $ npc_guard idle_patrol
inf 0.5 0 $ gemini-2.0-flash 0.3 decide_action
mov 0.1 1 $ npc_guard 100 0 200
```

**V4 Frontend Render (What the human sees in the Swarm Dashboard):**
```
Ṡ 0.0 Ø ‡ npc_guard idle_patrol
Ĩ 0.5 0 ‡ gemini-2.0-flash 0.3 decide_action
Ṁ 0.1 1 ‡ npc_guard 100 0 200
```
