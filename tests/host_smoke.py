import c4d
print('C4D_VERSION', c4d.GetC4DVersion())
for i in (1072551, 1072552, 1072553):
    plugin = c4d.plugins.FindPlugin(i, c4d.PLUGINTYPE_COMMAND)
    print('WIRE_PLUGIN', i, plugin.GetName() if plugin else None)

