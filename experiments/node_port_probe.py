import c4d, maxon, traceback
from pathlib import Path
out = []
try:
    doc=c4d.documents.GetActiveDocument(); mat=doc.GetActiveMaterial(); ns=c4d.GetActiveNodeSpaceId()
    graph=mat.GetNodeMaterialReference().GetGraph(ns)
    def walk(node, depth=0):
        if depth>4: return
        try:
            val=node.GetValue('widgetDataBlackBox')
            if val: out.append(str(node.GetPath())+' -> '+str(val))
        except Exception as e: out.append('error '+str(node.GetPath())+' '+repr(e))
        for child in node.GetChildren(): walk(child,depth+1)
    walk(graph.GetViewRoot())
except Exception: out.append(traceback.format_exc())
Path(r'D:\AgentProject\c4d-node-stroke\experiments\node_port_probe.txt').write_text('\n'.join(out),encoding='utf-8')
print('port probe complete')
