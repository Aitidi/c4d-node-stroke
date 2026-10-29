"""Match observed draw labels to graph ports; fail closed on ambiguity.

This pure helper deliberately accepts already-scoped graph ports; it does not
choose a graph or editor. The caller must prove both belong to the same view.
"""
from collections import Counter


def match_labels(circles, ports):
    """Return (x, y, port ID) by unique exact name, or raise ValueError.

    circles: iterable of (x, y, visible label)
    ports: iterable of (graph port ID, effective name)
    """
    circles, ports = list(circles), list(ports)
    if not circles or len(circles) != len(ports):
        raise ValueError("visible circle and graph port counts differ")
    visible = [name for _, _, name in circles]
    effective = [name for _, name in ports]
    ids = [identifier for identifier, _ in ports]
    if any(not isinstance(name, str) or not name for name in visible + effective):
        raise ValueError("empty or invalid label")
    if len(set(ids)) != len(ids) or any(count != 1 for count in Counter(effective).values()):
        raise ValueError("duplicate graph port name or ID")
    if Counter(visible) != Counter(effective):
        raise ValueError("drawn labels do not uniquely match graph ports")
    by_name = {name: identifier for identifier, name in ports}
    return [(x, y, by_name[name]) for x, y, name in circles]
