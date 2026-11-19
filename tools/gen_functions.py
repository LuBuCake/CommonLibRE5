"""Build the function database of re5dx9.exe: data/Functions.tsv, include/RE/IDs_Functions.h, include/RE/IDs_VFuncs.h.

Inputs:
  <export>/functions.tsv, <export>/vtables.tsv  written by tools/ghidra/RE5_ExportFunctions.java (Ghidra)
  <export>/properties.tsv                       written by tools/PropertyDump (MT properties, getter/setter addresses)
  <classes.csv>                                 class tree (Tools/Ghidra Addons/RE5 Ghidra/tools/mtdti_classes.py)
  data/Names.tsv, data/VFuncs.tsv               hand-given names (source of truth)

Name of each function, first match wins:
  1. Names.tsv (by address)
  2. vtable slot: the class that introduces the function in a vtable (its parent's vtable has something else in that
     slot) + the slot name from VFuncs.tsv (looked up in the class and its ancestors), or "vfNNN" if the slot has none
  3. MT property getter/setter: get_<Property> / set_<Property>, class = the common ancestor of the classes using it
  4. Ghidra: ctor/dtor/getDTI of RE5_MtDTI.java, library functions (FID), and names given in Ghidra by hand
     (USER_DEFINED; also appended to Names.tsv so they are not lost when Ghidra is re-imported)

Usage: python -I tools/gen_functions.py <export dir> <classes.csv> [CommonLibRE5 dir]
"""
import collections
import csv
import os
import re
import sys

BASE = 0x400000
TEXT_BEGIN, TEXT_END = 0x401000, 0xF4A000  # .text of the dump (section .sec0)
LICENSE = """/*
    This file is part of CommonLibRE5.
    CommonLibRE5 is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.
    CommonLibRE5 is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with CommonLibRE5. If not, see <https://www.gnu.org/licenses/>.
*/

"""
IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
CPP_KEYWORDS = {"delete", "new", "default", "register", "class", "namespace", "operator", "this", "template", "typename"}


def read_tsv(path):
    """TSV with '#' comment lines before the header."""
    with open(path, encoding="utf-8", newline="") as f:
        lines = [line for line in f if not line.startswith("#") and line.strip()]
    return list(csv.DictReader(lines, delimiter="\t"))


def ns_ident(name):
    """Class name as a C++ identifier, same mangling as gen_ids.py (A::B -> A__B)."""
    s = re.sub(r"[^0-9A-Za-z_]", "_", name.replace("::", "__"))
    return "_" + s if s[0].isdigit() else s


def load_classes(path):
    parent = {}
    for r in csv.DictReader(open(path, newline="")):
        parent[r["class"]] = r["parent"] or None
    return parent


def ancestors(parent, c):
    out = []
    while c:
        out.append(c)
        c = parent.get(c)
    return out


def common_ancestor(parent, classes):
    classes = list(classes)
    common = ancestors(parent, classes[0])
    for c in classes[1:]:
        chain = set(ancestors(parent, c))
        common = [a for a in common if a in chain]
    return common[0] if common else ""


class VTables:
    """The vtables of the export, cleaned, with the class that introduces each slot target."""

    def __init__(self, path, parent, functions):
        self.parent = parent
        tables = collections.OrderedDict()
        for r in csv.DictReader(open(path, encoding="utf-8", newline=""), delimiter="\t"):
            key = (r["class"], r["vtable"])
            tables.setdefault(key, {"class": r["class"], "label": r["label"], "va": int(r["vtable"], 16), "targets": [], "flags": []})
            tables[key]["targets"].append(int(r["target"], 16))
            tables[key]["flags"].append(r["flags"])

        # Nodes: the class's own vtable ("vftable") is node <class>; a shared one ("vftable_<addr>": the class and its
        # subclasses without a DTI) is node <class>@<addr>, child of the class's node (or of its nearest ancestor).
        primary = {t["class"] for t in tables.values() if t["label"] == "vftable"}
        self.nodes = {}
        for t in tables.values():
            c = t["class"]
            if t["label"] == "vftable":
                nid, up = c, parent.get(c)
            else:
                nid, up = f"{c}@{t['va']:08X}", c
            while up and up not in primary:
                up = parent.get(up)
            self.nodes[nid] = {"class": c, "parent": up, "va": t["va"], "targets": t["targets"], "flags": t["flags"]}

        # Length, base classes first. The export walked each table to a sure end and flagged the possible ends ("s"
        # slot stored by code, "c" target not the start of code). A derived vtable starts with a copy of its base's,
        # so below the base's length a flag is a false end (a flag constant that matches the address, code Ghidra
        # never disassembled); from there on, the first flag ends the table.
        for nid in sorted(self.nodes, key=self.depth):
            n = self.nodes[nid]
            up = self.nodes[n["parent"]]["length"] if n["parent"] else 1
            k = min(max(up, 1), len(n["targets"]))
            while k < len(n["targets"]) and not n["flags"][k]:
                k += 1
            n["length"] = k
            del n["targets"][k:]
            del n["flags"][k:]

        # Introducers: (class, slot) where the target appears and the parent vtable has something else there.
        self.introducers = collections.defaultdict(list)
        for nid, n in self.nodes.items():
            up = self.nodes.get(n["parent"]) if n["parent"] else None
            for s, t in enumerate(n["targets"]):
                if up and s < len(up["targets"]) and up["targets"][s] == t:
                    continue
                self.introducers[t].append((n["class"], s))

    def depth(self, nid):
        d = 0
        while nid:
            nid = self.nodes[nid]["parent"]
            d += 1
        return d

    def slot_count(self):
        return sum(len(n["targets"]) for n in self.nodes.values())


def load_properties(path, parent):
    """va -> (class, name or "") of every getter/setter. The name only when every use has the same property and role
    (the MT code reuses one getter for fields of the same layout: 1,296 functions for 23,625 properties)."""
    uses = collections.defaultdict(set)
    for r in csv.DictReader(open(path, encoding="utf-8", newline=""), delimiter="\t"):
        prop = re.sub(r"[^0-9A-Za-z_]", "_", r["name"])
        getter = int(r["offset"][4:], 16) if r["offset"].startswith("get 0x") else 0
        setter = int(r["setter"], 16) if r["setter"] else 0
        # The setter column also holds small non-address values for some fields (0x4, 0x14...): code only.
        if TEXT_BEGIN <= getter < TEXT_END:
            uses[getter].add((r["class"], "get_" + prop))
        if TEXT_BEGIN <= setter < TEXT_END:
            uses[setter].add((r["class"], "set_" + prop))
    out = {}
    for va, u in uses.items():
        names = {n for _, n in u}
        out[va] = (common_ancestor(parent, {c for c, _ in u}), names.pop() if len(names) == 1 else "")
    return out


def main():
    export, classes_csv = sys.argv[1], sys.argv[2]
    lib = sys.argv[3] if len(sys.argv) > 3 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data = os.path.join(lib, "data")

    parent = load_classes(classes_csv)
    ghidra = {int(r["va"], 16): r for r in csv.DictReader(open(os.path.join(export, "functions.tsv"), encoding="utf-8", newline=""), delimiter="\t")}
    vt = VTables(os.path.join(export, "vtables.tsv"), parent, set(ghidra))
    props = load_properties(os.path.join(export, "properties.tsv"), parent)

    names_path = os.path.join(data, "Names.tsv")
    names = {int(r["rva"], 16) + BASE: r for r in read_tsv(names_path)}
    vfuncs = {(r["class"], int(r["slot"])): r for r in read_tsv(os.path.join(data, "VFuncs.tsv"))}

    # Names given by hand in Ghidra that Names.tsv does not have yet: append them (Ghidra is an editor of the database).
    added = []
    def slot_name(c, s):
        for a in ancestors(parent, c):
            if (a, s) in vfuncs:
                return vfuncs[(a, s)]["name"], a
        return None, None

    def named_slot(va):
        intro = vt.introducers.get(va, [])
        return len({s for _, s in intro}) == 1 and slot_name(common_ancestor(parent, {c for c, _ in intro}), intro[0][1])[0]

    for va, g in ghidra.items():
        if g["source"] == "USER_DEFINED" and va not in names and g["name"] not in ("ctor", "dtor", "getDTI") and not named_slot(va):
            row = {"rva": f"0x{va - BASE:X}", "class": g["namespace"], "name": g["name"], "signature": "", "note": "[Ghidra]"}
            names[va] = row
            added.append(row)
    if added:
        with open(names_path, "a", encoding="utf-8", newline="\n") as f:
            for r in added:
                f.write("\t".join(r[k] for k in ("rva", "class", "name", "signature", "note")) + "\n")
        print(f"Names.tsv: {len(added)} names taken from Ghidra")

    rows = []
    for va in sorted(set(ghidra) | set(vt.introducers) | set(props)):
        g = ghidra.get(va)
        intro = vt.introducers.get(va, [])
        slots = sorted({s for _, s in intro})
        row = {"va": va, "size": g["size"] if g else "", "purge": g["purge"] if g else "", "class": "", "name": "",
               "kind": "function", "slot": ",".join(map(str, slots)), "origin": "", "signature": ""}
        if g and g["thunk_target"]:
            row["kind"] = "thunk"
        if intro:
            row["kind"] = "virtual"

        if va in names:
            n = names[va]
            row.update({"class": n["class"], "name": n["name"], "origin": "names", "signature": n["signature"]})
        elif intro:
            owner = common_ancestor(parent, {c for c, _ in intro})
            row["class"] = owner
            if len(slots) == 1:
                name, at = slot_name(owner, slots[0])
                if name:
                    row.update({"name": name, "origin": "slot", "signature": vfuncs[(at, slots[0])]["signature"]})
                else:
                    row.update({"name": f"vf{slots[0]:03d}", "origin": "auto"})
            # Same code in several slots (identical functions folded by the linker): owner only, no name.
        elif va in props:
            row["class"], row["name"] = props[va]
            row.update({"kind": "property", "origin": "property" if row["name"] else ""})
        elif g and g["source"] != "DEFAULT":
            row.update({"class": g["namespace"], "name": g["name"], "origin": "ghidra"})
            if g["name"] in ("ctor", "dtor", "getDTI"):
                row["kind"] = g["name"]
            elif not g["namespace"]:
                row["kind"] = "library"
        rows.append(row)

    # The same class/name for two addresses (a property name shared by sibling getters): suffix by address.
    seen = collections.Counter()
    for r in rows:
        if r["name"]:
            key = (r["class"], r["name"])
            seen[key] += 1
            if seen[key] > 1:
                r["name"] = f"{r['name']}_{r['va'] - BASE:X}"

    for r in rows:
        problem = check_purge(r["signature"], r["purge"])
        if problem:
            print(f"warning: {r['class']}::{r['name']} (0x{r['va'] - BASE:X}) {r['signature']!r}: {problem}")

    write_database(os.path.join(data, "Functions.tsv"), rows)
    n_ids = write_ids(os.path.join(lib, "include", "RE", "IDs_Functions.h"), rows)
    n_vf = write_vfuncs(os.path.join(lib, "include", "RE", "IDs_VFuncs.h"), vfuncs, parent, vt)

    by_origin = collections.Counter(r["origin"] or "unnamed" for r in rows)
    missing = sum(1 for r in rows if r["va"] not in ghidra)
    print(f"{len(rows)} functions ({missing} vtable targets / property functions Ghidra has no function for), "
          f"{vt.slot_count()} vtable slots")
    print("names: " + ", ".join(f"{k} {v}" for k, v in by_origin.most_common()))
    print(f"IDs_Functions.h: {n_ids} ids, IDs_VFuncs.h: {n_vf} slots")


def check_purge(signature, purge):
    """The signature against the `ret N` Ghidra saw (a wrong convention unbalances the stack: CommonLibRE5/CLAUDE.md).
    Parameters are counted as 4 bytes except double/int64; unknown ("?") parameter lists are not checked."""
    m = re.search(r"\(([^()]*)\)\s*$", signature or "")
    if not m or purge == "" or "?" in m.group(1):
        return None
    params = [p for p in m.group(1).split(",") if p.strip() and p.strip() != "void"]
    size = sum(8 if re.search(r"\b(double|std::u?int64_t|long long)\b", p) and "*" not in p else 4 for p in params)
    cdecl = "__cdecl" in signature
    expected = 0 if cdecl else size
    if int(purge) != expected:
        conv = "__cdecl" if cdecl else ("__stdcall" if "__stdcall" in signature else "__thiscall")
        return f"{conv} with {size} bytes of parameters should end in ret {expected}, the function does ret {purge}"
    return None


def write_database(path, rows):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# GENERATED by tools/gen_functions.py: every function of re5dx9.exe (Steam 2023). Do not edit: name functions\n"
                "# in Names.tsv / VFuncs.tsv (or in Ghidra) and regenerate. rva = va - 0x400000. kind: virtual (vtable target; slot =\n"
                "# its slot), property (MT property getter/setter), ctor/dtor/getDTI, library (CRT), thunk, function. purge = bytes the\n"
                "# function pops (`ret N`: 0 = __cdecl or no stack arguments). origin: names, slot, property, ghidra, auto (vfNNN).\n")
        f.write("rva\tva\tclass\tname\tkind\tslot\tpurge\tsize\torigin\tsignature\n")
        for r in rows:
            f.write(f"0x{r['va'] - BASE:X}\t{r['va']:08X}\t{r['class']}\t{r['name']}\t{r['kind']}\t{r['slot']}\t{r['purge']}\t"
                    f"{r['size']}\t{r['origin']}\t{r['signature']}\n")


def write_ids(path, rows):
    """RE::ID::<class>::<name> for every named function, except the automatic vfNNN, library functions and getDTI
    (RE::DTI already has what it returns)."""
    by_class = collections.defaultdict(list)
    for r in rows:
        if r["origin"] in ("names", "slot", "property") or r["kind"] in ("ctor", "dtor"):
            if IDENT.match(r["name"]) and r["name"] not in CPP_KEYWORDS and r["name"] != "getDTI":
                by_class[r["class"]].append(r)
    out = [LICENSE, "#pragma once\n\n",
           "// Generated by tools/gen_functions.py from data/Functions.tsv (runtime Steam 2023). Do not edit: name functions in\n"
           "// data/Names.tsv / data/VFuncs.tsv. RE::ID::<class>::<function> = the function's RVA; a virtual function is the\n"
           "// override of that class (call virtuals through RE::VFUNC + REL::InvokeVFunc, hook them in RE::VTABLE).\n\n",
           '#include "REL/Relocation.h"\n\n']
    count = 0
    globals_ = by_class.pop("", [])
    out.append("namespace RE::ID\n{\n")
    for r in globals_:
        out.append(id_line(r, "    "))
        count += 1
    for c in sorted(by_class, key=str.lower):
        out.append(f"    namespace {ns_ident(c)}{'  // ' + c if ns_ident(c) != c else ''}\n    {{\n")
        for r in sorted(by_class[c], key=lambda r: r["name"].lower()):
            out.append(id_line(r, "        "))
            count += 1
        out.append("    }\n")
    out.append("}\n")
    open(path, "w", encoding="utf-8", newline="\n").write("".join(out))
    return count


def id_line(r, indent):
    note = r["signature"] or (f"vfunc {r['slot']}" if r["kind"] == "virtual" else "")
    return f"{indent}inline constexpr REL::Offset {r['name']}{{ 0x{r['va'] - BASE:X} }};{'  // ' + note if note else ''}\n"


def write_vfuncs(path, vfuncs, parent, vt):
    """RE::VFUNC::<class>::<name> = slot. Each class namespace pulls its parent's (using-directive), so a slot named
    in uPawn is also RE::VFUNC::uPlayer::<name>."""
    named = {c for c, _ in vfuncs}
    # Classes that need a namespace: those that have (or inherit) a named slot.
    need = {c for c in parent if any(a in named for a in ancestors(parent, c))}
    order = sorted(need, key=lambda c: (len(ancestors(parent, c)), c.lower()))
    out = [LICENSE, "#pragma once\n\n",
           "// Generated by tools/gen_functions.py from data/VFuncs.tsv (runtime Steam 2023). Do not edit.\n"
           "// RE::VFUNC::<class>::<function> = vtable slot, for REL::InvokeVFunc / Relocation::WriteVFunc. A class sees\n"
           "// the slots named in its ancestors (RE::VFUNC::uPlayer::SubHp = uPawn's slot 69).\n\n",
           "#include <cstddef>\n\nnamespace RE::VFUNC\n{\n"]
    for c in order:
        own = sorted((s, r) for (k, s), r in vfuncs.items() if k == c)
        up = parent.get(c)
        if not own:
            using = f" using namespace {ns_ident(up)};" if up in need else ""
            comment = f"  // {c}" if ns_ident(c) != c else ""
            out.append(f"    namespace {ns_ident(c)} {{{using} }}{comment}\n")
            continue
        out.append(f"    namespace {ns_ident(c)}{'  // ' + c if ns_ident(c) != c else ''}\n    {{\n")
        if up in need:
            out.append(f"        using namespace {ns_ident(up)};\n")
        for s, r in own:
            note = (r["signature"] + " " + r["note"]).strip()
            out.append(f"        inline constexpr std::size_t {r['name']} = {s};{'  // ' + note if note else ''}\n")
        out.append("    }\n")
    out.append("}\n")
    open(path, "w", encoding="utf-8", newline="\n").write("".join(out))
    return len(vfuncs)


if __name__ == "__main__":
    main()
