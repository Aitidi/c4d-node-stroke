import c4d,maxon,traceback
from pathlib import Path
out=[]
try:
 m=c4d.documents.GetActiveDocument().GetActiveMaterial(); g=m.GetNodeMaterialReference().GetGraph(c4d.GetActiveNodeSpaceId())
 wd=g.GetViewRoot().GetValue('widgetDataBlackBoxOut')
 for i,(k,v) in enumerate(wd):
  out.append('outer %s key str=%s repr=%r type=%s' % (i,str(k),k,type(k)))
  for j,(ik,iv) in enumerate(v):
   out.append(' inner %s key=%r type=%s val=%r type=%s' % (j,ik,type(ik),iv,type(iv)))
   try: out.append(' pair first=%r type=%s second=%r type=%s' % (iv.GetFirst(),type(iv.GetFirst()),iv.GetSecond(),type(iv.GetSecond())))
   except Exception as e: out.append(' pair error '+repr(e))
except Exception: out.append(traceback.format_exc())
Path(r'D:\AgentProject\c4d-node-stroke\experiments\view_keys_probe.txt').write_text('\n'.join(out),encoding='utf-8')
print('view keys probe complete')
