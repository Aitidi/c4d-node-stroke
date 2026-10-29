import c4d,maxon,traceback
from pathlib import Path
out=[]
try:
 m=c4d.documents.GetActiveDocument().GetActiveMaterial(); g=m.GetNodeMaterialReference().GetGraph(c4d.GetActiveNodeSpaceId()); d=g.GetViewRoot().GetValue('widgetDataBlackBoxOut')
 for i,(k,v) in enumerate(d):
  for expr in ('str(k)','repr(k)','hash(k)','k.GetHashCode()','k.ToString()','k.IsEmpty()'):
   try: out.append(f'{i} {expr}: {eval(expr)!r}')
   except Exception as e: out.append(f'{i} {expr}: {type(e).__name__}: {e}')
  out.append('value: '+str(v))
except Exception: out.append(traceback.format_exc())
Path(r'D:\AgentProject\c4d-node-stroke\experiments\view_key_hash_probe.txt').write_text('\n'.join(out),encoding='utf-8')
print('key hash probe complete')
