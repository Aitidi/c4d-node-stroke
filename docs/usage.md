# NodeStroke User Guide

English | [简体中文](usage.zh-CN.md)

NodeStroke lets you wire Redshift node materials in the Cinema 4D node editor by drawing strokes while you hold a key.

**Requirements**: Windows, Redshift node materials, and Cinema 4D 2027+ or 2026+. On 2026, see [Cinema 4D 2026](#12-cinema-4d-2026) for what differs.

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
10. [Menu and commands](#10-menu-and-commands)
11. [Settings](#11-settings)
12. [Cinema 4D 2026](#12-cinema-4d-2026)
13. [Troubleshooting](#13-troubleshooting)

## 1. Installation

1. Quit Cinema 4D.
2. Unzip the download for your version: `NodeStroke-C4D2027.zip` for 2027, `NodeStroke-C4D2026.zip` for 2026. It holds a `NodeStroke` folder with `nodestroke.xdl64`, a `res` folder and `README.txt`.
3. Copy that whole folder into your user plugins folder, `%APPDATA%\Maxon\Maxon Cinema 4D <version>_<id>\plugins\`. To find it, open Edit > Preferences and click "Open Preferences Folder" at the bottom left. The program's own `plugins` folder works too, but needs administrator rights.
4. Start Cinema 4D. **Extensions > NodeStroke** and **Edit > Preferences > NodeStroke** now appear.

Install only one copy: two copies would both react to every gesture.

**Updating**: quit Cinema 4D and replace the folder. **Uninstalling**: quit Cinema 4D and delete it. Your connection rules stay in `nodestroke_rules.txt` in the preferences folder; delete it too if you no longer need it.

## 2. Quick start

1. Open a Redshift node material in the node editor.
2. Put the mouse over the canvas, **hold I**, and move from one node across another. Release I: the nodes are connected.
3. **Hold Y** and move across a wire. Release Y: the wire is cut.

You don't press a mouse button for I and Y. The stroke shows while you move and applies when you release the key. Each change is one undo step.

## 3. Connecting: hold I

Hold **I**, move across the nodes you want to connect, then release it.

- **Order**: nodes connect in the order you pass them: first → second → third.
- **Exact ports**: start on an output port row of the first node and end on an input port row of the last node to use those ports. Starting or ending on a title or thumbnail lets NodeStroke choose (see [How ports are chosen](#8-how-ports-are-chosen)).
- **Occupied input**: the new wire replaces the old one.
- **No obvious input**: between two nodes, a menu of the target's inputs appears; pick one, or close it to cancel.
- **UV Context Projection**: a stroke that starts on this node connects it to the UV Context input of every node you pass.
- **Node groups**: strokes work inside groups and with the group's interface ports in the side panels of the group view.

The status bar reports the result, for example "NodeStroke: connected 3 nodes."

## 4. Cutting: hold Y

Hold **Y**, move across the wires you want to remove, then release it. Every wire the stroke crosses is cut, including wires to a group's interface ports. If it crossed none, the status bar says so.

## 5. Shaking a node out of its wires

Drag a node and shake it quickly left and right (about four changes of direction). NodeStroke takes it out of its wires and connects what fed it to what it fed, where the types match. If several nodes are selected, all of them are taken out.

## 6. Dropping a node onto a wire

Drag a node that has no wires onto a wire and rest for about a third of a second before releasing. The node is inserted into the wire using compatible ports. Connected nodes are never inserted, so moving them over wires is safe.

## 7. Automatic Bump Map and Displacement nodes

| You connect into | NodeStroke adds |
| --- | --- |
| The Bump input of a Standard Material, or the Geometry Normal input of an OpenPBR material | a **Bump Map** node |
| The Displacement input of the Output node | a **Displacement** node |

The helper goes between the source and the target, one node slot right of the source, and is skipped if one is already upstream. Connecting and adding it are one undo step.

## 8. How ports are chosen

**Strokes (hold I):**

1. The exact ports under the start and end of the stroke.
2. Your [connection rules](#9-connection-rules) for this pair of node types.
3. The first free input of the target, top to bottom, that the source can connect to.
4. A menu of the target's inputs (two-node strokes), or an automatic choice for longer chains.

To reach a specific input, end the stroke on its row.

**Commands (Chain Connect, Fan Out, Fan In):** your connection rules first, then a free input of the same type family (such as col3/col4), then any free connectable input, then replacing an occupied input (Fan In never replaces).

## 9. Connection rules

A rule says: "when connecting node type A to node type B, use this output and this input." Rules come before automatic choices.

**Edit > Preferences > NodeStroke > Edit Rules...** opens the rules window. Each line shows *Source node / output port → Target node / input port*.

- **Add From Selected Ports**: select one output and one input port in the node editor, then click. The ports are connected and saved as a rule.
- **Delete**: removes the selected rules.
- **Reload**: re-reads the file after you edited it by hand.
- **Open Rules File...**: opens `nodestroke_rules.txt`. Each line is `<source asset id> <output port id> <target asset id> <input port id>`; lines starting with `#` are comments.

## 10. Menu and commands

**Extensions > NodeStroke** has two commands:

- **NodeStroke** turns all gestures on or off. It shows a check mark while they are on.
- **NodeStroke Settings...** opens the settings page.

Plugin managers such as Boghma that list NodeStroke show the same commands and icon.

The commands below are in the **Commander** (Shift+C, search "NodeStroke"); give them shortcuts in **Window > Customization > Customize Commands**. Select the node material in the Material Manager and the elements in the node graph first.

| Command | What it does |
| --- | --- |
| Connect Selected Ports | Connects one selected output port to one selected input port |
| Disconnect Selected Nodes | Removes every wire of the selected nodes |
| Cut Selected Wires | Removes the selected wires |
| Extract Selected Nodes | Takes the selected nodes out of their wires (like shaking) |
| Chain Connect Selected Nodes | Connects the selected nodes from left to right |
| Fan Out From Leftmost Node | Connects the leftmost node to every other selected node |
| Fan In To Rightmost Node | Connects the other nodes into free inputs of the rightmost one |
| Insert Node Into Selected Wire | Inserts the selected node into the selected wire (or select the wire's two nodes plus the node to insert) |
| Add Rule From Selected Ports | Connects the selected port pair and saves it as a rule |
| Edit Connection Rules | Opens the rules window |

## 11. Settings

**Edit > Preferences > NodeStroke**:

| Setting | Default | Effect |
| --- | --- | --- |
| Enable Gestures | on | Master switch, same as Extensions > NodeStroke |
| Hold I to Draw Connections | on | Section 3 |
| Hold Y to Cut Wires | on | Section 4 |
| Shake Node to Disconnect | on | Section 5 |
| Drop Node onto Wire to Insert | on | Section 6 |
| Edit Rules... | | Section 9 |
| Write Diagnostic Log | off | Writes why a gesture did nothing to `nodestroke_debug.txt` in the preferences folder |

Turn a gesture off here if its key clashes with your shortcuts. The interface follows the Cinema 4D language (English or Simplified Chinese).

## 12. Cinema 4D 2026

- **Scroll first**: before the first stroke, scroll the canvas once with the mouse wheel or middle-drag it. If holding I or Y draws nothing later, for example after double-clicking into a group, scroll again.
- **2026.4.0**: everything else works as on 2027, including exact ports and group interface ports.
- **Before 2026.4.0**: positions are estimated, and wires to a group's interface ports cannot be connected or cut.

Details: [Cinema 4D 2026 compatibility](compatibility-2026.md).

## 13. Troubleshooting

**A stroke does nothing.** Check the status bar first. Make sure the mouse is over the node editor canvas and the editor shows the material that is active in the Material Manager. On 2026, scroll the canvas once.

**"Cannot identify this node editor."** Select the editor's material in the Material Manager and try again. On 2026, scroll the canvas once, then draw.

**The wrong ports were connected.** End the stroke on the input row you want, or add a connection rule.

**Something still does not work.** Turn on **Write Diagnostic Log**, repeat the gesture, and send `nodestroke_debug.txt` from the preferences folder with your report. Turn the log off afterwards.
