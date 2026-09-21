#!/usr/bin/env python3
"""Documentation coverage gate for cflatten.

Fails when any function definition, struct/union/enum definition,
struct member or enumerator lacks Doxygen documentation, or when a
@param name does not match the signature.  Prototypes need no docs:
definitions carry them.

Usage: check-docs.py src/main.c src/parse/*.c ...
"""
import re
import sys

FUNC_RE = re.compile(r'^([A-Za-z_][\w\s\*]*?)\b([A-Za-z_]\w*)\(')
SKIP = {'if', 'for', 'while', 'switch', 'return', 'sizeof'}
failures = 0


def fail(path, nr, msg):
    global failures
    print('%s:%d: %s' % (path, nr, msg))
    failures += 1


def signature_params(sig):
    """Parameter names of a C declarator (skips 'void')."""
    inner = sig[sig.index('(') + 1:sig.rindex(')')]
    depth = 0
    parts, cur = [], ''
    for ch in inner:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        elif ch == ',' and depth == 0:
            parts.append(cur)
            cur = ''
            continue
        cur += ch
    parts.append(cur)
    names = []
    for part in parts:
        part = part.strip()
        if not part or part == 'void' or part == '...':
            continue
        toks = re.findall(r'[A-Za-z_]\w*', part)
        if toks:
            names.append(toks[-1])
    return names


def check_file(path):
    lines = open(path).readlines()
    n = len(lines)

    # ---- function definitions ----
    i = 0
    while i < n:
        m = FUNC_RE.match(lines[i])
        if m and m.group(2) not in SKIP:
            name = m.group(2)
            j = i
            buf = ''
            depth = 0
            started = False
            while j < n:
                for ch in lines[j]:
                    if ch == '(':
                        depth += 1
                        started = True
                    elif ch == ')':
                        depth -= 1
                buf += lines[j]
                if started and depth == 0:
                    break
                j += 1
            nxt = lines[j + 1] if j + 1 < n else ''
            if nxt.strip() == '{' and not buf.rstrip().endswith(';'):
                pre = lines[i - 1].strip() if i > 0 else ''
                doc = ''.join(lines[max(0, i - 16):i])
                if not (pre == '*/' and '@brief' in doc):
                    fail(path, i + 1,
                         'undocumented function %s' % name)
                else:
                    for pn in signature_params(buf):
                        if '@param ' + pn not in doc:
                            fail(path, i + 1,
                                 '%s: @param %s missing/mismatched' %
                                 (name, pn))
        i += 1

    # ---- struct/union/enum blocks ----
    i = 0
    while i < n:
        ln = lines[i]
        m = re.match(r'^(\s*)(typedef\s+)?(struct|union|enum)\b(.*)$', ln)
        if m and '{' in ln:
            indent, is_td, kind, rest = m.groups()
            # find closing brace at same indent level
            depth = ln.count('{') - ln.count('}')
            j = i
            while depth > 0 and j + 1 < n:
                j += 1
                code = re.sub(r'"(?:[^"\\]|\\.)*"', '""', lines[j])
                code = re.sub(r"'(?:[^'\\]|\\.)*'", "'c'", code)
                code = re.sub(r'/\*.*?\*/', '', code)
                depth += code.count('{') - code.count('}')
            # named definition requires a doc block above
            tail = lines[j].strip() if j < n else ''
            named = bool(re.match(r'^\}\s*[A-Za-z_]\w*\s*;\s*$', tail))
            one_liner = (j == i)
            if named and not one_liner:
                # locate typedef/enum opener for the doc check
                s = i
                while s >= 0 and not lines[s].lstrip().startswith(
                        ('typedef', 'enum', 'struct', 'union')):
                    s -= 1
                prev = ''.join(lines[max(0, s - 8):s])
                if '@brief' not in prev:
                    fail(path, s + 1, 'undocumented type definition')
            # members / enumerators need trailing docs
            if not one_liner:
                for k in range(i + 1, j):
                    s = lines[k].strip()
                    if not s or s.startswith(('*', '/', '#')):
                        continue
                    if s.startswith('}'):
                        continue
                    if kind == 'enum':
                        if re.match(r'^[A-Za-z_]\w*(\s*=.*)?,?\s*$', s) \
                                and '/**<' not in lines[k]:
                            fail(path, k + 1, 'undocumented enumerator')
                    else:
                        if s.endswith(';') and '/**<' not in lines[k]:
                            fail(path, k + 1, 'undocumented member')
            i = j + 1
            continue
        i += 1


for p in sys.argv[1:]:
    check_file(p)

if failures:
    print('%d documentation gap(s)' % failures)
    sys.exit(1)
print('docs OK: %s' % ' '.join(sys.argv[1:]))
