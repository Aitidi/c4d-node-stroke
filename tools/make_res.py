"""Generates c4d_plugin/nodestroke/res: the preferences page description and its strings.

Cinema 4D resource files are ASCII with CRLF line ends; non-ASCII text is written as
\\uXXXX escapes, as Cinema 4D's own string tables do. Run from the repository root:
    python tools/make_res.py
"""
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'c4d_plugin', 'nodestroke', 'res')


def write(rel, text):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = text.replace('\r\n', '\n').replace('\n', '\r\n')
    data.encode('ascii')  # resources must stay ASCII
    with open(path, 'w', encoding='ascii', newline='') as f:
        f.write(data)


def esc(s):
    return ''.join(c if ord(c) < 128 else chr(92) + 'u%04x' % ord(c) for c in s)


# Plain strings (GeLoadString): the connection rules dialog, command names, errors and
# status messages.
DIALOG_STRINGS = {
    'IDS_RULES_TITLE': ('NodeStroke Connection Rules', 'NodeStroke 连接规则'),
    'IDS_RULES_HINT': ('Which ports to use when connecting two node types. Rules are tried before automatic port choice.',
                       '两种节点相连时使用哪对端口。规则优先于自动选择端口。'),
    'IDS_RULES_EMPTY': ('No rules yet. Select an output port and an input port in the node editor, then click Add.',
                        '还没有规则。在节点编辑器中选中一个输出端口和一个输入端口，然后点击“添加”。'),
    'IDS_RULES_ADD': ('Add From Selected Ports', '从选中端口添加'),
    'IDS_RULES_DELETE': ('Delete', '删除'),
    'IDS_RULES_RELOAD': ('Reload', '重新载入'),
    'IDS_RULES_OPEN': ('Open Rules File...', '打开规则文件...'),
    'IDS_RULES_ADDED': ('Rule added and ports connected.', '已添加规则并连接端口。'),

    # Command names and help texts (Commander, Customize Commands).
    'IDS_CMD_DISCONNECT': ('NodeStroke: Disconnect Selected Nodes', 'NodeStroke：断开所选节点'),
    'IDS_CMD_DISCONNECT_HELP': ('Disconnect all wires on selected nodes in the active material graph.',
                                '断开当前材质节点图中所选节点的全部连线。'),
    'IDS_CMD_CUT': ('NodeStroke: Cut Selected Wires', 'NodeStroke：删除所选连线'),
    'IDS_CMD_CUT_HELP': ('Remove selected wires in the active material graph.', '删除当前材质节点图中所选的连线。'),
    'IDS_CMD_EXTRACT': ('NodeStroke: Extract Selected Nodes', 'NodeStroke：抽出所选节点'),
    'IDS_CMD_EXTRACT_HELP': ('Take selected nodes out of their wires and reconnect upstream to downstream.',
                             '把所选节点从连线中取出，并把上游直接接到下游。'),
    'IDS_CMD_CONNECT': ('NodeStroke: Connect Selected Ports', 'NodeStroke：连接所选端口'),
    'IDS_CMD_CONNECT_HELP': ('Connect one selected output port to one selected input port.',
                             '把选中的一个输出端口连接到选中的一个输入端口。'),
    'IDS_CMD_RULES': ('NodeStroke: Edit Connection Rules', 'NodeStroke：编辑连接规则'),
    'IDS_CMD_RULES_HELP': ('List, add and delete the connection rules used when NodeStroke picks ports.',
                           '查看、添加和删除 NodeStroke 选择端口时使用的连接规则。'),
    'IDS_CMD_CHAIN': ('NodeStroke: Chain Connect Selected Nodes', 'NodeStroke：依次连接所选节点'),
    'IDS_CMD_CHAIN_HELP': ('Connect selected nodes left to right using compatible ports or custom rules.',
                           '按从左到右的顺序连接所选节点，使用兼容端口或自定义规则。'),
    'IDS_CMD_FAN_OUT': ('NodeStroke: Fan Out From Leftmost Node', 'NodeStroke：从最左节点分发'),
    'IDS_CMD_FAN_OUT_HELP': ('Connect the leftmost selected node to every other selected node.',
                             '把最左边的所选节点连接到其他每个所选节点。'),
    'IDS_CMD_FAN_IN': ('NodeStroke: Fan In To Rightmost Node', 'NodeStroke：汇入最右节点'),
    'IDS_CMD_FAN_IN_HELP': ('Connect every other selected node into free inputs of the rightmost selected node.',
                            '把其他所选节点连接到最右边所选节点的空闲输入。'),
    'IDS_CMD_INSERT': ('NodeStroke: Insert Node Into Selected Wire', 'NodeStroke：把节点插入所选连线'),
    'IDS_CMD_INSERT_HELP': ('Insert the selected node into the selected wire.', '把所选节点插入所选连线。'),
    'IDS_CMD_ADD_RULE': ('NodeStroke: Add Rule From Selected Ports', 'NodeStroke：从所选端口添加规则'),
    'IDS_CMD_ADD_RULE_HELP': ('Connect the selected output and input ports and remember them as a rule for these node types.',
                              '连接所选的输出和输入端口，并记为这两种节点之间的规则。'),
    'IDS_CMD_TOGGLE': ('NodeStroke: Toggle Canvas Gestures', 'NodeStroke：开关画布手势'),
    'IDS_CMD_TOGGLE_HELP': ('Hold I and move the mouse across nodes to connect them; hold Y and move across wires to cut them.',
                            '按住 I 并划过节点即可连接；按住 Y 并划过连线即可切断。'),

    # Errors of the selection commands.
    'IDS_ERR_NO_MATERIAL': ('Select a node material in the Material Manager first.', '请先在材质管理器中选中一个节点材质。'),
    'IDS_ERR_NO_GRAPH': ('The selected material has no graph in the active node space.', '所选材质在当前节点空间中没有节点图。'),
    'IDS_ERR_READ_ONLY': ('This graph is read-only.', '这个节点图是只读的。'),
    'IDS_ERR_SELECT_PORTS': ('Select exactly one output port and one input port.', '请只选中一个输出端口和一个输入端口。'),
    'IDS_ERR_NOT_CONNECTABLE': ('These ports cannot be connected.', '这两个端口不能相连。'),
    'IDS_ERR_SELECT_WIRES': ('Select one or more wires in the material graph.', '请在材质节点图中选中一条或多条连线。'),
    'IDS_ERR_SELECT_NODES': ('Select one or more nodes in the material graph.', '请在材质节点图中选中一个或多个节点。'),
    'IDS_ERR_SELECT_TWO_NODES': ('Select two or more nodes.', '请选中两个或更多节点。'),
    'IDS_ERR_NO_COMPATIBLE': ('No compatible ports between two of the selected nodes.', '所选节点中有两个节点之间没有兼容的端口。'),
    'IDS_ERR_INSERT_SELECTION': ('Select one wire (or the two nodes it joins) and the node to insert.',
                                 '请选中一条连线（或它连接的两个节点）以及要插入的节点。'),
    'IDS_ERR_INSERT_ONE': ('Select only one node to insert.', '只能选中一个要插入的节点。'),
    'IDS_ERR_INSERT_WITH_WIRE': ('Select the node to insert together with the wire.', '请同时选中要插入的节点和连线。'),
    'IDS_ERR_INSERT_NO_PORTS': ('The node has no ports compatible with this wire.', '这个节点没有与该连线兼容的端口。'),
    'IDS_ERR_RULES_CREATE': ('Cannot create the rules file.', '无法创建规则文件。'),
    'IDS_ERR_RULES_READ': ('Cannot read the rules file.', '无法读取规则文件。'),
    'IDS_ERR_RULES_WRITE': ('Cannot write the rules file.', '无法写入规则文件。'),
    'IDS_ERR_HELPER_GROUP': ('NodeStroke: could not add the helper node inside the group.', 'NodeStroke：无法在组内添加辅助节点。'),
    'IDS_ERR_HELPER_PORTS': ('NodeStroke: the helper node has unexpected ports.', 'NodeStroke：辅助节点的端口与预期不符。'),

    # Status bar messages of the canvas gestures; # is replaced by a count.
    'IDS_GESTURE_UNKNOWN_EDITOR': ('NodeStroke: cannot identify this node editor. Select the material it shows and try again.',
                                   'NodeStroke：无法识别这个节点编辑器。请选中它显示的材质后重试。'),
    'IDS_GESTURE_OTHER_GRAPH': ("NodeStroke: this editor does not show the active material's graph; select its material and draw again.",
                                'NodeStroke：这个编辑器显示的不是当前材质的节点图；请选中它的材质后重新绘制。'),
    'IDS_GESTURE_CHANGED': ('NodeStroke: the graph or editor changed while drawing; draw again.',
                            'NodeStroke：绘制过程中节点图或编辑器发生了变化，请重新绘制。'),
    'IDS_GESTURE_GROUP_DEPTH': ('NodeStroke: node group nesting is too deep.', 'NodeStroke：节点组嵌套层数过多。'),
    'IDS_GESTURE_INSERTED': ('NodeStroke: node inserted into the wire.', 'NodeStroke：已把节点插入连线。'),
    'IDS_GESTURE_EXTRACTED': ('NodeStroke: took # node(s) out of their wires.', 'NodeStroke：已把 # 个节点从连线中取出。'),
    'IDS_GESTURE_GROUP_STROKE': ('NodeStroke: draw from a group interface port to a compatible node port.',
                                 'NodeStroke：请从组接口端口画到一个兼容的节点端口。'),
    'IDS_GESTURE_NOT_CONNECTABLE': ('NodeStroke: these ports cannot be connected.', 'NodeStroke：这两个端口不能相连。'),
    'IDS_GESTURE_GROUP_CONNECTED': ('NodeStroke: group interface connected.', 'NodeStroke：已连接组接口。'),
    'IDS_GESTURE_TWO_NODES': ('NodeStroke: start the stroke on one node and end it on another.',
                              'NodeStroke：请从一个节点开始画，到另一个节点结束。'),
    'IDS_GESTURE_UV_ONLY': ('NodeStroke: UV Context Projection connects only to nodes with a UV Context input.',
                            'NodeStroke：UV Context Projection 只能连接到带 UV Context 输入的节点。'),
    'IDS_GESTURE_UV_CONNECTED': ('NodeStroke: UV context connected to # node(s).', 'NodeStroke：已把 UV 上下文连接到 # 个节点。'),
    'IDS_GESTURE_CONNECTED': ('NodeStroke: connected # nodes.', 'NodeStroke：已连接 # 个节点。'),
    'IDS_GESTURE_NO_WIRES': ('NodeStroke: the stroke crossed no wires.', 'NodeStroke：笔画没有穿过任何连线。'),
    'IDS_GESTURE_CUT': ('NodeStroke: cut # wires.', 'NodeStroke：已切断 # 条连线。'),
    'IDS_GESTURE_ON': ('NodeStroke: canvas gestures on (hold I to connect, Y to cut).',
                       'NodeStroke：画布手势已开启（按住 I 连接，按住 Y 切断）。'),
    'IDS_GESTURE_OFF': ('NodeStroke: canvas gestures off.', 'NodeStroke：画布手势已关闭。'),
}
write('c4d_symbols.h', '''#ifndef C4D_SYMBOLS_H__
#define C4D_SYMBOLS_H__

enum
{
	_DUMMY_ELEMENT_ = 10000,
''' + ''.join('\t%s,\n' % k for k in DIALOG_STRINGS) + '''};

#endif // C4D_SYMBOLS_H__
''')
write('description/prefsnodestroke.h', '''#ifndef PREFSNODESTROKE_H__
#define PREFSNODESTROKE_H__

enum
{
	NODESTROKE_PREFS_GROUP_MAIN			= 999,
	NODESTROKE_PREFS_GESTURES				= 1000,
	NODESTROKE_PREFS_CONNECT,
	NODESTROKE_PREFS_CUT,
	NODESTROKE_PREFS_SHAKE,
	NODESTROKE_PREFS_DROP,
	NODESTROKE_PREFS_EDIT_RULES			= 1100,
	NODESTROKE_PREFS_HEADER_GESTURES	= 1200,
	NODESTROKE_PREFS_HEADER_RULES,
	NODESTROKE_PREFS_HEADER_DIAGNOSTICS,
	NODESTROKE_PREFS_LOG							= 1300
};

#endif // PREFSNODESTROKE_H__
''')
write('description/prefsnodestroke.res', '''CONTAINER prefsnodestroke
{
	NAME prefsnodestroke;
	GROUP NODESTROKE_PREFS_GROUP_MAIN
	{
		DEFAULT 1;
		COLUMNS 1;

		SEPARATOR NODESTROKE_PREFS_HEADER_GESTURES { }
		BOOL NODESTROKE_PREFS_GESTURES { ANIM OFF; }
		BOOL NODESTROKE_PREFS_CONNECT { ANIM OFF; }
		BOOL NODESTROKE_PREFS_CUT { ANIM OFF; }
		BOOL NODESTROKE_PREFS_SHAKE { ANIM OFF; }
		BOOL NODESTROKE_PREFS_DROP { ANIM OFF; }

		SEPARATOR NODESTROKE_PREFS_HEADER_RULES { }
		BUTTON NODESTROKE_PREFS_EDIT_RULES { }

		SEPARATOR NODESTROKE_PREFS_HEADER_DIAGNOSTICS { }
		BOOL NODESTROKE_PREFS_LOG { ANIM OFF; }
	}
}
''')
STRINGS = {
    'en-US': {
        'prefsnodestroke': 'NodeStroke',
        'NODESTROKE_PREFS_HEADER_GESTURES': 'Node Editor Gestures',
        'NODESTROKE_PREFS_GESTURES': 'Enable Gestures',
        'NODESTROKE_PREFS_CONNECT': 'Hold I to Draw Connections',
        'NODESTROKE_PREFS_CUT': 'Hold Y to Cut Wires',
        'NODESTROKE_PREFS_SHAKE': 'Shake Node to Disconnect',
        'NODESTROKE_PREFS_DROP': 'Drop Node onto Wire to Insert',
        'NODESTROKE_PREFS_HEADER_RULES': 'Connection Rules',
        'NODESTROKE_PREFS_EDIT_RULES': 'Edit Rules...',
        'NODESTROKE_PREFS_HEADER_DIAGNOSTICS': 'Diagnostics',
        'NODESTROKE_PREFS_LOG': 'Write Diagnostic Log (nodestroke_debug.txt)',
    },
    'zh-CN': {
        'prefsnodestroke': 'NodeStroke',
        'NODESTROKE_PREFS_HEADER_GESTURES': '节点编辑器手势',
        'NODESTROKE_PREFS_GESTURES': '启用手势',
        'NODESTROKE_PREFS_CONNECT': '按住 I 绘制连接',
        'NODESTROKE_PREFS_CUT': '按住 Y 切断连线',
        'NODESTROKE_PREFS_SHAKE': '摇晃节点断开连线',
        'NODESTROKE_PREFS_DROP': '拖到连线上插入节点',
        'NODESTROKE_PREFS_HEADER_RULES': '连接规则',
        'NODESTROKE_PREFS_EDIT_RULES': '编辑规则...',
        'NODESTROKE_PREFS_HEADER_DIAGNOSTICS': '诊断',
        'NODESTROKE_PREFS_LOG': '写入诊断日志（nodestroke_debug.txt）',
    },
}
for lang, table in STRINGS.items():
    width = max(len(k) for k in table) + 2
    rows = ''.join('\t%s%s"%s";\n' % (k, ' ' * (width - len(k)), esc(v)) for k, v in table.items())
    write('strings_%s/description/prefsnodestroke.str' % lang, 'STRINGTABLE prefsnodestroke\n{\n' + rows + '}\n')
    index = 0 if lang == 'en-US' else 1
    width = max(len(k) for k in DIALOG_STRINGS) + 2
    rows = ''.join('\t%s%s"%s";\n' % (k, ' ' * (width - len(k)), esc(v[index])) for k, v in DIALOG_STRINGS.items())
    write('strings_%s/c4d_strings.str' % lang, 'STRINGTABLE\n{\n' + rows + '}\n')
print('resources written to', os.path.normpath(ROOT))
