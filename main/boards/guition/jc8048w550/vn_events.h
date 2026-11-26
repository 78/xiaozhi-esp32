#ifndef JC8048W550_VN_EVENTS_H
#define JC8048W550_VN_EVENTS_H

#include <string>
#include <vector>

namespace dashboard {

// Holidays and giỗ (shared Vietnamese observances) on a solar date, most
// specific first; empty on an ordinary day. Lunar observances never fire in a
// leap month. Needs no clock or network.
std::vector<std::string> EventsFor(int day, int month, int year);

}  // namespace dashboard

#endif  // JC8048W550_VN_EVENTS_H
