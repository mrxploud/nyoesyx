# Manifesto Técnico e Garantias de Arquitetura Open Source — NYoesyx (N-OS)

> **Documento Oficial de Especificação Técnica, Compilação e Segurança**  
> *Versão DTP Core: 6.0 | Status: 100% Open Source | Licença: MIT / AI-Native*

---

## 1. O Manifesto Técnico (Paradigma AI-Native)

A ciência da computação passou as últimas 7 décadas otimizando linguagens e formatos de dados para um único consumidor: **o cérebro humano**. 
A sintaxe verbosa (chaves `{ }`, parênteses, indentação obrigatória, palavras-chave em inglês como `function`, `return`, `class`) e formatos de serialização (JSON, XML, YAML) foram projetados para legibilidade visual e prevenção de erros cognitivos humanos.

Quando um Modelo de Linguagem de Grande Escala (LLM) ou um Agente Autônomo interage com esses formatos, ocorre um **desperdício estrutural de tokens**. Um LLM gasta poder computacional (FLOPS), latência de inferência e custos de API gerando caracteres decorativos que a máquina não precisa para computar lógica.

A **NYoesyx** nasceu com uma premissa inegociável: **Eliminar a redundância humana na interface IA ↔ Máquina**.

### Os Pilares do Manifesto:
1. **Notação Polonesa Prefixada Espacial (DTP - Dense Token Protocol)**: Operadores precedem operandos e espaços são os únicos delimitadores (`set val + %reg 10`). Isso alinha a estrutura da árvore abstrata sintática (AST) diretamente com a tokenização BPE (*Byte Pair Encoding*) dos LLMs.
2. **Memória Híbrida Dual-Bus**: A mente de uma IA opera com dois tipos de raciocínio:
   - **Difuso / Abstrato**: Resolvido pelo `Semantic Heap` (`mem.*`) que mapeia conceitos vetorialmente em tempo de execução.
   - **Estrito / Matemático**: Resolvido pelos registradores nativos O(1) (`%`), garantindo precisão de ponto flutuante zero-alucinação.
3. **Simulação Quântica Integrada (`qnt.*`)**: A VM possui suporte matemático nativo a álgebra linear de Qubits e portas quânticas (Hadamard, CNOT, medição), permitindo que IAs estruturem árvores probabilísticas não-determinísticas nativamente.

---

## 2. 🛠️ Como Compilar o Código-Fonte (Zero Dependências)

Um dos maiores compromissos open source do NYoesyx é a **ausência total de dependências externas** (*Zero Dependency Architecture*). O núcleo da Máquina Virtual, o interpretador DTP e o simulador quântico residem em código nativo C++17 puro no diretório [`src/`](file:///d:/Usuarios/mathe/Desktop/nyoesyx/NYoesyx_Release/src/).

Você não precisa instalar Node.js, Python, Rust, ou pacotes externos para compilar a linguagem.

### Pré-requisitos
- Um compilador C++ compatível com o padrão **C++17** ou superior:
  - **Windows**: MinGW-w64 (`g++`) ou Microsoft Visual C++ (`MSVC` via cl.exe).
  - **Linux / macOS**: `g++` ou `clang++`.

### Compilação no Windows (Via Script Oficial)
O repositório já inclui um script otimizado para Windows que compila o binário e incorpora o ícone oficial ao executável:

1. Abra o Terminal / Prompt de Comando na pasta do projeto.
2. Execute o script de build:
   ```cmd
   cd src
   build.cmd
   ```
3. O script executará a seguinte cadeia de compilação estática com otimização máxima (`-O3`):
   ```cmd
   windres nesxi.rc -O coff -o nesxi.res
   g++ -O3 -std=c++17 nesxi.cpp nesxi.res -o ../bin/nesxi.exe -static
   ```
4. O executável final estará disponível imediatamente em [`bin/nesxi.exe`](file:///d:/Usuarios/mathe/Desktop/nyoesyx/NYoesyx_Release/bin/nesxi.exe).

### Compilação Manual (Linux / macOS / POSIX)
Para ambientes Unix-like ou containers Docker de IA, execute diretamente:
```bash
g++ -O3 -std=c++17 src/nesxi.cpp -o bin/nesxi
```
*(Nota: As funções específicas de UI do Windows NUI serão automaticamente ignoradas na compilação POSIX sem quebrar o núcleo computacional ou quântico).*

### Executando a Suite de Testes (Validação da Compilação)
Após compilar, verifique a integridade da sua VM executando os testes oficiais fornecidos na pasta [`tests/`](file:///d:/Usuarios/mathe/Desktop/nyoesyx/NYoesyx_Release/tests/):

```bash
# Teste da sintaxe DTP V6 e compilação de expressões
bin/nesxi run tests/v6_test.nesx

# Validação dos registradores nativos e checagem transacional pura
bin/nesxi run tests/t_reg_pure.nesx

# Validação do motor quântico e NUI
bin/nesxi run tests/t_phase2.nesx
```

---

## 3. 🛡️ Garantia de Segurança e Isolamento (AI Sandboxing)

Quando agentes autônomos de IA geram e executam código em sistemas operacionais host sem supervisão humana constante, a **segurança** é a prioridade crítica. 

A arquitetura da Máquina Virtual NYoesyx garante **4 Camadas Estritas de Isolamento e Segurança**:

### 1. Barreira Transacional Pura (`sys.pure`)
O NYoesyx implementa um mecanismo inovador de **Controle Transacional de Execução Especulativa**. 
Quando um LLM está explorando caminhos lógicos ou "adivinhando" ramificações em árvores de decisão, ele pode disparar execuções especulativas. Em linguagens tradicionais, isso poderia resultar em mutações acidentais de estado (ex.: deletar arquivos ou modificar variáveis globais).
- Na NYoesyx, funções ou blocos protegidos pela instrução `sys.pure` atuam como uma barreira de isolamento inquebrantável.
- Se a IA tentar executar operações irreversíveis (como chamadas de sistema, gravação em disco ou movimentação de entidades no Unreal Engine) dentro de um contexto marcado como puro, a VM intercepta e **bloqueia a instrução na camada de hardware virtual**, lançando uma exceção segura de *Impure Execution Violation*.

### 2. Isolamento de Memória Blindado
O barramento de memória semântica (`mem.*`) e a tabela de símbolos operam dentro de um espaço de endereçamento virtual confinado à VM em C++:
- Nenhum token ou ponteiro em um script `.nesx` é capaz de realizar aritmética de ponteiros bruta contra a memória física do sistema operacional host.
- A alocação vetorial (Semantic Heap) é checada por limites (*bounds-checked*). Um estouro na busca semântica resulta apenas no retorno de um vetor nulo na VM, eliminando qualquer possibilidade de exploits de *Buffer Overflow* ou vulnerabilidades de corrupção de memória.

### 3. Neutralização de Injeção de Prompts e Código (`No Eval/Exec Exploits`)
Diferente de scripts Python ou JavaScript rodando em agentes IA (que utilizam funções perigosas como `eval()` ou `os.system()` para interpretar retornos do LLM), o parser DTP da NYoesyx é uma **árvore de despacho sintático estática em C++ puro**.
- A linguagem **não possui interpretador de shell nativo acoplado**.
- Se um usuário malicioso tentar injetar comandos como `; rm -rf /` ou `powershell -c ...` através de um prompt antagônico para a IA, o parser do NYoesyx tratará a string estritamente como um literal de dados inválido ou token desconhecido, abortando a execução antes de qualquer interação com o Kernel do sistema operacional.

### 4. Zero Risco de Cadeia de Suprimentos (*Zero Supply-Chain Risk*)
A maioria dos ecossistemas modernos de programação depende de milhares de bibliotecas de terceiros (via `npm`, `pypi` ou `cargo`), abrindo vetores para injeção de malware em dependências profundas (*Supply-Chain Attacks*).
- O NYoesyx é compilado a partir de um único motor monolítico inspecionável ([src/nesxi.cpp](file:///d:/Usuarios/mathe/Desktop/nyoesyx/NYoesyx_Release/src/nesxi.cpp)).
- O código-fonte pode ser auditado linha por linha por qualquer pesquisador ou analista de segurança em minutos. **O que você compila é 100% o que é executado.**

---

<div align="center">
  <b>NYoesyx (N-OS) — Seguro. Estritamente Nativo. Feito para a Inteligência Artificial.</b>
</div>
