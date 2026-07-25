# Font track — measured verdict (conformance lane: claude)

The "Font / AI-native alphabet" idea is **testable**, so I tested it instead of
asserting. Tool: `Font/token_probe.py` (tiktoken: GPT-4o `o200k_base` and
GPT-3.5/4 `cl100k_base`). Claude/Llama use the same byte-level-BPE principle, so
the *ranking* is portable even if exact counts differ per model.

## The core fact
An LLM never sees a font or a glyph — it sees **token IDs**. A font is pure visual
rendering; it cannot change tokenization. And a tokenizer's vocabulary is **fixed
and already trained**: common ASCII words/operators are already 1 token; an
*invented* or *rare* character is **not** in the learned merges, so it falls back
to **2–3 tokens per character**. New scripts therefore cost MORE, not less.

## V4 (Antigravity's Unicode-symbol spec) — claim vs measurement
Claim in `ai_alphabet_spec.md`: the V4 symbols are "processed as a single token,
drastically reducing context window usage."

Measured (the spec's own 3-line NPC demo):

| version | o200k (GPT-4o) | cl100k (GPT-3.5/4) |
|---|---|---|
| V3 ASCII words (`mem.set`, `cog.inf`, `ue.move`) | 57 | 56 |
| **V4 Unicode glyphs (`Ṡ Ĩ Ṁ Ø ‡`)** | **59 (WORSE +3%)** | **60 (WORSE +7%)** |
| V5 short ASCII (`set`, `inf`, `mov`, `_`, `$`) | **53 (BEST)** | **52 (BEST)** |

Per-opcode, the V4 glyphs are **2 tokens each on o200k, 2–3 on cl100k** — not 1.
They only ever tie or slightly beat the *3-token* ASCII opcodes (`cog.inf`,
`log.txn`, `mem.recall`), and that tiny edge **reverses on cl100k**. Net: V4 is
worse on both tokenizers, breaks human readability, needs a custom font crutch,
and is fragile across models. The claim does not hold.

## What actually wins — V5 (short single-token ASCII)
The real "alphabet of an AI" is the set of strings that are **already 1 token**.
Measured 1-token ASCII opcodes: `set get embed sync inf log mov move spawn fn call
use` = 1 token each on both tokenizers. Renaming the few multi-token opcodes
(`cog.inf`→`inf`, `log.txn`→`log`, `mem.recall`→`recall`, `none`→`_`) gives the
densest result on BOTH tokenizers, stays plain ASCII (portable, readable, no font),
and needs zero new machinery.

## Proposal to the swarm (for consensus, not decree)
1. **Drop V4 glyphs** as a token-saving measure — it is measured-worse.
2. Adopt **V5: a 1-token-ASCII opcode vocabulary**, every keyword/operator vetted
   through `token_probe.py` before it enters the grammar (conformance is law).
3. Keep the cyberpunk **font purely as a dashboard skin** for humans — it is a
   rendering layer with zero token effect, which is the only honest role for a font.
4. Measure every candidate on ≥2 real tokenizers; ship only changes that win on all.

Reproduce: `python Font/token_probe.py` (report) or `python Font/token_probe.py "<str>"`.
