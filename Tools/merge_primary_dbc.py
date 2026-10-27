#!/usr/bin/env python3
"""Unisce il primary.dbc di libcan-sw con i dbc degli acquisinator in un unico dbc.

Uso: python merge_primary_dbc.py [--primary PATH] [--out PATH] [acquisinator.dbc ...]

Il primary viene copiato cosi' com'e' (CRLF, attributi, VAL_): i messaggi degli acquisinator
vengono inseriti nel testo, ognuno nella sua sezione (BO_ prima dei CM_, i CM_ in coda ai CM_,
SIG_VALTYPE_ in fondo), e il nodo Acquisinator viene aggiunto a BU_. Niente ri-dump con
cantools: perderebbe le definizioni degli attributi del primary (es. StandardName).
Si ferma se un ID o un nome di messaggio e' gia' usato dal primary. libcan-sw non viene toccato.
"""

import argparse
import re
import sys
from pathlib import Path

import cantools

TOOLS_DIR = Path(__file__).resolve().parent
DEFAULT_PRIMARY = TOOLS_DIR.parent.parent / "libcan-sw" / "dbc" / "primary" / "primary.dbc"
DEFAULT_SOURCES = [TOOLS_DIR / "acquisinator_hydra_adc.dbc", TOOLS_DIR / "acquisinator_lockin.dbc"]
DEFAULT_OUT = TOOLS_DIR / "primary_with_acquisinators.dbc"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sources", nargs="*", type=Path, default=DEFAULT_SOURCES, help="dbc degli acquisinator")
    parser.add_argument("--primary", type=Path, default=DEFAULT_PRIMARY, help="primary.dbc di libcan-sw")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="dbc generato")
    return parser.parse_args()


def split_source(text: str) -> tuple[set[str], list[str], list[str], list[str]]:
    """Nodi, blocchi BO_ (con i loro SG_), righe CM_ e righe SIG_VALTYPE_ di un dbc."""
    lines = text.splitlines()
    nodes = set()
    blocks, comments, valtypes = [], [], []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("BU_:"):
            nodes.update(line[len("BU_:") :].split())
        elif line.startswith("BO_ "):
            block = [line]
            while i + 1 < len(lines) and lines[i + 1].startswith(" SG_ "):
                i += 1
                block.append(lines[i])
            blocks.append("\n".join(block))
        elif line.startswith("CM_ "):
            comments.append(line)
        elif line.startswith("SIG_VALTYPE_ "):
            valtypes.append(line)
        i += 1
    return nodes, blocks, comments, valtypes


def main() -> None:
    args = parse_args()
    primary_db = cantools.database.load_file(args.primary, strict=False)
    used_ids = {m.frame_id: m.name for m in primary_db.messages}
    used_names = set(used_ids.values())

    nodes, blocks, comments, valtypes = set(), [], [], []
    for source in args.sources:
        for message in cantools.database.load_file(source).messages:
            if message.frame_id in used_ids:
                sys.exit(f"{source.name}: {message.name} usa 0x{message.frame_id:X}, gia' di {used_ids[message.frame_id]}")
            if message.name in used_names:
                sys.exit(f"{source.name}: il nome {message.name} esiste gia'")
            used_ids[message.frame_id] = message.name
            used_names.add(message.name)
        n, b, c, v = split_source(source.read_text())
        nodes |= n
        blocks += b
        comments += c
        valtypes += v

    # newline="" mantiene i CRLF del primary cosi' come sono
    with open(args.primary, newline="") as f:
        text = f.read()
    eol = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(eol)

    bu = next(i for i, line in enumerate(lines) if line.startswith("BU_:"))
    present = set(lines[bu][len("BU_:") :].split())
    lines[bu] = lines[bu].rstrip() + "".join(f" {node}" for node in sorted(nodes - present))

    # BO_ subito prima del primo CM_ (o della prima sezione dopo i messaggi)
    first_after_messages = next(
        i for i, line in enumerate(lines) if re.match(r"(CM_|BA_DEF_|BA_|VAL_|SIG_VALTYPE_) ", line)
    )
    new_messages = []
    for block in blocks:
        new_messages += block.split("\n") + [""]
    lines[first_after_messages:first_after_messages] = new_messages

    # CM_ in coda ai CM_ del primary
    last_comment = max((i for i, line in enumerate(lines) if line.startswith("CM_ ")), default=None)
    insert_at = last_comment + 1 if last_comment is not None else first_after_messages + len(new_messages)
    lines[insert_at:insert_at] = comments

    # SIG_VALTYPE_ in fondo, prima delle righe vuote finali
    end = len(lines)
    while end > 0 and not lines[end - 1].strip():
        end -= 1
    lines[end:end] = [""] + valtypes

    with open(args.out, "w", newline="") as f:
        f.write(eol.join(lines))

    merged = cantools.database.load_file(args.out)
    print(f"{args.out}: {len(merged.messages)} messaggi ({len(primary_db.messages)} primary + {len(blocks)} acquisinator)")


if __name__ == "__main__":
    main()
