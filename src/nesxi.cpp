// nesxi — the native NYoesyx interpreter.
//
// A single-file, dependency-free C++17 runtime for the NYoesyx Dense Token
// Protocol (.nesx). It exists to make the project SELF-HOSTING: the reference
// runtime is no longer Python (nyoesyx/*.py) — it's this native core, which is
// also the logic mirrored by the UE plugin's FNYoesyxVM. No .py needed to run,
// test, or verify the language.
//
// Build (MSVC):  cl /EHsc /std:c++17 /O2 nesxi.cpp /Fe:nesxi.exe
// Usage:
//   nesxi run  <file.nesx>     run a program, print a trace, exit 0 on success
//   nesxi test <dir>           run every *.nesx in <dir> as a test, summarize
//
// DTP line:  <op> <cost> <deps...|none> $ <payload tokens...>
// Node ids are SEQUENTIAL over executable lines (comments/blank lines skip).
//
// Ops (spec v3.0, arg-count parity with the UE NYoesyxRuntime workers):
//   mem.set mem.get mem.embed mem.recall mem.sync
//   cog.inf cog.daemon  log.txn(concat|prefix-AST|identity)  sys.spawn  net.server
//   ue.log ue.tool ue.spawn ue.destroy ue.move ue.anim ue.fx ue.sound  assert
// L3 composition (the "usable for real apps" gate):
//   fn      declare a named sub-graph; body lines separated by '|'; no-op returns "fn"
//   call    invoke a fn in a FRESH scope; args reach the body via the heap as
//           @arg / @arg0 @arg1 ...; returns the scalar of the body's LAST node;
//           heap arg-keys are saved/restored so recursion is safe
//   branch  <cond> <fnTrue> <fnFalse> [args]; truthy(cond)->fnTrue else fnFalse
//   use     <file.nesx>  import another module's fn/heap defs into this VM
//           (idempotent, cycle-safe) — the foundation of a real LIBRARY system
// Refs: @N (node scalar), @N.field (tool-result field), @key (heap value)

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <set>
#include <filesystem>

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------- util
static std::vector<std::string> splitWS(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.length()) {
        while (start < s.length() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        if (start >= s.length()) break;
        size_t end = start;
        while (end < s.length() && !std::isspace(static_cast<unsigned char>(s[end]))) end++;
        out.emplace_back(s.substr(start, end - start));
        start = end;
    }
    return out;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static std::string join(const std::vector<std::string>& v, const std::string& sep, size_t from = 0) {
    std::string out;
    for (size_t i = from; i < v.size(); ++i) { if (i > from) out += sep; out += v[i]; }
    return out;
}
static bool parseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size();
}
static bool isInt(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    if (i == s.size()) return false;
    for (; i < s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}
static std::string numToStr(double d) {
    if (d == std::floor(d) && std::abs(d) < 1e15) {
        return std::to_string((long long)d);
    }
    std::ostringstream o; o << d; return o.str();
}

// GPT-2-style pre-tokenization count (ASCII). Each pre-token is a maximal run the
// GPT-2 regex would isolate:  ` ?letters` | ` ?digits` | ` ?punct+` | `whitespace+`
// (a single leading space is absorbed by the following word/number/punct run).
// BPE only ever SPLITS pre-tokens further, never merges across them, so this is a
// well-defined LOWER BOUND on the real BPE token count. What the benchmark reports
// is the .nesx/.py RATIO, which is stable across tokenizers — that's the honest,
// reproducible measure of the project's core thesis (fewer tokens than Python).
static long pretokCount(const std::string& s) {
    long n = 0; size_t i = 0, L = s.size();
    auto A = [](unsigned char c){ return std::isalpha(c) != 0; };
    auto D = [](unsigned char c){ return std::isdigit(c) != 0; };
    auto S = [](unsigned char c){ return std::isspace(c) != 0; };
    while (i < L) {
        bool lead = (s[i] == ' ');
        size_t start = i + (lead ? 1 : 0);
        if (start < L && A((unsigned char)s[start])) { size_t k = start; while (k < L && A((unsigned char)s[k])) k++; n++; i = k; continue; }
        if (start < L && D((unsigned char)s[start])) { size_t k = start; while (k < L && D((unsigned char)s[k])) k++; n++; i = k; continue; }
        if (start < L && !S((unsigned char)s[start]) && !A((unsigned char)s[start]) && !D((unsigned char)s[start])) {
            size_t k = start; while (k < L && !S((unsigned char)s[k]) && !A((unsigned char)s[k]) && !D((unsigned char)s[k])) k++; n++; i = k; continue;
        }
        if (S((unsigned char)s[i])) { while (i < L && S((unsigned char)s[i])) i++; n++; continue; }
        ++i;
    }
    return n;
}

// ----------------------------------------------------------------------------- model
struct Node {
    int id = -1;
    std::string op;
    double cost = 0.0;
    std::vector<int> deps;
    std::vector<std::string> payload;
    int line = 0;
};
struct NodeResult {
    int id = -1;
    std::string op;
    bool ok = false;
    std::string scalar;
    std::map<std::string, std::string> data;
    std::string error;
};
struct RunResult {
    bool ok = false;
    int total = 0, run = 0;
    std::string error;
    std::vector<NodeResult> nodes;
};
// L3: a named sub-graph. Bodies are inline DTP lines (split from the fn payload by
// '|'), each carrying its own '$' separator, parsed into body-local nodes whose
// ids restart at 0 — so @N inside a body refers to the body, not the caller.
struct Func {
    std::string name;
    std::vector<Node> body;
};

// V5 "AI-native" vocabulary: short 1-token-ASCII opcodes are aliases for the V3
// canonical ops. Measured (Font/token_probe.py): these cost fewer tokens than the
// dotted V3 names AND fewer than V4 Unicode glyphs, while staying plain ASCII.
// Normalizing here (not renaming) keeps every existing V3 program/golden valid —
// the runtime accepts both vocabularies. The visual V4 glyph set is a dashboard
// skin only (zero token effect) and never reaches the runtime.
static std::string canonicalOp(const std::string& op) {
    if (op.empty()) return op;
    std::string check = op;
    if (op[0] == '>') check = "ue." + op.substr(1);
    else if (op[0] == '*') check = "mem." + op.substr(1);
    else if (op[0] == '?') check = "cog." + op.substr(1);
    else if (op[0] == '=') check = "reg." + op.substr(1);

    static const std::unordered_map<std::string, std::string> v5 = {
        {"set","mem.set"}, {"get","mem.get"}, {"embed","mem.embed"},
        {"find","mem.recall"}, {"recall","mem.recall"}, {"sync","mem.sync"},
        {"inf","cog.inf"}, {"daemon","cog.daemon"}, {"log","log.txn"},
        {"ulg","ue.log"}, {"ulog","ue.log"}, {"tool","ue.tool"},
        {"spawn","ue.spawn"}, {"destroy","ue.destroy"}, {"mov","ue.move"},
        {"anim","ue.anim"}, {"fx","ue.fx"}, {"sound","ue.sound"},
        {"ue.mov","ue.move"},
        {"reg.set","reg.set"}, {"reg.get","reg.get"}
    };
    auto it = v5.find(check);
    if (it != v5.end()) return it->second;
    
    auto it2 = v5.find(op);
    return it2 != v5.end() ? it2->second : check;
}

// Parse one inline body line (already tokenised) into a Node with the given local
// id:  <op> <cost> <deps...|none> $ <payload...>. Mirrors parse()'s per-line rules.
static bool parseLineTokens(const std::vector<std::string>& toks, int id, Node& n, std::string& err) {
    if (toks.empty()) return false;
    auto d = std::find(toks.begin(), toks.end(), std::string("$"));
    size_t di = (size_t)(d - toks.begin());
    if (d == toks.end()) {
        n.id = id; n.op = canonicalOp(toks[0]);
        n.cost = 0.0;
        for (size_t i = 1; i < toks.size(); ++i) n.payload.push_back(toks[i]);
        return true;
    }
    if (di < 2) { err = "body line: too few meta tokens"; return false; }
    n.id = id; n.op = canonicalOp(toks[0]);
    if (!parseDouble(toks[1], n.cost)) { err = "body line: cost '" + toks[1] + "' not a number"; return false; }
    for (size_t i = 2; i < di; ++i) {
        std::string dl = toks[i]; for (auto& c : dl) c = (char)std::tolower((unsigned char)c);
        if (dl == "none" || dl == "_") continue;
        if (!isInt(toks[i])) { err = "body line: dep '" + toks[i] + "' not an int (use 'none')"; return false; }
        n.deps.push_back(std::stoi(toks[i]));
    }
    for (size_t i = di + 1; i < toks.size(); ++i) n.payload.push_back(toks[i]);
    return true;
}

// ----------------------------------------------------------------------------- parser
static bool parse(const std::string& src, std::vector<Node>& out, std::string& err) {
    out.clear();
    std::istringstream iss(src);
    std::string raw;
    int lineNo = 0, nid = 0;
    while (std::getline(iss, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        // V6 Macro: Action Vector
        if (line[0] == '!') {
            auto m = splitWS(line);
            if (m.size() >= 3 && m[0] == "!npc_rout") {
                std::string target = m[1];
                std::string action = m[2];
                Node n1; n1.id = nid++; n1.line = lineNo; n1.op = "mem.set"; n1.cost = 0.0;
                n1.payload = {target, "idle_" + action}; out.push_back(n1);
                Node n2; n2.id = nid++; n2.line = lineNo; n2.op = "cog.inf"; n2.cost = 0.5; n2.deps = {n1.id};
                n2.payload = {"gemini-2.0-flash", "0.3", "decide_npc_action_for", target + "_area"}; out.push_back(n2);
                Node n3; n3.id = nid++; n3.line = lineNo; n3.op = "ue.move"; n3.cost = 0.1; n3.deps = {n2.id};
                n3.payload = {target, "100", "0", "200"}; out.push_back(n3);
                Node n4; n4.id = nid++; n4.line = lineNo; n4.op = "ue.anim"; n4.cost = 0.0; n4.deps = {n3.id};
                n4.payload = {target, "walk_" + action}; out.push_back(n4);
                continue;
            }
        }

        auto dollar = line.find('$');
        auto pipe = line.find('|');
        if (pipe != std::string::npos && dollar != std::string::npos && pipe < dollar) {
            dollar = std::string::npos;
        }
        std::string meta, payload;
        if (dollar == std::string::npos) {
            auto toks = splitWS(line);
            if (toks.empty()) continue;
            meta = toks[0] + " 0.0 none";
            payload = line.substr(toks[0].length());
            payload = trim(payload);
        } else {
            meta = line.substr(0, dollar);
            payload = line.substr(dollar + 1);
        }
        auto m = splitWS(meta);
        if (m.size() < 2) {
            err = "line " + std::to_string(lineNo) + ": too few meta tokens";
            return false;
        }
        Node n;
        n.id = nid; n.line = lineNo; n.op = canonicalOp(m[0]);
        if (!parseDouble(m[1], n.cost)) {
            err = "line " + std::to_string(lineNo) + ": cost '" + m[1] + "' not a number";
            return false;
        }
        for (size_t i = 2; i < m.size(); ++i) {
            std::string d = m[i];
            std::string dl = d; for (auto& c : dl) c = (char)std::tolower((unsigned char)c);
            if (dl == "none" || dl == "_") continue;
            if (!isInt(d)) {
                err = "line " + std::to_string(lineNo) + ": dep '" + d + "' not an int (use 'none')";
                return false;
            }
            int di = std::stoi(d);
            if (di < 0 || di >= nid) {
                err = "line " + std::to_string(lineNo) + ": dep " + d + " out of range";
                return false;
            }
            n.deps.push_back(di);
        }
        n.payload = splitWS(payload);
        out.push_back(std::move(n));
        ++nid;
    }
    if (out.empty()) { err = "no executable nodes"; return false; }
    return true;
}

static bool readFile(const std::string& path, std::string& out);  // defined in cli section

// ----------------------------------------------------------------------------- VM
class VM {
public:
    RunResult run(const std::vector<Node>& nodes, bool trace) {
        traceOn = trace;
        RunResult r; r.total = (int)nodes.size(); r.ok = true;
        for (const auto& n : nodes) {
            bool depsOk = true; std::string failed;
            for (int d : n.deps) {
                auto it = results.find(d);
                if (it == results.end() || !it->second.ok) { depsOk = false; failed = std::to_string(d); break; }
            }
            NodeResult nr;
            if (!depsOk) {
                nr.id = n.id; nr.op = n.op; nr.ok = false;
                nr.error = "skipped: dependency node " + failed + " did not succeed";
            } else {
                nr = exec(n);
            }
            results[n.id] = nr;
            r.nodes.push_back(nr);
            if (nr.ok) ++r.run; else { r.ok = false; if (r.error.empty()) r.error = "node " + std::to_string(nr.id) + " (" + nr.op + "): " + nr.error; }
            if (trace) {
                std::cout << "  #" << nr.id << " " << nr.op << " -> " << (nr.ok ? "ok" : "FAIL")
                          << (nr.scalar.empty() ? "" : ("  '" + nr.scalar + "'"))
                          << (nr.error.empty() ? "" : ("  [" + nr.error + "]")) << "\n";
            }
        }
        return r;
    }

private:
    struct QuantumSim {
        std::vector<double> real, imag;
        int num_qubits = 0;
        
        void ensure(int q) {
            if (q >= num_qubits) {
                int old_n = num_qubits;
                num_qubits = q + 1;
                int size = 1 << num_qubits;
                std::vector<double> nr(size, 0.0), ni(size, 0.0);
                if (real.empty()) { nr[0] = 1.0; }
                else {
                    for (size_t i = 0; i < real.size(); ++i) {
                        nr[i] = real[i]; ni[i] = imag[i];
                    }
                }
                real = nr; imag = ni;
            }
        }
        
        void h(int q) {
            ensure(q);
            int size = 1 << num_qubits;
            std::vector<double> nr = real, ni = imag;
            double inv_sqrt2 = 1.0 / std::sqrt(2.0);
            for (int i = 0; i < size; ++i) {
                if ((i & (1 << q)) == 0) {
                    int i1 = i | (1 << q);
                    double r0 = real[i], i0 = imag[i];
                    double r1 = real[i1], i1_i = imag[i1];
                    nr[i] = (r0 + r1) * inv_sqrt2;
                    ni[i] = (i0 + i1_i) * inv_sqrt2;
                    nr[i1] = (r0 - r1) * inv_sqrt2;
                    ni[i1] = (i0 - i1_i) * inv_sqrt2;
                }
            }
            real = nr; imag = ni;
        }

        void cnot(int c, int t) {
            ensure(std::max(c, t));
            int size = 1 << num_qubits;
            std::vector<double> nr = real, ni = imag;
            for (int i = 0; i < size; ++i) {
                if ((i & (1 << c)) != 0) {
                    if ((i & (1 << t)) == 0) {
                        int i1 = i | (1 << t);
                        nr[i] = real[i1]; ni[i] = imag[i1];
                        nr[i1] = real[i]; ni[i1] = imag[i];
                    }
                }
            }
            real = nr; imag = ni;
        }

        int measure(int q) {
            ensure(q);
            int size = 1 << num_qubits;
            double p1 = 0;
            for (int i = 0; i < size; ++i) {
                if (i & (1 << q)) p1 += real[i]*real[i] + imag[i]*imag[i];
            }
            int result = ((double)rand() / RAND_MAX) < p1 ? 1 : 0;
            
            // collapse
            double norm = 0;
            for (int i = 0; i < size; ++i) {
                if (((i >> q) & 1) == result) {
                    norm += real[i]*real[i] + imag[i]*imag[i];
                } else {
                    real[i] = 0; imag[i] = 0;
                }
            }
            norm = std::sqrt(norm);
            for (int i = 0; i < size; ++i) {
                real[i] /= norm; imag[i] /= norm;
            }
            return result;
        }
    };

    std::map<int, NodeResult> results;
    std::unordered_map<std::string, std::string> memory;
    std::unordered_map<std::string, std::string> registers; // High-speed deterministic memory
    std::map<std::string, Func> funcs;   // L3 function table
    std::set<std::string> imported;      // files already pulled in by `use` (cycle/dup guard)
    std::vector<std::vector<std::string>> callStackArgs; // Optimized O(1) argument scoping
    QuantumSim qsim;
    bool traceOn = false;

    std::string resolve(const std::string& tok) const {
        if (tok.empty()) return tok;
        if (tok[0] == '%') {
            std::string ref = tok.substr(1);
            auto it = registers.find(ref);
            return it != registers.end() ? it->second : tok;
        }
        if (tok[0] != '@') return tok;
        if (tok.find("@arg") == 0) {
            std::string nStr = tok.substr(4);
            if (nStr.empty()) nStr = "0";
            if (isInt(nStr)) {
                int idx = std::stoi(nStr);
                if (!callStackArgs.empty() && idx >= 0 && idx < callStackArgs.back().size()) {
                    return callStackArgs.back()[idx];
                }
            }
            return tok;
        }
        std::string ref = tok.substr(1);
        std::string idpart = ref, field;
        auto dot = ref.find('.');
        if (dot != std::string::npos) { idpart = ref.substr(0, dot); field = ref.substr(dot + 1); }
        if (isInt(idpart)) {
            auto it = results.find(std::stoi(idpart));
            if (it == results.end()) return tok;
            if (field.empty()) return it->second.scalar;
            auto f = it->second.data.find(field);
            return f != it->second.data.end() ? f->second : tok;
        }
        auto m = memory.find(ref);
        return m != memory.end() ? m->second : tok;
    }

    static bool truthy(const std::string& s) {
        return !(s.empty() || s == "0" || s == "0.0" || s == "false" || s == "False");
    }

    NodeResult exec(const Node& n) {
        if (n.op == "reg.set")    return execRegSet(n);
        if (n.op == "reg.get")    return execRegGet(n);
        if (n.op == "mem.set")    return execMemSet(n);
        if (n.op == "mem.get")    return execMemGet(n);
        if (n.op == "mem.embed")  return execMemEmbed(n);
        if (n.op == "mem.recall") return execMemRecall(n);
        if (n.op == "mem.sync")   return execMemSync(n);
        if (n.op == "ue.log")     return execLog(n);
        if (n.op == "ue.tool")    return execTool(n);
        if (n.op == "log.txn")    return execLogTxn(n);
        if (n.op == "cog.inf")    return execCog(n);
        if (n.op == "cog.daemon") return execCogDaemon(n);
        if (n.op == "sys.spawn")  return execSysSpawn(n);
        if (n.op == "sys.pure")   return execSysPure(n);
        if (n.op == "net.server") return execNetServer(n);
        if (n.op == "assert")     return execAssert(n);
        if (n.op == "fn")         return execFn(n);
        if (n.op == "call")       return execCall(n);
        if (n.op == "branch")     return execBranch(n);
        if (n.op == "use")        return execUse(n);
        // ue.* spatial/world ops — headless models mirroring FNYoesyxUnrealWorker
        // (NYoesyxRuntime plugin). Same arg-count validation; no engine here, so
        // they log + return the worker's OutResult string instead of touching a
        // UWorld. Lets nesxi validate game-targeted .nesx offline (cross-runtime).
        if (n.op == "ue.spawn" || n.op == "ue.destroy" || n.op == "ue.move" ||
            n.op == "ue.anim"  || n.op == "ue.fx"      || n.op == "ue.sound" || n.op == "ue.ulg")
            return execUe(n);
        if (n.op.find("ui.") == 0) return execUi(n);
        if (n.op.find("qnt.") == 0) return execQuantum(n);
        NodeResult r; r.id = n.id; r.op = n.op; r.ok = false; r.error = "unknown op '" + n.op + "'";
        return r;
    }

    // -------- L3: fn / call / branch ----------------------------------------
    // fn <name> | <body line> | <body line> ...
    // The fn payload keeps each body line's own '$' (parse() only splits the
    // OUTER line at its FIRST '$'), so a body line is a full DTP line we re-parse.
    NodeResult execFn(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "fn requires a name"; return r; }
        Func f; f.name = n.payload[0];
        std::vector<std::string> seg;
        int bid = 0;
        for (size_t i = 1; i <= n.payload.size(); ++i) {
            if (i == n.payload.size() || n.payload[i] == "|") {
                if (!seg.empty()) {
                    Node bn; std::string perr;
                    if (!parseLineTokens(seg, bid, bn, perr)) {
                        r.ok = false; r.error = "fn '" + f.name + "': " + perr; return r;
                    }
                    f.body.push_back(std::move(bn)); ++bid;
                }
                seg.clear();
            } else {
                seg.push_back(n.payload[i]);
            }
        }
        if (f.body.empty()) { r.ok = false; r.error = "fn '" + f.name + "' has no body"; return r; }
        funcs[f.name] = std::move(f);
        r.ok = true; r.scalar = "fn"; return r;
    }

    NodeResult execCall(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "call requires a fn name"; return r; }
        auto it = funcs.find(n.payload[0]);
        if (it == funcs.end()) { r.ok = false; r.error = "fn '" + n.payload[0] + "' not defined"; return r; }
        std::vector<std::string> args;
        for (size_t i = 1; i < n.payload.size(); ++i) args.push_back(resolve(n.payload[i]));
        return invoke(it->second, args, r);
    }

    NodeResult execBranch(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.size() < 3) { r.ok = false; r.error = "branch needs <cond> <fnTrue> <fnFalse> [args]"; return r; }
        std::string cond = resolve(n.payload[0]);
        const std::string& fnName = truthy(cond) ? n.payload[1] : n.payload[2];
        auto it = funcs.find(fnName);
        if (it == funcs.end()) { r.ok = false; r.error = "branch fn '" + fnName + "' not defined"; return r; }
        std::vector<std::string> args;
        for (size_t i = 3; i < n.payload.size(); ++i) args.push_back(resolve(n.payload[i]));
        return invoke(it->second, args, r);
    }

    // Run a fn body in a FRESH @N scope. Args are pushed to the O(1) call stack.
    // The body's node ids are its own. Save/restore the @N results map so nesting
    // and recursion stay isolated.
    NodeResult invoke(const Func& f, const std::vector<std::string>& args, NodeResult r) {
        callStackArgs.push_back(args);

        // fresh @N space for the body
        std::map<int, NodeResult> savedResults;
        savedResults.swap(results);
        RunResult br = run(f.body, traceOn);
        results.swap(savedResults);  // restore caller's @N space

        callStackArgs.pop_back();

        if (!br.ok) { r.ok = false; r.error = "fn '" + f.name + "' body failed: " + br.error; return r; }
        r.ok = true;
        r.scalar = br.nodes.empty() ? "" : br.nodes.back().scalar;
        return r;
    }

    // use <file.nesx> — import another module's definitions. The target is
    // parsed and its top-level nodes are executed in the CURRENT VM, so its `fn`
    // declarations and `mem.set` seeds land in this VM's shared funcs/heap and
    // become callable here. This is what turns L3 functions into a real library
    // system: foundation of an ECOSYSTEM, not just a language. Idempotent and
    // cycle-safe (a file is pulled in at most once). The import runs in a FRESH
    // @N space so the caller's node results are never clobbered.
    NodeResult execUse(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "use requires a file path"; return r; }
        const std::string& path = n.payload[0];
        if (imported.count(path)) { r.ok = true; r.scalar = "use:" + path + ":cached"; return r; }
        imported.insert(path);
        std::string src;
        if (!readFile(path, src)) { r.ok = false; r.error = "use: cannot read '" + path + "'"; return r; }
        std::vector<Node> nodes; std::string perr;
        if (!parse(src, nodes, perr)) { r.ok = false; r.error = "use '" + path + "': " + perr; return r; }
        size_t before = funcs.size();
        std::map<int, NodeResult> savedResults;
        savedResults.swap(results);                 // isolate the import's @N space
        RunResult ir = run(nodes, false);           // load quietly (setup, not the program's trace)
        results.swap(savedResults);                 // restore caller's @N space
        if (!ir.ok) { r.ok = false; r.error = "use '" + path + "' failed to load: " + ir.error; return r; }
        r.ok = true; r.scalar = "use:" + path + ":+" + std::to_string(funcs.size() - before) + "fn";
        return r;
    }

    NodeResult execRegSet(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "reg.set requires a key"; return r; }
        std::vector<std::string> parts;
        for (size_t i = 1; i < n.payload.size(); ++i) parts.push_back(resolve(n.payload[i]));
        std::string val = join(parts, " ");
        registers[n.payload[0]] = val;
        r.ok = true; r.scalar = val; return r;
    }

    NodeResult execRegGet(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "reg.get requires a key"; return r; }
        auto it = registers.find(n.payload[0]);
        if (it == registers.end()) { r.ok = false; r.error = "register '" + n.payload[0] + "' not found"; return r; }
        r.ok = true; r.scalar = it->second; return r;
    }

    NodeResult execMemSet(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "mem.set requires a key"; return r; }
        std::vector<std::string> parts;
        for (size_t i = 1; i < n.payload.size(); ++i) parts.push_back(resolve(n.payload[i]));
        std::string val = join(parts, " ");
        memory[n.payload[0]] = val;
        r.ok = true; r.scalar = val; return r;
    }

    NodeResult execLog(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::vector<std::string> parts;
        for (auto& t : n.payload) parts.push_back(resolve(t));
        r.scalar = join(parts, " ");
        std::cout << "   [ue.log #" << n.id << "] " << r.scalar << "\n";
        r.ok = true; return r;
    }

    NodeResult execTool(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "ue.tool requires a tool name"; return r; }
        std::string name = n.payload[0];
        std::map<std::string, std::string> input;
        for (size_t i = 1; i < n.payload.size(); ++i) {
            auto eq = n.payload[i].find('=');
            if (eq == std::string::npos) { r.ok = false; r.error = "arg '" + n.payload[i] + "' not key=value"; return r; }
            input[n.payload[i].substr(0, eq)] = resolve(n.payload[i].substr(eq + 1));
        }
        // Standalone builtin registry. Inside UE the same op dispatches to the
        // ~95 NeuralForge editor tools via FAIToolRegistry; here we ship just
        // enough to exercise the bridge + tool-output chaining in tests.
        if (name == "echo") {
            r.data["echo"] = input.count("message") ? input["message"] : "";
            r.scalar = "ok:echo";
            r.ok = true; return r;
        }
        if (name == "concat") {
            r.data["result"] = input["a"] + input["b"];
            r.scalar = r.data["result"];
            r.ok = true; return r;
        }
        r.ok = false; r.error = "tool '" + name + "' not in standalone registry";
        return r;
    }

    NodeResult execLogTxn(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "log.txn requires a payload"; return r; }
        const std::string& head = n.payload[0];
        bool isArith = (head == "+" || head == "-" || head == "*" || head == "/");
        // Parity with FNYoesyxLogicWorker (NYoesyxRuntime):
        //  - 'concat a b c'  -> tokens joined with NO separator ("abc")
        //  - leading operator -> recursive prefix AST eval (strict here: the
        //    native oracle rejects trailing tokens that the UE worker silently
        //    ignores — flagging ambiguous programs is a feature of the oracle)
        //  - anything else    -> identity pass-through of the whole payload
        if (head == "concat") {
            std::string out;
            for (size_t i = 1; i < n.payload.size(); ++i) out += resolve(n.payload[i]);
            r.ok = true; r.scalar = out; return r;
        }
        if (!isArith && head != "ref") {
            std::vector<std::string> parts;
            for (auto& t : n.payload) parts.push_back(resolve(t));
            r.ok = true; r.scalar = join(parts, " "); return r;
        }
        size_t cur = 0; bool ok = true; std::string e;
        std::string v = evalPrefix(n.payload, cur, ok, e);
        if (!ok) { r.ok = false; r.error = e; return r; }
        if (cur != n.payload.size()) { r.ok = false; r.error = "trailing tokens in prefix AST"; return r; }
        r.ok = true; r.scalar = v; return r;
    }

    NodeResult execCog(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::string concept;
        if (n.payload.size() > 2) {
            std::vector<std::string> parts;
            for (size_t i = 2; i < n.payload.size(); ++i) parts.push_back(resolve(n.payload[i]));
            concept = join(parts, " ");
        }
        r.scalar = "[cog.inf:" + concept + "]";
        r.ok = true; return r;  // offline-deterministic (mock); live routing lives in UE/host
    }

    NodeResult execAssert(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.size() < 3) { r.ok = false; r.error = "assert needs: <eq|neq|contains> <a> <b...>"; return r; }
        std::string cmp = n.payload[0];
        std::string a = resolve(n.payload[1]);
        // b is the rest, joined — so multi-word values (e.g. a string fold) compare cleanly.
        std::vector<std::string> bparts;
        for (size_t i = 2; i < n.payload.size(); ++i) bparts.push_back(resolve(n.payload[i]));
        std::string b = join(bparts, " ");
        bool pass;
        if (cmp == "eq") pass = (a == b);
        else if (cmp == "neq") pass = (a != b);
        else if (cmp == "contains") pass = (a.find(b) != std::string::npos);
        else { r.ok = false; r.error = "unknown assert cmp '" + cmp + "'"; return r; }
        if (!pass) { r.ok = false; r.error = "assert " + cmp + " failed: '" + a + "' vs '" + b + "'"; return r; }
        r.ok = true; r.scalar = "assert:" + cmp + ":ok"; return r;
    }

    // mem.get <concept> — retrieve a value previously stored in the heap.
    NodeResult execMemGet(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "mem.get requires a key"; return r; }
        auto it = memory.find(n.payload[0]);
        if (it == memory.end()) { r.ok = false; r.error = "concept '" + n.payload[0] + "' not in heap"; return r; }
        r.ok = true; r.scalar = it->second; return r;
    }

    // mem.embed <concept> — store this node's dependency output under a concept.
    // Headless model of the SemanticHeap embed: value = joined scalars of deps.
    NodeResult execMemEmbed(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "mem.embed requires a concept"; return r; }
        std::vector<std::string> parts;
        for (int d : n.deps) { auto it = results.find(d); if (it != results.end()) parts.push_back(it->second.scalar); }
        std::string val = parts.empty() ? n.payload[0] : join(parts, " ");
        memory[n.payload[0]] = val;
        r.ok = true; r.scalar = val; return r;
    }

    // mem.recall <query...> — fuzzy search over the heap, mirroring
    // SemanticHeap's exact-then-fuzzy fallback. Order: (1) exact key on the
    // first token; (2) substring either direction (concept in query, or query
    // in concept) over the full joined query; (3) shared whole-word overlap.
    NodeResult execMemRecall(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "mem.recall requires a query"; return r; }
        auto exact = memory.find(n.payload[0]);
        if (exact != memory.end()) { r.ok = true; r.scalar = exact->second; return r; }
        std::string q = join(n.payload, " ");
        for (auto& kv : memory)
            if (q.find(kv.first) != std::string::npos || kv.first.find(q) != std::string::npos) {
                r.ok = true; r.scalar = kv.second; return r;
            }
        // word-overlap fallback: any concept word appearing as a query word
        for (auto& kv : memory) {
            for (const auto& cw : splitWS(kv.first))
                for (const auto& qw : n.payload)
                    if (cw == qw) { r.ok = true; r.scalar = kv.second; return r; }
        }
        r.ok = false; r.error = "no concept matches '" + q + "'"; return r;
    }

    // mem.sync <recipient> <message...> — broadcast to the IA2IA stream.
    // Headless: no socket; just confirms the dispatch deterministically.
    NodeResult execMemSync(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "mem.sync requires a recipient"; return r; }
        r.ok = true; r.scalar = "sync:" + n.payload[0]; return r;
    }

    // cog.daemon <interval_s> — persistent cognitive loop. Headless: validated
    // and acknowledged (the loop itself only runs inside a live host/UE Tick).
    NodeResult execCogDaemon(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::string iv = n.payload.empty() ? "0" : n.payload[0];
        r.ok = true; r.scalar = "daemon:" + iv; return r;
    }

    // sys.spawn <script_path> — spawn a child sub-VM. Headless: acknowledge the
    // spawn (NYoesyxSubVMManager runs the child for real on the UE Tick).
    NodeResult execSysSpawn(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "sys.spawn requires a script path"; return r; }
        r.ok = true; r.scalar = "spawned:" + n.payload[0]; return r;
    }

    bool isOpPure(const std::string& op) const {
        return op.find("ue.") != 0 && op.find("net.") != 0 && op.find("sys.") != 0 && op.find("cog.") != 0;
    }

    // sys.pure <fnName> — statically analyze if a fn contains any impure nodes (like ue.* or net.*)
    NodeResult execSysPure(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        if (n.payload.empty()) { r.ok = false; r.error = "sys.pure requires a fn name"; return r; }
        auto it = funcs.find(n.payload[0]);
        if (it == funcs.end()) { r.ok = false; r.error = "fn '" + n.payload[0] + "' not defined"; return r; }
        for (const auto& bn : it->second.body) {
            if (!isOpPure(bn.op)) {
                r.ok = false; r.error = "fn '" + n.payload[0] + "' is not pure (contains " + bn.op + ")"; return r;
            }
        }
        r.ok = true; r.scalar = "pure:" + n.payload[0]; return r;
    }

    // net.server <port> — allocate a native HTTP/SSE server. Headless ack.
    NodeResult execNetServer(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::string port = n.payload.empty() ? "0" : n.payload[0];
        r.ok = true; r.scalar = "server:" + port; return r;
    }

    // ue.* world ops — headless mirror of FNYoesyxUnrealWorker. Arg-count rules
    // are kept IN LOCKSTEP with the plugin so a program that validates here
    // validates there. Numeric coords are resolved (@refs allowed) but not
    // applied to any UWorld; the scalar echoes the worker's OutResult.
    NodeResult execUe(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::vector<std::string> a;
        for (auto& t : n.payload) a.push_back(resolve(t));
        auto need = [&](size_t k, const char* msg) -> bool {
            if (a.size() < k) { r.ok = false; r.error = msg; return false; }
            return true;
        };
        if (n.op == "ue.spawn") {
            if (!need(1, "ue.spawn needs concept")) return r;
            r.scalar = "spawned " + a[0];
        } else if (n.op == "ue.destroy") {
            if (!need(1, "ue.destroy needs concept")) return r;
            r.scalar = "destroyed " + a[0];
        } else if (n.op == "ue.move") {
            if (!need(4, "ue.move needs concept x y z")) return r;
            r.scalar = "moved " + a[0] + " to " + a[1] + " " + a[2] + " " + a[3];
        } else if (n.op == "ue.anim") {
            if (!need(2, "ue.anim needs concept anim_name")) return r;
            r.scalar = "animated " + a[0] + " " + a[1];
        } else if (n.op == "ue.fx") {
            if (!need(4, "ue.fx needs effect x y z")) return r;
            r.scalar = "fx " + a[0] + " at " + a[1] + " " + a[2] + " " + a[3];
        } else if (n.op == "ue.sound") {
            if (!need(4, "ue.sound needs sound x y z")) return r;
            r.scalar = "sound " + a[0] + " at " + a[1] + " " + a[2] + " " + a[3];
        } else if (n.op == "ue.ulg") {
            if (!need(1, "ue.ulg needs message")) return r;
            r.scalar = "ulg " + a[0];
        }
        std::cout << "   [" << n.op << " #" << n.id << "] " << r.scalar << "\n";
        r.ok = true; return r;
    }

    // NUI mock for valid syntax check
    NodeResult execUi(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        r.ok = true; 
        std::vector<std::string> parts;
        for (auto& t : n.payload) parts.push_back(resolve(t));
        r.scalar = n.op + ":" + join(parts, " ");
        std::cout << "   [NUI #" << n.id << "] " << r.scalar << "\n";
        return r;
    }

    // Quantum classical simulator
    NodeResult execQuantum(const Node& n) {
        NodeResult r; r.id = n.id; r.op = n.op;
        std::vector<std::string> a;
        for (auto& t : n.payload) a.push_back(resolve(t));
        
        if (n.op == "qnt.hadamard") {
            if (a.empty() || !isInt(a[0])) { r.ok = false; r.error = "qnt.hadamard needs qubit_id"; return r; }
            qsim.h(std::stoi(a[0]));
            r.scalar = "H|" + a[0] + ">";
        } else if (n.op == "qnt.cnot") {
            if (a.size() < 2 || !isInt(a[0]) || !isInt(a[1])) { r.ok = false; r.error = "qnt.cnot needs control target"; return r; }
            qsim.cnot(std::stoi(a[0]), std::stoi(a[1]));
            r.scalar = "CNOT|" + a[0] + "," + a[1] + ">";
        } else if (n.op == "qnt.measure") {
            if (a.size() < 2 || !isInt(a[0])) { r.ok = false; r.error = "qnt.measure needs qubit_id target_reg"; return r; }
            int m = qsim.measure(std::stoi(a[0]));
            registers[a[1]] = std::to_string(m);
            r.scalar = std::to_string(m);
            std::cout << "   [QNT #" << n.id << "] collapsed to " << m << "\n";
        } else {
            r.ok = false; r.error = "unknown quantum op"; return r;
        }
        r.ok = true; return r;
    }

    std::string evalPrefix(const std::vector<std::string>& t, size_t& cur, bool& ok, std::string& err) {
        if (cur >= t.size()) { ok = false; err = "unexpected end of prefix AST"; return ""; }
        std::string tok = t[cur++];
        auto isArith = [](const std::string& s) { return s == "+" || s == "-" || s == "*" || s == "/"; };
        if (isArith(tok)) {
            std::string l = evalPrefix(t, cur, ok, err); if (!ok) return "";
            std::string rr = evalPrefix(t, cur, ok, err); if (!ok) return "";
            double L, R; bool ln = parseDouble(l, L), rn = parseDouble(rr, R);
            if (tok == "+" && !(ln && rn)) return l + " " + rr;
            if (!ln || !rn) { ok = false; err = "operator '" + tok + "' needs numeric operands"; return ""; }
            double o = 0;
            if (tok == "+") o = L + R; else if (tok == "-") o = L - R; else if (tok == "*") o = L * R;
            else { if (R == 0) { ok = false; err = "division by zero"; return ""; } o = L / R; }
            return numToStr(o);
        }
        if (tok == "ref") {
            if (cur >= t.size()) { ok = false; err = "'ref' expects one operand"; return ""; }
            std::string operand = t[cur++];
            return resolve(operand.size() && operand[0] == '@' ? operand : "@" + operand);
        }
        return resolve(tok);
    }
};

// ----------------------------------------------------------------------------- cli
static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}

static int cmdRun(const std::string& path) {
    std::string src;
    if (!readFile(path, src)) { std::cerr << "cannot read " << path << "\n"; return 2; }
    std::vector<Node> nodes; std::string err;
    if (!parse(src, nodes, err)) { std::cerr << "parse error: " << err << "\n"; return 2; }
    VM vm; RunResult r = vm.run(nodes, true);
    std::cout << (r.ok ? "OK" : "FAILED") << " (" << r.run << "/" << r.total << " nodes)";
    if (!r.error.empty()) std::cout << "  " << r.error;
    std::cout << "\n";
    return r.ok ? 0 : 1;
}

static int cmdTest(const std::string& dir) {
    int pass = 0, fail = 0;
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file() && e.path().extension() == ".nesx") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (auto& p : files) {
        std::string src; readFile(p.string(), src);
        std::vector<Node> nodes; std::string err;
        bool ok;
        if (!parse(src, nodes, err)) { ok = false; }
        else { VM vm; ok = vm.run(nodes, false).ok; }
        std::cout << "[" << (ok ? "PASS" : "FAIL") << "] " << p.filename().string();
        if (!ok && !err.empty()) std::cout << "  (" << err << ")";
        std::cout << "\n";
        if (ok) ++pass; else ++fail;
    }
    std::cout << "\n=== " << pass << " passed, " << fail << " failed ===\n";
    return fail == 0 ? 0 : 1;
}

// bench — measure the project's core thesis: a NYoesyx (.nesx) program costs fewer
// LLM tokens than the equivalent Python (.py). Walks <dir> recursively, pairs files
// by (folder + stem), and for each pair with both a .nesx and a .py reports the
// pre-token and byte counts plus the % the .nesx saves. Pure measurement, offline,
// no deps — the executable companion to lang/BENCH.md.
struct BenchRow { std::string label; long ntok=0, ptok=0; long nB=0, pB=0; };

static int cmdBench(const std::string& dir) {
    // group by key = parent_dir + "/" + stem (without extension)
    std::map<std::string, std::map<std::string, fs::path>> groups; // key -> ext -> path
    for (auto& e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        if (ext != ".nesx" && ext != ".py") continue;
        std::string key = (e.path().parent_path() / e.path().stem()).string();
        groups[key][ext] = e.path();
    }
    fs::path base = fs::path(dir);
    std::vector<BenchRow> rows;
    BenchRow tot; tot.label = "TOTAL";
    for (auto& g : groups) {
        auto itN = g.second.find(".nesx");
        auto itP = g.second.find(".py");
        if (itN == g.second.end() || itP == g.second.end()) continue; // need a matched pair
        std::string ns, ps;
        if (!readFile(itN->second.string(), ns) || !readFile(itP->second.string(), ps)) continue;
        BenchRow row;
        std::error_code ec;
        row.label = fs::relative(fs::path(g.first), base, ec).generic_string();
        if (row.label.empty()) row.label = fs::path(g.first).filename().string();
        row.ntok = pretokCount(ns); row.ptok = pretokCount(ps);
        row.nB = (long)ns.size();   row.pB = (long)ps.size();
        rows.push_back(row);
        tot.ntok += row.ntok; tot.ptok += row.ptok; tot.nB += row.nB; tot.pB += row.pB;
    }

    std::cout << "=== NYoesyx token-efficiency benchmark ===\n";
    std::cout << "metric 'tok' = GPT-2-style pre-tokens (a well-defined LOWER BOUND on real BPE\n";
    std::cout << "tokens; the .nesx/.py ratio is stable across tokenizers). 'B' = bytes.\n\n";
    if (rows.empty()) { std::cout << "(no matched .nesx/.py pairs under " << dir << ")\n"; return 1; }

    auto pct = [](long n, long p) -> double { return p > 0 ? (100.0 * (double)(p - n) / (double)p) : 0.0; };
    auto ratio = [](long n, long p) -> double { return n > 0 ? (double)p / (double)n : 0.0; };

    char buf[256];
    std::printf("  %-24s %8s %8s %8s   %8s %8s\n", "case", "nesx_tok", "py_tok", "saved", "nesx_B", "py_B");
    std::printf("  %s\n", std::string(70, '-').c_str());
    for (auto& r : rows) {
        std::snprintf(buf, sizeof(buf), "  %-24s %8ld %8ld %7.1f%%   %8ld %8ld",
                      r.label.c_str(), r.ntok, r.ptok, pct(r.ntok, r.ptok), r.nB, r.pB);
        std::cout << buf << "\n";
    }
    std::printf("  %s\n", std::string(70, '-').c_str());
    std::snprintf(buf, sizeof(buf), "  %-24s %8ld %8ld %7.1f%%   %8ld %8ld",
                  tot.label.c_str(), tot.ntok, tot.ptok, pct(tot.ntok, tot.ptok), tot.nB, tot.pB);
    std::cout << buf << "\n\n";
    std::printf("NYoesyx uses %.1f%% fewer tokens than Python across %zu cases (%.2fx denser).\n",
                pct(tot.ntok, tot.ptok), rows.size(), ratio(tot.ntok, tot.ptok));
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "run")   return cmdRun(argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "test")  return cmdTest(argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "bench") return cmdBench(argv[2]);
    std::cerr << "nesxi — native NYoesyx interpreter\n"
                 "  nesxi run   <file.nesx>     run a program, print a trace\n"
                 "  nesxi test  <dir>           run every *.nesx in <dir> as a test\n"
                 "  nesxi bench <dir>           token-efficiency: .nesx vs .py pairs\n";
    return 64;
}
