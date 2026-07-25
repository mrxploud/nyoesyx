#!/usr/bin/env python3
# token_probe.py — the measuring tool for the "Font" (AI-native alphabet) track.
#
# An LLM never sees fonts or glyphs — it sees TOKEN IDS. So the real "alphabet of
# an AI" is its tokenizer's vocabulary: the set of byte-sequences that already map
# to ONE token. Designing an AI-native script is therefore NOT inventing new
# symbols (that is provably WORSE — see measure below), it is CURATING the symbols
# that are already 1 token under real tokenizers.
#
# This tool scores any candidate string/keyword/operator in tokens, across the
# tokenizers we can run locally (tiktoken's GPT-2/GPT-4o byte-level BPE). Claude
# and Llama use the same byte-level-BPE principle, so the RANKING is portable even
# though the exact counts differ per model. Run it before adopting any new syntax.
#
#   python Font/token_probe.py            # run the built-in evidence report
#   python Font/token_probe.py "your str" # score an arbitrary string
#
# Dep: pip install tiktoken
import sys, io
if sys.stdout.encoding and sys.stdout.encoding.lower() != "utf-8":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

try:
    import tiktoken
except ImportError:
    print("need tiktoken: pip install tiktoken"); sys.exit(1)

ENCODERS = {}
for name in ("o200k_base", "cl100k_base", "gpt2"):
    try: ENCODERS[name] = tiktoken.get_encoding(name)
    except Exception: pass

def toks(s, enc="o200k_base"):
    return len(ENCODERS[enc].encode(s))

def score_all(s):
    return {name: len(e.encode(s)) for name, e in ENCODERS.items()}

def is_single_token(s, enc="o200k_base"):
    """True if the whole string is exactly ONE token — the gold standard for a
    NYoesyx keyword/operator. These are the only symbols worth putting in syntax."""
    return toks(s, enc) == 1

def report():
    ref = "o200k_base"
    print(f"== token_probe — reference encoder: {ref} (GPT-4o) ==")
    print(f"   available locally: {', '.join(ENCODERS)}\n")

    print("1) THE CLAIM UNDER TEST: an invented 'AI alphabet' saves tokens.  VERDICT: FALSE.")
    normal = "fn add | log.txn + @arg0 @arg1"
    glyphs = "ƒη ⊕ ⟦ λτχ ☹ ＠０ ＠１ ⟧"
    print(f"   ascii NYoesyx   : {toks(normal):3d} tok ({len(normal)} chars)")
    print(f"   invented glyphs: {toks(glyphs):3d} tok ({len(glyphs)} chars)  <-- fewer chars, MORE tokens\n")

    print("2) WHY: cost per single character by script family (it is the tokenizer, not the look):")
    families = [(" function (ascii word)", " function"), ("a (ascii)", "a"),
                ("common CJK", "人"), ("rare CJK", "鱲"),
                ("emoji", "\U0001f9e0"), ("private-use", ""),
                ("math symbol", "☹"), ("greek", "η")]
    for label, ch in families:
        print(f"   {label:24s}: {toks(ch)} tok / {len(ch.encode('utf-8'))} bytes")
    print("   -> a whole common WORD is 1 token; an exotic GLYPH is 2-3. New scripts lose.\n")

    print("3) WHAT ACTUALLY WINS: dense syntax built from already-cheap (1-token) ASCII:")
    py = "def add(a, b):\n    return a + b\n"
    nx = "fn add | + @0 @1"
    print(f"   python: {toks(py)} tok   nyoesyx: {toks(nx)} tok   ({100*(toks(py)-toks(nx))//toks(py)}% fewer)\n")

    print("4) NYoesyx keyword audit — are our current ops 1 token each? (the real 'font' work)")
    for kw in ["fn", "call", "branch", "use", "log.txn", "mem.set", "cog.inf",
               "assert", "@arg", "@arg0", "none", "$", "|", "+", "-", "*", "/"]:
        marks = "/".join(str(score_all(kw)[e]) for e in ENCODERS)
        flag = "" if is_single_token(kw) else "   <-- multi-token, candidate to redesign"
        print(f"   {kw:9s}: {marks} tok ({'/'.join(ENCODERS)}){flag}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        s = " ".join(sys.argv[1:])
        print(f"{s!r}")
        for name, n in score_all(s).items():
            print(f"  {name:12s}: {n} tok")
    else:
        report()
