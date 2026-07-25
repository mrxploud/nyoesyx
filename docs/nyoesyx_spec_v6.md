# NYoesyx DTP Specification v6.0 (High Density Sandbox)

## Objetivo
O objetivo da V6 é elevar a compressão semântica para LLMs ao extremo (almejando 90-95%+ de redução em relação a formatos padrão), usando Contexto Implícito, Omissão de Parâmetros e Macro-Opcodes.

## Mudanças em relação a V5

### 1. Parâmetros Implícitos
Se um nó for sequencial (dependência = nó anterior ou `none`) e tiver custo `0.0` (sem custo especulativo), esses parâmetros **NÃO** devem ser impressos.
O kernel `compiler.nesx` preencherá as lacunas em tempo de compilação.

**Sintaxe V5:**
`ue.move 0.0 none $ npc_guard 100 0 200`
**Sintaxe V6 (Linear):**
`ue.move npc_guard 100 0 200`

Se for necessário especificar dependência, usamos o separador `$`:
**Sintaxe V6 (Com Dependência):**
`ue.move 0.1 1 $ npc_guard 100 0 200`

### 2. Action Vectors (Macro-Opcodes Nativos)
Ações compostas que seriam fragmentadas em múltiplas linhas (por exemplo, pensar, registrar e mover) agora podem ser colapsadas em um Opcode Complexo. A responsabilidade de desdobrar as micro-ações fica a cargo do Hypervisor.

**V5 (4 Linhas / Alta Verbosidade):**
```text
mem.set 0.0 none $ npc_guard idle_patrol
cog.inf 0.5 0 $ gemini-2.0-flash 0.3 decide_npc_action_for guard_patrolling_area
ue.move 0.1 1 $ npc_guard 100 0 200
ue.anim 0.0 3 $ npc_guard walk_patrol
```

**V6 (Action Vector - Apenas a Intenção Final):**
```text
!npc_rout guard patrol
```
*O compilador V6 lê o prefixo `!` como um "Action Vector de IA" e o kernel nativo executa as 4 micro-ações em C++.*

### 3. BPE Hacking & Prefixos (Abreviações de Subsistemas)
Subsistemas frequentes são abreviados para 1 caractere especial para que o tokenizer consolide melhor:
* `ue.*` (Unreal Engine) vira `>`
* `mem.*` (Memória) vira `*`
* `cog.*` (Cognitivo) vira `?`

**Exemplo Prático (Abreviação Extrema):**
V5: `ue.move 0.0 none $ npc_guard 100 0 200`
V6: `>move npc_guard 100 0 200`
