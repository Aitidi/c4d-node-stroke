# Host test helpers for NodeStroke, run inside Cinema 4D 2027 (e.g. via the Cinema 4D MCP exec_python).
# Put the tests folder on sys.path, then: g = host_ns.fresh(); host_ns.select("tex", "mix"); then in a
# SEPARATE exec call c4d.CallCommand(<id>); then read host_ns.wires() in another call. CallCommand and
# DoUndo issued in the same exec call as other edits gave inconsistent results.
# GraphModelHelper.SelectConnection fails from Python 2027 even for existing wires.
import c4d, maxon
NS = maxon.Id("com.redshift3d.redshift4c4d.class.nodespace")
RS = "com.redshift3d.redshift4c4d.nodes.core."
def fresh(extra=(("tex","texturesampler"),("mix","rscolormix"),("tex2","texturesampler"))):
    doc = c4d.documents.GetActiveDocument()
    mat = c4d.BaseMaterial(c4d.Mmaterial); mat.SetName("WT_Test"); doc.InsertMaterial(mat)
    g = mat.GetNodeMaterialReference().CreateDefaultGraph(NS)
    with g.BeginTransaction() as t:
        for k, a in extra: g.AddChild(maxon.Id(k), maxon.Id(RS + a))
        t.Commit()
    doc.SetActiveMaterial(mat)
    return g
def graph():
    return c4d.documents.GetActiveDocument().GetActiveMaterial().GetNodeMaterialReference().GetGraph(NS)
def nodes(g):
    return {str(n.GetId()).split('@')[0]: n for n in g.GetViewRoot().GetChildren() if n.GetKind() == maxon.NODE_KIND.NODE}
def short(p):
    return str(p.GetPath()).split('>')[0].split('@')[0] + "." + str(p.GetId()).split('.')[-1]
def wires(g=None):
    g = g or graph(); r = []
    for name, n in nodes(g).items():
        for p in n.GetInputs().GetChildren():
            for c in p.GetConnections(maxon.PORT_DIR.INPUT):
                r.append(short(c[0]) + "->" + name + "." + str(p.GetId()).split('.')[-1])
    return sorted(r)
def select(*ks, wire=None, ports=()):
    g = graph(); N = nodes(g)
    with g.BeginTransaction() as t:
        maxon.GraphModelHelper.DeselectAll(g, maxon.NODE_KIND.ALL_MASK)
        for k in ks: maxon.GraphModelHelper.SelectNode(N[k])
        for node, side, pid in ports:
            lst = N[node].GetOutputs() if side == "out" else N[node].GetInputs()
            maxon.GraphModelHelper.SelectNode(lst.FindChild(RS + pid))
        if wire:
            (sn, sp), (tn, tp) = wire
            maxon.GraphModelHelper.SelectConnection(N[sn].GetOutputs().FindChild(RS + sp), N[tn].GetInputs().FindChild(RS + tp))
        t.Commit()
