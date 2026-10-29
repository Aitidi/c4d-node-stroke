import c4d, maxon, traceback
from pathlib import Path
out = []
try:
    doc = c4d.documents.GetActiveDocument()
    mat = doc.GetActiveMaterial()
    out.append('material: ' + str(mat))
    ns = c4d.GetActiveNodeSpaceId()
    out.append('nodespace: ' + str(ns))
    graph = mat.GetNodeMaterialReference().GetGraph(ns)
    for n in graph.GetViewRoot().GetChildren():
        if n.GetKind() != maxon.NODE_KIND.NODE: continue
        out.append('node: ' + str(n.GetPath()))
        try:
            wd = n.GetValue('widgetDataBlackBox')
            out.append('blackbox: ' + repr(wd))
            if wd:
                for k, v in wd:
                    out.append('  entry: ' + repr(k) + ' => ' + repr(v))
                    try:
                        for k2, v2 in v:
                            out.append('    nested: ' + repr(k2) + ' => ' + repr(v2))
                            try: out.append('      pair: ' + repr(v2.GetFirst()) + ', ' + repr(v2.GetSecond()))
                            except Exception as e: out.append('      pair error: ' + repr(e))
                    except Exception as e: out.append('    nested error: ' + repr(e))
        except Exception as e: out.append('blackbox error: ' + repr(e))
except Exception:
    out.append(traceback.format_exc())
Path(r'D:\AgentProject\c4d-node-stroke\experiments\node_layout_probe.txt').write_text('\n'.join(out), encoding='utf-8')
print('NodeStroke layout probe complete')
