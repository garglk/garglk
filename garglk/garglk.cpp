#include <cstdlib>
#include <string>

#include "garglk.h"

std::string gli_program_name = "Unknown";
std::string gli_program_info;
std::string gli_story_name;
std::string gli_story_title;

bool gli_exiting = false;

void garglk_set_program_name(const char *name)
{
    gli_program_name = name;
    wintitle();
}

void garglk_set_program_info(const char *info)
{
    gli_program_info = info;
}

void garglk_set_story_name(const char *name)
{
    gli_story_name = name;
    wintitle();
}

void garglk_set_story_title(const char *title)
{
    gli_story_title = title;
    wintitle();
}

// All normal program termination should go through here instead of
// directly calling std::exit() to ensure that gli_exiting is properly
// set. Some code in destructors needs to be careful what it's doing
// during program exit, and uses this flag to check that.
void gli_exit(int status)
{
    gli_exiting = true;
    std::exit(status);
}
