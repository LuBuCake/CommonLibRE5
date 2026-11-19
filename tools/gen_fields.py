"""Build the field database of re5dx9.exe: include/RE/IDs_Fields.h (RE::FIELD, RE::PROP).

Inputs:
  <export>/properties*.tsv   MT properties from tools/PropertyDump: properties.tsv (every class on a zeroed object,
                             2026-11-14) and properties_live*.tsv (live objects: complete, address in column 1)
  <classes.csv>              class tree (Tools/Ghidra Addons/RE5 Ghidra/tools/mtdti_classes.py)
  data/Fields.tsv            hand-given fields (state the engine does not reflect: health, inventory...)
  data/Functions.tsv         the `ret N` of each getter/setter (tools/gen_functions.py)

RE::FIELD::<class>::<field> = FieldID<T>{ offset } for fields; RE::PROP::<class>::<property> = PropID<T, kind>{ get, set }
for properties read through accessor functions. A class namespace pulls its parent's (using-directive), so
RE::FIELD::uPlayer::mPos finds uCoord's. A property goes in the topmost class whose dump has it with the same offset/type.

Usage: python -I tools/gen_fields.py <export dir> <classes.csv> [CommonLibRE5 dir]
"""
import collections
import csv
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I leaves the script's folder out
from gen_functions import BASE, CPP_KEYWORDS, LICENSE, TEXT_BEGIN, TEXT_END, ancestors, load_classes, ns_ident, read_tsv

# MtPropType -> C++ type. "value" properties pass T by value; "pointer" ones through a T* (get fills it, set reads it).
TYPES = {
    2: ("MtObject*", "value"), 3: ("bool", "value"), 4: ("std::uint8_t", "value"), 5: ("std::uint16_t", "value"),
    6: ("std::uint32_t", "value"), 7: ("std::uint64_t", "value"), 8: ("std::int8_t", "value"), 9: ("std::int16_t", "value"),
    10: ("std::int32_t", "value"), 11: ("std::int64_t", "value"), 12: ("float", "value"), 13: ("double", "value"),
    15: ("MtColor", "pointer"), 16: ("MtPoint", "pointer"), 17: ("MtSize", "pointer"), 18: ("MtRect", "pointer"),
    19: ("MtMatrix", "pointer"), 20: ("MtVector3", "pointer"), 21: ("MtVector4", "pointer"), 22: ("MtQuaternion", "pointer"),
}
# Only properties (accessors): the getter returns the string / an object pointer, the field layout is not known.
PROP_ONLY = {14: ("const char*", "value"), 32: ("std::uint32_t", "value"), 58: ("void*", "value")}
NO_DATA = {25, 26, 27, 28, 31}  # editor group label, events, list end
ARRAY, GETSET = 0x20, 0x80


def load_properties(export):
    """(class, name, type, attr, offset or None, get, set, count, owner) of every property in every dump, without
    duplicates. owner (the `this` of the accessors, PropertyDump's last column): 0 = the object, K = a sub-object at
    +K, "outside" = a separate object, None = a dump older than the column (2026-11-19)."""
    keys = ["class", "name", "type", "attr", "offset", "count", "setter", "owner"]
    rows = []
    for path in sorted(glob.glob(os.path.join(export, "properties*.tsv"))):
        with open(path, encoding="utf-8", newline="") as f:
            for line in f:
                cols = line.rstrip("\r\n").split("\t")
                if cols[0] == "class":
                    continue
                if re.fullmatch(r"[0-9A-F]{8}", cols[0]):  # live dump: object address first
                    cols = cols[1:]
                # Some names end in tabs in the game itself ("mbDropEnable\t\t"): the fixed columns are read from
                # the right. The owner column exists in dumps made after 2026-11-19.
                owner = cols.pop() if re.fullmatch(r"this|\+0x[0-9A-F]+|@0x[0-9A-F]+", cols[-1]) else ""
                if len(cols) < 7:
                    continue
                name = "\t".join(cols[1:-5]).strip()
                rows.append(dict(zip(keys, [cols[0], name] + cols[-5:] + [owner])))
    out, seen = [], set()
    for r in rows:
        if not r["type"].isdigit() or int(r["type"]) in NO_DATA or not r["name"]:
            continue
        attr = int(r["attr"], 16) if r["attr"] else 0
        offset = int(r["offset"][1:], 16) if r["offset"].startswith("+") else None
        get = int(r["offset"][4:], 16) if r["offset"].startswith("get 0x") else 0
        setter = int(r["setter"], 16) if r["setter"] else 0
        if offset is None and not get:
            continue
        o = r.get("owner") or ""
        owner = 0 if o == "this" else int(o[1:], 16) if o.startswith("+") else "outside" if o.startswith("@") else None
        item = (r["class"], r["name"], int(r["type"]), attr & (ARRAY | GETSET), offset, get,
                setter if TEXT_BEGIN <= setter < TEXT_END else 0, int(r["count"]) if r["count"].isdigit() else 0, owner)
        key = item[:7] + (item[8],)
        if key not in seen:
            seen.add(key)
            out.append(item)
    return out


def ident(name):
    s = re.sub(r"[^0-9A-Za-z_]+", "_", name).strip("_") or "_"
    s = "_" + s if s[0].isdigit() else s
    return s + "_" if s in CPP_KEYWORDS else s


def main():
    export, classes_csv = sys.argv[1], sys.argv[2]
    lib = sys.argv[3] if len(sys.argv) > 3 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parent = load_classes(classes_csv)
    purge = {}
    for r in read_tsv(os.path.join(lib, "data", "Functions.tsv")):
        purge[int(r["va"], 16)] = int(r["purge"]) if r["purge"] else None

    props = load_properties(export)
    by_class = collections.defaultdict(set)
    for p in props:
        by_class[p[0]].add(p[1:7] + (p[8],))

    def declared_above(c, key):
        """The nearest ancestor with a dump has the same entry: c inherits it."""
        for a in ancestors(parent, c)[1:]:
            if a in by_class:
                return key in by_class[a]
        return False

    fields = collections.defaultdict(dict)  # class -> name -> (cpp type, offset, note)
    accessors = collections.defaultdict(dict)  # class -> name -> (cpp type, kind, get, set, this offset)
    skipped = collections.Counter()
    for c, name, typ, attr, offset, get, setter, count, owner in props:
        if declared_above(c, (name, typ, attr, offset, get, setter, owner)):
            continue
        n = ident(name)
        if offset is not None:
            cpp = TYPES.get(typ, (None,))[0]
            if typ == 1:
                cpp = "MtObject" if not attr & ARRAY else None  # embedded object; element size of an array unknown
            if cpp and attr & ARRAY and count > 1:
                cpp = f"{cpp}[{count}]"
            note = f"MtPropType {typ}" + (f"[{count}]" if attr & ARRAY else "")
            entry = (cpp or "void", offset, note if not cpp or cpp == "MtObject" else "")
            store, key = fields[c], n
        else:
            # The accessors run on MtProperty::owner, not always the object: uModel::RenderInfo registers mTransparency
            # for every model (called on a uPlayer it crashed, 2026-11-19). Unknown or separate owner: no id.
            if owner is None or owner == "outside":
                skipped["owner unknown" if owner is None else "owner is a separate object"] += 1
                continue
            cpp, kind = TYPES.get(typ) or PROP_ONLY.get(typ) or (None, None)
            if not cpp or (typ == 2 and attr & ARRAY):
                skipped["type"] += 1
                continue
            array = 4 if attr & ARRAY else 0
            size = 8 if cpp in ("std::uint64_t", "std::int64_t", "double") else 4
            want_get, want_set = (0 if kind == "value" else 4) + array, (size if kind == "value" else 4) + array
            if purge.get(get) not in (None, want_get) or (setter and purge.get(setter) not in (None, want_set)):
                skipped["ret N"] += 1  # the accessor does not pop what the rule says: no typed call
                continue
            entry = (cpp, kind + ("Array" if array else ""), get, setter, owner)
            store, key = accessors[c], n
        if key in store and store[key] != entry:
            key = f"{n}_{(offset if offset is not None else get - BASE):X}"
        store[key] = entry

    # Hand-given fields win over the dump (same name in the same class).
    for r in read_tsv(os.path.join(lib, "data", "Fields.tsv")):
        fields[r["class"]][r["name"]] = (r["type"], int(r["offset"], 16), r["note"])

    path = os.path.join(lib, "include", "RE", "IDs_Fields.h")
    n_f, n_p = write_header(path, fields, accessors, parent)
    print(f"{len(props)} properties in the dumps -> IDs_Fields.h: {n_f} fields in {len(fields)} classes, "
          f"{n_p} accessor properties in {len(accessors)} classes (skipped: {dict(skipped)})")


def write_header(path, fields, accessors, parent):
    out = [LICENSE, "#pragma once\n\n",
           "// Generated by tools/gen_fields.py from the MT property dumps and data/Fields.tsv (runtime Steam 2023). Do not edit.\n"
           "// RE::FIELD::<class>::<field> = FieldID<T>{ offset }: RE::Get(object, id) / object->Get(id) is a T&.\n"
           "// RE::PROP::<class>::<property> = PropID<T, kind>{ getter, setter[, sub-object offset] }: RE::Get / RE::Set call\n"
           "// the game's accessors, on the sub-object that registers the property when it is not the object itself.\n"
           "// A class sees the fields and properties of its ancestors (RE::FIELD::uPlayer::mPos is uCoord's).\n\n",
           '#include "RE/MT/MtField.h"\n\n']
    # Game classes named in the hand-given types (uPlayer*, cItem): declared, defined later by their headers.
    used = {w for e in (x for t in fields.values() for x in t.values()) for w in re.findall(r"[A-Za-z_]\w*", e[0])}
    declared = sorted(w for w in used if w in parent and w != "MtObject")
    if declared:
        out.append("namespace RE\n{\n" + "".join(f"    class {w};\n" for w in declared) + "}\n\n")
    def qualify(cpp):
        """Inside RE::FIELD::uPlayer, `uPlayer` (or cItem, MtObject) is a generated namespace: name the types fully."""
        return re.sub(r"(?<![:\w])([A-Za-z_]\w*)", lambda m: "::RE::" + m.group(1) if m.group(1) in parent or
                      m.group(1).startswith("Mt") else m.group(1), cpp)

    counts = []
    for space, table in (("FIELD", fields), ("PROP", accessors)):
        need = set()
        for c in table:
            need.update(ancestors(parent, c) if c in parent else [c])
        order = sorted(need, key=lambda c: (len(ancestors(parent, c)), c.lower()))
        out.append(f"namespace RE::{space}\n{{\n")
        count = 0
        for c in order:
            up = parent.get(c)
            using = f"using namespace {ns_ident(up)};" if up in need else ""
            comment = f"  // {c}" if ns_ident(c) != c else ""
            own = table.get(c, {})
            if not own:
                out.append(f"    namespace {ns_ident(c)} {{ {using} }}{comment}\n".replace("{  }", "{ }"))
                continue
            out.append(f"    namespace {ns_ident(c)}{comment}\n    {{\n")
            if using:
                out.append(f"        {using}\n")
            for name in sorted(own, key=str.lower):
                e = own[name]
                if space == "FIELD":
                    cpp, offset, note = e
                    out.append(f"        inline constexpr FieldID<{qualify(cpp)}> {name}{{ 0x{offset:X} }};{'  // ' + note if note else ''}\n")
                else:
                    cpp, kind, get, setter, this = e
                    adjust = f", 0x{this:X}" if this else ""
                    out.append(f"        inline constexpr PropID<{qualify(cpp)}, PropKind::k{kind[0].upper() + kind[1:]}> {name}{{ "
                               f"REL::Offset{{ 0x{get - BASE:X} }}, REL::Offset{{ 0x{(setter - BASE) if setter else 0:X} }}{adjust} }};\n")
                count += 1
            out.append("    }\n")
        out.append("}\n\n")
        counts.append(count)
    open(path, "w", encoding="utf-8", newline="\n").write("".join(out).rstrip("\n") + "\n")
    return counts


if __name__ == "__main__":
    main()
