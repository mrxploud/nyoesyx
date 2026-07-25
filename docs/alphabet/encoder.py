import sys
import re

# V3 (Legacy) to V5 (1-Token ASCII Backend) Mapping
V3_TO_V5 = {
    "mem.set": "set",
    "mem.get": "get",
    "mem.embed": "embed",
    "mem.recall": "find",
    "mem.sync": "sync",
    "cog.inf": "inf",
    "cog.daemon": "daemon",
    "log.txn": "log",
    "ue.log": "ulg",
    "ue.spawn": "spawn",
    "ue.destroy": "destroy",
    "ue.move": "mov",
    "ue.anim": "anim",
    "ue.fx": "fx",
    "ue.sound": "sound",
    "none": "_",
    "$": "$"
}

# V5 (Backend) to V4 (Visual Skin) Mapping
V5_TO_V4 = {
    "set": "Ṡ",
    "get": "Ġ",
    "embed": "Ë",
    "find": "Ṙ",
    "sync": "Ÿ",
    "inf": "Ĩ",
    "daemon": "Ḋ",
    "log": "Ṫ",
    "ulg": "Ĺ",
    "spawn": "Ṗ",
    "destroy": "Ẍ",
    "mov": "Ṁ",
    "anim": "Ä",
    "fx": "Ḟ",
    "sound": "Ŝ",
    "_": "Ø",
    "$": "‡"
}

V4_TO_V5 = {v: k for k, v in V5_TO_V4.items()}

def encode_v5(text):
    """Converts a V3 DTP .nesx string into the V5 backend format."""
    encoded = text
    for k, v in V3_TO_V5.items():
        if k == "$": continue
        encoded = re.sub(r'\b' + re.escape(k) + r'\b', v, encoded)
    return encoded

def v5_to_v4_ui(text):
    """Translates V5 backend ASCII to V4 Unicode for the Dashboard skin."""
    rendered = text
    rendered = rendered.replace(" $ ", " ‡ ")
    for k, v in V5_TO_V4.items():
        if k == "$": continue
        rendered = re.sub(r'\b' + re.escape(k) + r'\b', v, rendered)
    return rendered

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python encoder.py [to_v5|to_v4_ui] <file>")
        sys.exit(1)

    mode = sys.argv[1]
    filepath = sys.argv[2]

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    if mode == "to_v5":
        result = encode_v5(content)
        out_path = filepath.replace(".nesx", ".v5.nesx")
    elif mode == "to_v4_ui":
        result = v5_to_v4_ui(content)
        out_path = filepath.replace(".v5.nesx", ".v4ui.txt")
    else:
        print("Invalid mode. Use 'to_v5' or 'to_v4_ui'.")
        sys.exit(1)

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(result)
    
    print(f"Successfully processed {mode}. Output written to {out_path}")
