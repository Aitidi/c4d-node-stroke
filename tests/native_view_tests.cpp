#include "native_view.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace nodestroke;
void check(bool v,const char* msg) { if (!v) { std::cerr<<msg<<'\n'; std::exit(1); } }
int main() {
 ViewRecord v{.784,323.666,370.096}; Point origin{41,165};
 auto a=graph_to_screen({0,0},origin,v), b=graph_to_screen({284.697,2.692},origin,v);
 check(a && b && std::abs((b->x-a->x)-223.202448)<.001,"observed node spacing");
 auto back=screen_to_graph(*b,origin,v);
 check(back && std::abs(back->x-284.697)<1e-9 && std::abs(back->y-2.692)<1e-9,"round trip");
 auto panned=graph_to_screen({0,0},origin,{.784,323.666,770.096});
 check(panned && std::abs(panned->y-a->y-400)<1e-9,"observed pan");
 check(!graph_to_screen({0,0},origin,{0,0,0}),"zero zoom");
 check(!screen_to_graph({0,0},origin,{std::numeric_limits<double>::infinity(),0,0}),"infinite zoom");
}
