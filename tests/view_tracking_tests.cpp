#include "view_tracking.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace nodestroke;
void check(bool value,const char* msg) { if (!value) {std::cerr << msg << '\n'; std::exit(1);} }
int main() {
 const std::vector<ViewRecord> before{{1.114,291.637,273.169},{.784,323.666,370.096},{1,201.151,412}};
 const std::vector<ViewRecord> after{{1,201.151,412},{.784,323.666,770.096},{1.114,291.637,273.169}};
 auto hit=identify_view_change(before,after);
 check(hit && hit->before.offset_y==370.096 && hit->after.offset_y==770.096,"identify changed view despite reordering");
 check(!identify_view_change(before,before),"unchanged has no result");
 check(!identify_view_change({{1,0,0},{1,0,0}},{{1,0,0},{1,10,0}}),"duplicate records are ambiguous");
 check(!identify_view_change({{1,0,0},{1,10,0}},{{1,20,0},{1,30,0}}),"multiple changes are ambiguous");
 check(!identify_view_change({{1,0,0}},{{std::numeric_limits<double>::quiet_NaN(),0,0}}),"invalid values rejected");
 check(!identify_view_change({{1,0,0}},{{1,0,0},{1,5,0}}),"changed count rejected");
}
