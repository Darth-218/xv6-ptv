#!/usr/bin/env python3
"""Convert specs.md to a polished HTML page with code shown as git diffs."""

import re
import sys
import subprocess
from pathlib import Path

def slugify(text):
    s = text.lower().strip()
    s = re.sub(r'[^a-z0-9\s-]', '', s)
    s = re.sub(r'[\s]+', '-', s)
    return s

def get_project_diffs():
    """Get git diff for each project file. Returns dict: filename → diff_text."""
    files = [
        "kernel/pinfo.h", "kernel/syscall.h", "kernel/syscall.c",
        "kernel/sysproc.c", "user/usys.pl", "user/user.h",
        "Makefile", "test-xv6.py", "user/zombie.c",
        "user/ptv.c", "user/forktree.c", "user/orphantree.c",
    ]
    diffs = {}
    BASE = "12a4b12^"
    for f in files:
        try:
            result = subprocess.run(
                ["git", "diff", BASE, "HEAD", "--", f],
                capture_output=True, text=True, cwd=Path(__file__).parent.parent.parent
            )
            if result.stdout.strip():
                diffs[f] = result.stdout
        except Exception:
            pass
    return diffs

def mark_line_as_diff(line):
    """Prepend + to non-diff-metadata lines in a diff."""
    if line.startswith('diff --git') or line.startswith('index ') or line.startswith('new file') or line.startswith('---') or line.startswith('+++') or line.startswith('@@'):
        return line
    if line.startswith('+') or line.startswith('-'):
        return line
    return '+' + line

def parse_markdown(text, diffs):
    lines = text.split('\n')
    html_parts = []
    i = 0
    n = len(lines)

    in_p = False
    section_stack = []
    shown_diffs = set()

    def close_p():
        nonlocal in_p
        if in_p:
            html_parts.append('</p>\n')
            in_p = False

    def get_current_section():
        return ' — '.join(section_stack)

    while i < n:
        line = lines[i]

        # Headers — update section context stack
        m = re.match(r'^(#{1,6})\s+(.+?)(?:\s*\{#(\S+)\})?\s*$', line)
        if m:
            close_p()
            level = len(m.group(1))
            title = m.group(2)
            custom_id = m.group(3)
            id_attr = custom_id or slugify(title)
            if level <= 4:
                section_stack = section_stack[:level-1] + [title]
            html_parts.append(f'<h{level} id="{id_attr}">{title}<a class="header-anchor" href="#{id_attr}">#</a></h{level}>\n')
            i += 1
            continue

        # Fenced code blocks
        if line.startswith('```'):
            close_p()
            lang = line[3:].strip()
            code_lines = []
            i += 1
            while i < n and not lines[i].startswith('```'):
                code_lines.append(lines[i])
                i += 1
            i += 1
            code = '\n'.join(code_lines)
            escaped = escape_html(code)

            if lang == 'mermaid':
                html_parts.append(f'<div class="mermaid-wrapper"><pre class="mermaid">{escaped}</pre></div>\n')
                continue

            is_source = is_source_code_block(code_lines, lang)

            if is_source:
                matched_file = match_code_to_file(code_lines, diffs, get_current_section())
                if matched_file and matched_file in diffs:
                    if matched_file not in shown_diffs:
                        shown_diffs.add(matched_file)
                        diff_text = diffs[matched_file]
                        relevant = extract_relevant_hunks(diff_text, code_lines)
                        diff_escaped = escape_html(relevant)
                        html_parts.append(
                            f'<div class="diff-header">📄 {matched_file}</div>'
                            f'<pre><code class="language-diff">{diff_escaped}</code></pre>\n'
                        )
                    else:
                        lang_class = f' class="language-{lang}"' if lang else ''
                        html_parts.append(
                            f'<pre><code{lang_class}>{escaped}</code></pre>\n'
                        )
                else:
                    diff_lines = [mark_line_as_diff(l) for l in code_lines]
                    diff_code = '\n'.join(diff_lines)
                    diff_escaped = escape_html(diff_code)
                    html_parts.append(f'<pre><code class="language-diff">{diff_escaped}</code></pre>\n')
            else:
                lang_class = f' class="language-{lang}"' if lang else ''
                html_parts.append(f'<pre><code{lang_class}>{escaped}</code></pre>\n')
            continue

        # Horizontal rule
        if re.match(r'^---+$', line.strip()):
            close_p()
            html_parts.append('<hr>\n')
            i += 1
            continue

        # Tables
        if '|' in line and i + 1 < n and re.match(r'^[\s|:,-]+$', lines[i+1]):
            close_p()
            html_parts.append(parse_table(lines, i))
            while i < n and '|' in lines[i]:
                i += 1
            continue

        # Blockquotes
        if line.startswith('> '):
            close_p()
            quote_lines = []
            while i < n and lines[i].startswith('> '):
                quote_lines.append(lines[i][2:])
                i += 1
            html_parts.append(f'<blockquote><p>{"<br>".join(escape_html(l) for l in quote_lines)}</p></blockquote>\n')
            continue

        # Unordered list
        if re.match(r'^[\s]*[-*+]\s+', line):
            close_p()
            list_html, i = parse_list(lines, i, 'ul')
            html_parts.append(list_html)
            continue

        # Ordered list
        if re.match(r'^[\s]*\d+\.\s+', line):
            close_p()
            list_html, i = parse_list(lines, i, 'ol')
            html_parts.append(list_html)
            continue

        # Empty line
        if not line.strip():
            close_p()
            html_parts.append('\n')
            i += 1
            continue

        # Regular paragraph
        if not in_p:
            html_parts.append('<p>')
            in_p = True
        else:
            html_parts.append('\n')

        html_parts.append(parse_inline(line))
        i += 1

    close_p()
    return ''.join(html_parts)

def is_source_code_block(lines, lang):
    """Detect if a code block contains source code vs example output."""
    if not lines:
        return False

    # ASCII art / diagrams with box-drawing characters → not source code
    box_chars = set('─│┌┐└┘├┤┬┴┼═║╒╓╔╕╖╗╘╙╚╛╜╝╞╟╠╡╢╣╤╥╦╧╨╩╪╫╬▄▀█░▒▓■□●')
    box_line_count = sum(1 for l in lines if any(c in l for c in box_chars))
    if box_line_count >= 2:
        return False

    # Lang-based detection
    source_langs = {'c', 'python', 'perl', 'makefile', 'asm'}
    if lang in source_langs:
        return True
    if lang == 'diff':
        return True

    # Content-based detection: look for code patterns
    first = lines[0].strip()
    source_patterns = [
        r'^#include', r'^#define', r'^#ifndef', r'^#endif',
        r'^int\s+\w+\s*\(', r'^void\s+\w+\s*\(', r'^uint64\s+\w+',
        r'^struct\s+\w+', r'^static\s+', r'^extern\s+',
        r'^entry\(', r'^#!/',
        r'^\w+\(', r'^\}', r'^    ', r'^\t',
        r'^  └──', r'^      └──',  # tree output
    ]
    for pat in source_patterns:
        if re.search(pat, first):
            if re.match(r'^\w+\(\d+', first):
                return False
            return True

    if re.match(r'^\w+\(\d+[,\)]', first):
        return False

    return False

def match_code_to_file(lines, diffs, section=''):
    """Try to identify which file a code block belongs to.

    Uses section context first, then content-based heuristics.
    """
    combined = '\n'.join(lines)

    # 0. Detect the code block's language from its first lines
    block_lang = detect_block_lang(lines)

    # 1. Section header mentions a filename like "### 3.1 kernel/pinfo.h"
    #    or "### 4.1 `user/usys.pl`" or "## 7. Build System — Makefile"
    section_file = extract_filename_from_section(section)
    if section_file and block_lang_matches_file(block_lang, section_file):
        return section_file

    # 2. First-line comment with filename ("// user/ptv.c")
    first_line = lines[0].strip() if lines else ''
    comment_file = re.match(r'^(?://|#)\s*((?:kernel|user)/\S+)', first_line)
    if comment_file:
        return comment_file.group(1)

    # 3. Function-based matching (only match if func appears as a definition)
    func_map = {
        'sys_getprocs': 'kernel/sysproc.c',
        'print_process': 'user/ptv.c',
        'fork_tree': 'user/forktree.c',
        'test_ptv': 'test-xv6.py',
    }
    for func, fname in func_map.items():
        if re.search(r'^\s*(?:int\s+|void\s+|uint64\s+)?' + re.escape(func) + r'\s*\(', combined, re.MULTILINE):
            if fname in diffs:
                return fname

    return None


def extract_filename_from_section(section):
    """Extract a project filename from a section header like
    '### 3.1 kernel/pinfo.h — Shared Data Structure'
    or '## 7. Build System — Makefile'
    """
    # Direct path match: kernel/foo.h, user/foo.c, user/foo.pl, test-xv6.py
    m = re.search(r'`?(kernel|user)/\S+?\.(?:[ch]|pl|py)', section)
    if m:
        return m.group(0).lstrip('`')
    # Match known top-level filenames like "Makefile" or "test-xv6.py"
    m = re.search(r'`?(Makefile|test-xv6\.py)', section)
    if m:
        return m.group(0).lstrip('`')
    return None


_BLOCK_LANG_MAP = {
    'c': {'.c', '.h'},
    'perl': {'.pl'},
    'python': {'.py'},
    'makefile': {'Makefile', 'makefile'},
    'asm': {'.S', '.s'},
}

def detect_block_lang(lines):
    """Guess the language of a code block from content."""
    if not lines:
        return None
    first = lines[0].strip()
    if first.startswith('#include') or first.startswith('#define') or first.startswith('#ifndef'):
        return 'c'
    if first.startswith('#!/usr/bin/perl'):
        return 'perl'
    if first.startswith('#!/') and 'python' in first:
        return 'python'
    if first.startswith('.global') or first.startswith('.globl'):
        return 'asm'
    if first.startswith('entry('):
        return 'perl'
    return None

def block_lang_matches_file(lang, filename):
    """Check whether a block's language is appropriate for the given file."""
    if lang is None:
        return True
    exts = _BLOCK_LANG_MAP.get(lang)
    if exts is None:
        return True
    for ext in exts:
        if filename.endswith(ext) or filename == ext:
            return True
    return False

def extract_relevant_hunks(diff_text, code_lines):
    sig_lines = [l.strip() for l in code_lines if l.strip() and l.strip() not in ('{', '}', ');', ');', '}', '};')]
    if not sig_lines:
        return diff_text

    lines = diff_text.split('\n')

    first_idx = None
    last_idx = None
    first_line = sig_lines[0]
    last_line = sig_lines[-1]

    for i, dl in enumerate(lines):
        stripped = dl.lstrip('+-').strip()
        if first_idx is None and (first_line in dl or first_line == stripped):
            first_idx = i
        if last_line in dl or last_line == stripped:
            last_idx = i

    if first_idx is None or last_idx is None:
        return diff_text

    hunk_start = first_idx
    for i in range(first_idx, -1, -1):
        if lines[i].startswith('@@'):
            hunk_start = i
            break

    context = 3
    hunk_end = min(last_idx + context, len(lines) - 1)

    header = lines[:hunk_start]
    body = lines[hunk_start:hunk_end + 1]

    return '\n'.join(header + body)


def parse_table(lines, start):
    header = lines[start]
    result = '<table>\n<thead>\n<tr>'
    for cell in split_table_row(header):
        result += f'<th>{parse_inline(cell.strip())}</th>'
    result += '</tr>\n</thead>\n<tbody>\n'
    row = start + 2
    while row < len(lines) and '|' in lines[row]:
        result += '<tr>'
        for cell in split_table_row(lines[row]):
            result += f'<td>{parse_inline(cell.strip())}</td>'
        result += '</tr>\n'
        row += 1
    result += '</tbody>\n</table>\n'
    return result

def split_table_row(row):
    cells = row.split('|')
    if cells and cells[0].strip() == '':
        cells = cells[1:]
    if cells and cells[-1].strip() == '':
        cells = cells[:-1]
    return cells

def parse_list(lines, start, list_type):
    result = f'<{list_type}>\n'
    i = start
    while i < len(lines):
        m = re.match(r'^([\s]*)[-*+]\s+(.+)', lines[i])
        if not m:
            m = re.match(r'^([\s]*)\d+\.\s+(.+)', lines[i])
        if not m:
            break
        content = m.group(2)
        result += f'<li>{parse_inline(content)}</li>\n'
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r'^[\s]*[-*+]\s+', lines[i]) and not re.match(r'^[\s]*\d+\.\s+', lines[i]) and not lines[i].startswith('#'):
            result += f'<br>{parse_inline(lines[i])}'
            i += 1
    result += f'</{list_type}>\n'
    return result, i

def escape_html(text):
    text = text.replace('&', '&amp;')
    text = text.replace('<', '&lt;')
    text = text.replace('>', '&gt;')
    return text

def parse_inline(text):
    text = escape_html(text)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'(?<!\*)\*([^*]+?)\*(?!\*)', r'<em>\1</em>', text)
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    return text

def build_toc(html_content):
    toc = []
    for m in re.finditer(r'<h(\d) id="([^"]+)">(.+?)<a class="header-anchor"', html_content):
        level = int(m.group(1))
        id_ = m.group(2)
        title = re.sub(r'<[^>]+>', '', m.group(3)).strip()
        toc.append((level, id_, title))
    return toc

def build_html(md_content, diffs):
    body = parse_markdown(md_content, diffs)
    toc_entries = build_toc(body)

    toc_html = '<nav class="toc">\n'
    toc_html += '<div class="toc-header">'
    toc_html += '<button id="theme-toggle">🌙</button>\n'
    toc_html += '<button id="toggle-all">▼ Expand All</button></div>\n'
    toc_html += '<input id="search-input" type="text" placeholder="Search sections...">\n'
    for level, id_, title in toc_entries:
        cls = f'toc-h{level}' if level <= 4 else 'toc-h4'
        toc_html += f'<a href="#{id_}" class="{cls}">{escape_html(title)}</a>\n'
    toc_html += '</nav>\n'

    html = f'''<!DOCTYPE html>
<html lang="en" data-theme="light">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>xv6 Process Tree Visualizer (ptv) — Specification</title>
<link rel="stylesheet" href="css/style.css">
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css" id="hljs-light">
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css" id="hljs-dark" disabled>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/c.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/diff.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/python.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/perl.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/makefile.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.min.js"></script>
</head>
<body>
{toc_html}
<main>
{body}
</main>
<script src="js/interactivity.js"></script>
<script>
hljs.highlightAll();
mermaid.initialize({{
  startOnLoad: true,
  theme: document.documentElement.getAttribute('data-theme') === 'dark' ? 'dark' : 'default',
  securityLevel: 'loose',
}});
const observer = new MutationObserver(() => {{
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  document.getElementById('hljs-light').disabled = isDark;
  document.getElementById('hljs-dark').disabled = !isDark;
  mermaid.run({{ nodes: document.querySelectorAll('.mermaid') }});
}});
observer.observe(document.documentElement, {{ attributes: true, attributeFilter: ['data-theme'] }});
</script>
</body>
</html>'''

    return html

def main():
    md_path = Path(__file__).parent.parent / 'specs.md'
    if not md_path.exists():
        print(f"Error: {md_path} not found", file=sys.stderr)
        sys.exit(1)

    print("Getting git diffs...")
    diffs = get_project_diffs()
    print(f"  Found {len(diffs)} file diffs")

    md_content = md_path.read_text()
    html = build_html(md_content, diffs)

    out_path = Path(__file__).parent / 'index.html'
    out_path.write_text(html)
    print(f"Generated {out_path} ({len(html)} bytes)")

if __name__ == '__main__':
    main()
