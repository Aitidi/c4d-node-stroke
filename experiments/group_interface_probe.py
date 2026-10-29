"""Read-only Cinema 4D probe for group interface metadata.

Run inside C4D with an active node material. The report intentionally omits
port values (which can contain filenames); it records IDs, order, layout data,
and connection endpoints only. No graph transaction or UI automation is used.
"""
import c4d
import maxon
from pathlib import Path

lines = []
doc = c4d.documents.GetActiveDocument()
material = doc.GetActiveMaterial() if doc else None
if not material:
    raise RuntimeError("Select a node material before running the probe")
graph = material.GetNodeMaterialReference().GetGraph(c4d.GetActiveNodeSpaceId())

def walk(container, depth=0):
    if depth > 64:
        raise RuntimeError("Node group depth exceeds probe limit")
    children = [n for n in container.GetChildren() if n.GetKind() == maxon.NODE_KIND.NODE]
    if depth:
        lines.append("GROUP " + str(container.GetPath()))
        for label, ports in (("input", container.GetInputs()), ("output", container.GetOutputs())):
            for port in ports.GetChildren():
                lines.append("  PORT " + label + " " + str(port.GetId()) + " name=" + str(port.GetValue(maxon.EffectiveName)))
                for key, value in port.GetValues(maxon.GraphAttributeInterface.FLAGS.DIRECT):
                    if str(key) in ("net.maxon.node.attribute.orderindex", "widgetDataBlackBox", "widgetDataBlackBoxOut"):
                        lines.append("    " + str(key) + "=" + str(value))
                for direction in (maxon.PORT_DIR.INPUT, maxon.PORT_DIR.OUTPUT):
                    for other, _ in port.GetConnections(direction):
                        lines.append("    connection " + str(direction) + " " + str(other.GetPath()))
    for child in children:
        if any(n.GetKind() == maxon.NODE_KIND.NODE for n in child.GetChildren()):
            walk(child, depth + 1)

walk(graph.GetViewRoot())
path = Path(r"D:\AgentProject\c4d-node-stroke\experiments\group_interface_probe.txt")
path.write_text("\n".join(lines), encoding="utf-8")
print("Group interface probe wrote", len(lines), "lines")
