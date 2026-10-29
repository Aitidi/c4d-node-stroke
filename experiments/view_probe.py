import c4d, maxon, traceback
from pathlib import Path
out=[]
try:
 d=c4d.documents.GetActiveDocument(); m=d.GetActiveMaterial(); sp=c4d.GetActiveNodeSpaceId(); g=m.GetNodeMaterialReference().GetGraph(sp)
 out.append('space '+str(sp))
 attrs=('widgetDataBlackBox','widgetDataBlackBoxSM','widgetDataBlackBoxOut')
 def walk(n, depth=0):
  if depth>4: return
  for key in attrs:
   try:
    v=n.GetValue(key)
    if v: out.append(str(n.GetPath())+' '+key+' '+str(v))
   except Exception as e: out.append('error '+str(n.GetPath())+' '+key+' '+repr(e))
  for child in n.GetChildren(): walk(child,depth+1)
 walk(g.GetViewRoot())
 def container(c, path, depth=0):
  if depth>4: return
  for key, value in c:
   if isinstance(value,c4d.BaseContainer): container(value,path+'/'+str(key),depth+1)
   elif isinstance(value,(float,int,str,c4d.Vector)): out.append(path+'/'+str(key)+' = '+repr(value))
 container(m.GetDataInstance(),'material')
except Exception: out.append(traceback.format_exc())
Path(r'D:\AgentProject\c4d-node-stroke\experiments\view_probe_latest.txt').write_text('\n'.join(out),encoding='utf-8')
print('view probe complete')
