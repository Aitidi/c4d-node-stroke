"""Read-only static inventory of selected C4D 2027 MVP interface method IDs.

Requires pefile and capstone. Never loads or executes the host binary. Method
hashes do not reveal signatures or provide a pointer to a live editor instance.
"""
from pathlib import Path
import sys

import capstone
from capstone.x86_const import X86_OP_MEM, X86_REG_RIP
import pefile

binary = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    r"C:\Program Files\Maxon Cinema 4D 2027\corelibs\c4d_nodeeditor.xdl64"
)
pe = pefile.PE(str(binary))
data = pe.__data__
base = pe.OPTIONAL_HEADER.ImageBase
ids = (
    "net.maxon.mvp.graphpresenter", "net.maxon.mvp.graphportpresenter",
    "net.maxon.mvp.graphinportpresenter", "net.maxon.mvp.graphoutportpresenter",
    "net.maxon.mvp.canvas", "net.maxon.mvp.widget", "net.maxon.mvp.appwidget",
    "net.maxon.mvp.gluon2.widget2interface", "net.maxon.mvp.gluon2.nodegraphwindowinterface",
    "net.maxon.mvp.gluon2.drawlistinterface", "net.maxon.mvp.graphportlistpresenter",
    "net.maxon.mvp.graphportgrouppresenter",
    "net.maxon.mvp.gluon2.widgetinterface", "net.maxon.mvp.gluon2.layoutwidget2interface",
    "net.maxon.mvp.gluon2.windowinterface", "net.maxon.mvp.graphmodelpresenter",
)
targets = {}
for name in ids:
    offset = data.find(name.encode("ascii") + b"\0")
    if offset >= 0:
        targets[base + pe.get_rva_from_offset(offset)] = name

text_section = next(s for s in pe.sections if s.Name.startswith(b".text"))
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
decoder.detail = True

def rip_address(ins):
    for operand in ins.operands:
        if operand.type == X86_OP_MEM and operand.mem.base == X86_REG_RIP:
            return ins.address + ins.size + operand.mem.disp
    return None

for ins in decoder.disasm(text_section.get_data(), base + text_section.VirtualAddress):
    if ins.mnemonic != "lea" or not ins.op_str.startswith("rdx,"):
        continue
    name = targets.get(rip_address(ins))
    if not name:
        continue
    print("INTERFACE", name, "registration", hex(ins.address - base))
    for candidate in decoder.disasm(pe.get_data(ins.address - base, 65), ins.address):
        if candidate.mnemonic != "lea" or not candidate.op_str.startswith("r9,"):
            continue
        addr = rip_address(candidate)
        try:
            raw = pe.get_data(addr - base, 4096)
        except (ValueError, TypeError):
            continue
        end = raw.find(b"\0\0")
        if end >= 0:
            raw = raw[:end + 1]
        for item in raw.split(b"\0"):
            if item and all(32 <= c < 127 for c in item):
                print(" ", item.decode("ascii"))
