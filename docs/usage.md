# NodeStroke User Guide

English | [简体中文](usage.zh-CN.md)

NodeStroke lets you wire Redshift node materials in the Cinema 4D node editor by drawing strokes while you hold a key. This guide covers installation, every gesture, the connection rules, the commands and the settings.

**Requirements**: Windows, Cinema 4D 2027.0, Redshift node materials.

## Contents

1. [Installation](#1-installation)
2. [Quick start](#2-quick-start)
3. [Connecting: hold I](#3-connecting-hold-i)
4. [Cutting: hold Y](#4-cutting-hold-y)
5. [Shaking a node out of its wires](#5-shaking-a-node-out-of-its-wires)
6. [Dropping a node onto a wire](#6-dropping-a-node-onto-a-wire)
7. [Automatic Bump Map and Displacement nodes](#7-automatic-bump-map-and-displacement-nodes)
8. [How ports are chosen](#8-how-ports-are-chosen)
9. [Connection rules](#9-connection-rules)
10. [Commands and shortcuts](#10-commands-and-shortcuts)
11. [Settings](#11-settings)
12. [Troubleshooting](#12-troubleshooting)

## 1. Installation

1. Quit Cinema 4D.
2. Unzip `NodeStroke.zip`. It contains a `NodeStroke` folder with `nodestroke.xdl64`, a `res` folder and `README.txt`.
3. Copy the whole `NodeStroke` folder into a Cinema 4D plugins folder. Either:
   - your user plugins folder: `%APPDATA%\Maxon\Maxon Cinema 4D 2027_<id>\plugins\` (Edit > Preferences, then "Open Preferences Folder" at the bottom left shows where it is), or
   - the program's plugins folder: `C:\Program Files\Maxon Cinema 4D 2027\plugins\` (needs administrator rights).
4. Start Cinema 4D. **Edit > Preferences** now has a **NodeStroke** page.

Install NodeStroke in one place only. Two copies would both react to every gesture.

**Updating**: quit Cinema 4D and replace the `NodeStroke` folder.
**Uninstalling**: quit Cinema 4D and delete the `NodeStroke` folder. Your connection rules stay in the preferences folder (`nodestroke_rules.txt`); delete that file too if you no longer need it.

## 2. Quick start

1. Open a Redshift node material in the node editor.
2. Put the mouse over the canvas, **hold I**, and move the mouse from one node across another. Release I: the two nodes are connected.
3. **Hold Y** and move the mouse across a wire. Release Y: the wire is cut.

No mouse button is pressed for I and Y: you only hold the key and move the mouse. The stroke is drawn on the canvas while you move and applied when you release the key. Every change is a single undo step, so **Ctrl+Z** takes it back.

## 3. Connecting: hold I

Hold **I**, move across the nodes you want to connect, then release **I**.

- **Two nodes**: the first node you pass is the source, the second is the target.
- **More nodes**: they are connected in the order you pass them, as a chain: first → second → third, and so on.
- **Exact ports**: start the stroke on an output port row of the first node, and end it on an input port row of the last node. Those exact ports are used. Starting or ending anywhere else on a node (its title or thumbnail) lets NodeStroke choose the ports (see [How ports are chosen](#8-how-ports-are-chosen)).
- **Occupied input**: if the chosen input already has a wire, the new wire replaces it.
- **When there is no obvious input**: for a stroke between two nodes, a menu of the target's inputs appears; pick one, or close the menu to cancel.
- **UV Context Projection**: a stroke that starts on a UV Context Projection node connects it to the UV Context input of every node you pass, for example all your textures in one stroke.
- **Node groups**: strokes work inside groups, and also to and from the group's interface ports in the side panels of the group view, so you can connect a group input or output to a node inside the group.

The status bar reports the result, for example "NodeStroke: connected 3 nodes."

## 4. Cutting: hold Y

Hold **Y**, move across the wires you want to remove, then release **Y**. Every wire your stroke crosses is cut, in one undo step. If the stroke crossed no wire, the status bar says so and nothing changes. Wires to a group's interface ports can be cut as well.

## 5. Shaking a node out of its wires

Drag a node with the left mouse button as usual, and shake it quickly left and right (about four changes of direction). NodeStroke takes the node out of all its wires and connects what fed it directly to what it fed, where the types match.

If several nodes are selected and you drag one of them, all selected nodes are taken out.

## 6. Dropping a node onto a wire

Drag a node that has no wires onto an existing wire, and rest there for a moment (about a third of a second) before releasing the mouse. The node is inserted into the wire: the wire's source connects to the node's input, and the node's output connects to the wire's target, using compatible ports.

Only nodes without wires are inserted, so moving a connected node over other wires never rewires it.

## 7. Automatic Bump Map and Displacement nodes

Some inputs need a helper node in Redshift. NodeStroke adds it for you:

| You connect into | NodeStroke adds |
| --- | --- |
| The Bump input of a Standard Material, or the Geometry Normal input of an OpenPBR material | a **Bump Map** node between source and material |
| The Displacement input of the Output node | a **Displacement** node between source and Output |

The helper is skipped if one is already upstream of the source.

The helper is placed one node slot to the right of the source node, level with it, and moved up or down a little to avoid covering other nodes. Connecting and adding the helper are one undo step.

## 8. How ports are chosen

Strokes and the connect commands choose ports in a different order.

### Strokes (hold I)

1. **Exact ports** under the start and end of the stroke.
2. **Your connection rules** for this pair of node types (see below).
3. The **first free input** of the target, from top to bottom as the node shows it, that the source can connect to.
4. A **menu** of the target's inputs (two-node strokes only), or an automatic choice for longer chains.

So a stroke that ends on a node's title or thumbnail goes to the first free input the node shows. To connect a specific input, end the stroke on that input's row.

### Commands (Chain Connect, Fan Out, Fan In)

1. **Your connection rules** for this pair of node types.
2. **Automatic choice**: a free input of the same type family (such as col3/col4), then any free input the source can connect to, then replacing an occupied input (Fan In never replaces).

## 9. Connection rules

A rule says: "when connecting a node of type A to a node of type B, use this output and this input." Rules come before automatic choices.

**Edit > Preferences > NodeStroke > Edit Rules...** opens the rules window:

- Each line shows one rule as *Source node / output port → Target node / input port*, with the names Cinema 4D shows in your interface language.
- **Add From Selected Ports**: in the node editor, select one output port and one input port, then click this button. The two ports are connected and remembered as a rule for these two node types.
- **Delete**: select one or more rules and delete them.
- **Reload**: re-reads the rules file after you edited it by hand.
- **Open Rules File...**: opens `nodestroke_rules.txt` in your text editor. Each line holds `<source node asset id> <output port id> <target node asset id> <input port id>`; lines starting with `#` are comments.

## 10. Commands and shortcuts

The commands are not in the Extensions menu. Find them by searching "NodeStroke" in the **Commander** (Shift+C), or assign shortcuts in **Window > Customization > Customize Commands**.

Before running a command, select the node material in the Material Manager and the elements in the node graph.

| Command | What it does |
| --- | --- |
| Connect Selected Ports | Connects one selected output port to one selected input port |
| Disconnect Selected Nodes | Removes every wire of the selected nodes |
| Cut Selected Wires | Removes the selected wires |
| Extract Selected Nodes | Takes the selected nodes out of their wires (like shaking) |
| Chain Connect Selected Nodes | Connects the selected nodes from left to right |
| Fan Out From Leftmost Node | Connects the leftmost selected node to every other selected node |
| Fan In To Rightmost Node | Connects every other selected node into free inputs of the rightmost one |
| Insert Node Into Selected Wire | Inserts the selected node into the selected wire (or select the two nodes the wire joins plus the node to insert) |
| Add Rule From Selected Ports | Connects the selected port pair and saves it as a rule |
| Edit Connection Rules | Opens the rules window |
| Toggle Canvas Gestures | Turns all gestures on or off |

## 11. Settings

**Edit > Preferences > NodeStroke**:

| Setting | Default | Effect |
| --- | --- | --- |
| Enable Gestures | on | Master switch for all gestures below |
| Hold I to Draw Connections | on | Section 3 |
| Hold Y to Cut Wires | on | Section 4 |
| Shake Node to Disconnect | on | Section 5 |
| Drop Node onto Wire to Insert | on | Section 6 |
| Edit Rules... | | Opens the rules window (section 9) |
| Write Diagnostic Log | off | Writes the reason a gesture did nothing to `nodestroke_debug.txt` in the preferences folder |

If I or Y conflict with your own shortcuts, turn that gesture off here. The interface follows the Cinema 4D language (English or Simplified Chinese).

## 12. Troubleshooting

**A stroke does nothing.** Check the status bar: NodeStroke explains most cases there, for example when the stroke touched only one node. Make sure the mouse is over the node editor canvas, and that the editor shows the material that is active in the Material Manager.

**"Cannot identify this node editor."** Select, in the Material Manager, the material the editor shows, and try again. On a Cinema 4D build other than 2027.0, exact positions are not available: scroll the canvas once with the mouse wheel, then draw.

**The wrong ports were connected.** End the stroke exactly on the input port row you want, or add a connection rule for that pair of node types.

**Something still does not work.** Turn on **Write Diagnostic Log**, repeat the gesture, and send `nodestroke_debug.txt` from the preferences folder along with your report. Turn the log off again afterwards.
