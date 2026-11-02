import c4d
print('C4D_VERSION', c4d.GetC4DVersion())
missing = []
for i in list(range(1072550, 1072561)) + [1073003]:
    plugin = c4d.plugins.FindPlugin(i, c4d.PLUGINTYPE_COMMAND)
    print('WIRE_PLUGIN', i, plugin.GetName() if plugin else None)
    if plugin is None:
        missing.append(i)
if missing:
    raise RuntimeError('NodeStroke commands not loaded: ' + str(missing))
print('NODESTROKE_HOST_SMOKE_PASS')
