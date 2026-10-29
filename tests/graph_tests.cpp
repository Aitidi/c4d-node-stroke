#include "graph.h"
#include <cstdlib>
#include <iostream>
using namespace nodestroke;
void check(bool ok, const char* text) { if (!ok) { std::cerr << text << '\n'; std::exit(1); } }
int main() {
 Graph g{{{"a","A",{},{{"out","color"}}},{"b","B",{{"in","color"}},{{"out","color"}}},{"c","C",{{"in","color"}},{}},{"bad","D",{{"in","number"}}, {}}},{}};
 check(draw(g,{{"a",{}},{"b",{}},{"c",{}}},{},false) && g.links.size()==2,"chain");
 auto before=g.links.size();
 check(!draw(g,{{"a",{}},{"b",{}},{"bad",{}}},{},false) && g.links.size()==before,"atomic failure");
 disconnect(g,"b"); check(g.links.empty(),"disconnect");
 check(draw(g,{{"a",{}},{"b",{}},{"c",{}}},{},true) && g.links.size()==2 && g.links[1].source_node=="a","fan out");
 disconnect(g,"b");
 Link original=g.links.front();
 check(insert(g,original,"b",{}) && g.links.size()==2 && g.links[1].source_node=="b", "insert");
 check(!insert(g,original,"bad",{}) && g.links.size()==2,"missing wire");
 Graph x{{{"a","A",{},{{"o1","color"},{"o2","color"}}},{"c","C",{{"i1","color"},{"i2","color"}}, {}}},{}};
 check(draw(x,{{"a",{}},{"c",{}}},{{"A","C","o2","i2"}},false) && x.links[0].source_port=="o2" && x.links[0].target_port=="i2","rule");
 check(draw(x,{{"a","o1"},{"c","i2"}},{},false) && x.links.size()==1 && x.links[0].source_port=="o1","replace");
 DisplayWire visible{x.links[0],{{0,0},{10,0}}};
 check(cut(x,{visible},{{5,-1},{5,1}})==1 && x.links.empty(),"cut");
}

