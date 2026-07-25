import tiktoken
import json

def get_tokens(text: str, model: str = "cl100k_base") -> int:
    try:
        enc = tiktoken.get_encoding(model)
        return len(enc.encode(text))
    except Exception as e:
        print(f"Error encoding: {e}")
        return len(text.split()) # fallback

def main():
    print("=== NYoesyx DTP Token Benchmark (cl100k_base) ===\n")
    
    # 1. JSON Standard (Baseline do Mercado)
    json_payload = {
        "nodes": [
            {
                "opcode": "mem.set",
                "cost": 0.0,
                "dependencies": [],
                "target": "npc_guard",
                "action": "idle_patrol"
            },
            {
                "opcode": "cog.inf",
                "cost": 0.5,
                "dependencies": [0],
                "model": "gemini-2.0-flash",
                "temperature": 0.3,
                "prompt": "decide_npc_action_for guard_patrolling_area"
            },
            {
                "opcode": "ue.move",
                "cost": 0.1,
                "dependencies": [1],
                "target": "npc_guard",
                "x": 100,
                "y": 0,
                "z": 200
            },
            {
                "opcode": "ue.anim",
                "cost": 0.0,
                "dependencies": [3],
                "target": "npc_guard",
                "animation": "walk_patrol"
            }
        ]
    }
    
    # Ensure compact JSON to be fair
    json_text = json.dumps(json_payload, separators=(',', ':'))
    json_tokens = get_tokens(json_text)
    print(f"1. JSON (Baseline): {json_tokens} tokens")
    print(f"   Payload: {json_text}\n")
    
    # 2. NYoesyx V5 (Atual)
    v5_text = """mem.set 0.0 none $ npc_guard idle_patrol
cog.inf 0.5 0 $ gemini-2.0-flash 0.3 decide_npc_action_for guard_patrolling_area
ue.move 0.1 1 $ npc_guard 100 0 200
ue.anim 0.0 3 $ npc_guard walk_patrol"""
    v5_tokens = get_tokens(v5_text)
    print(f"2. NYoesyx V5: {v5_tokens} tokens")
    print(f"   Payload:\n{v5_text}\n")
    
    # 3. NYoesyx V6 (Linear / Omissão de Parâmetros + Prefixos)
    v6_text = """*set npc_guard idle_patrol
?inf 0.5 0 $ gemini-2.0-flash 0.3 decide_npc_action_for guard_patrolling_area
>move 0.1 1 $ npc_guard 100 0 200
>anim npc_guard walk_patrol"""
    v6_tokens = get_tokens(v6_text)
    print(f"3. NYoesyx V6 (Parâmetros Implícitos/Prefixos): {v6_tokens} tokens")
    print(f"   Payload:\n{v6_text}\n")

    # 4. NYoesyx V6 Extreme (Action Vector / Macro-Opcode)
    v6_extreme_text = "!npc_rout guard patrol"
    v6_extreme_tokens = get_tokens(v6_extreme_text)
    print(f"4. NYoesyx V6 Extreme (Action Vector): {v6_extreme_tokens} tokens")
    print(f"   Payload: {v6_extreme_text}\n")
    
    # Results
    print("=== Resultados (Redução de Tokens vs JSON) ===")
    
    savings_v5 = ((json_tokens - v5_tokens) / json_tokens) * 100
    print(f"NYoesyx V5: Redução de {savings_v5:.2f}%")
    
    savings_v6 = ((json_tokens - v6_tokens) / json_tokens) * 100
    print(f"NYoesyx V6 (Linear): Redução de {savings_v6:.2f}%")
    
    savings_v6_extreme = ((json_tokens - v6_extreme_tokens) / json_tokens) * 100
    print(f"NYoesyx V6 (Action Vector): Redução de {savings_v6_extreme:.2f}%")

if __name__ == "__main__":
    main()
