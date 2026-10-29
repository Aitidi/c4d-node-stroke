import c4d
print('C4D_VERSION', c4d.GetC4DVersion())
for i in (1069997, 1069998, 1069999):
    plugin = c4d.plugins.FindPlugin(i, c4d.PLUGINTYPE_COMMAND)
    print('WIRE_PLUGIN', i, plugin.GetName() if plugin else None)

