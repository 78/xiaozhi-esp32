#ifndef HILILY_HOME_CLIENT_H
#define HILILY_HOME_CLIENT_H
#include <string>
class HiLilyDisplay;
void StartHiLilyHomeClient(HiLilyDisplay* display, const char* url);
void QueueHiLilyMorningEvent(const std::string& action, const std::string& day,
                             int completed, const char* reason);
#endif
