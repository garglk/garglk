#include <string>

#include "garglk.h"

std::string gli_program_name = "Unknown";
std::string gli_program_info;
std::string gli_story_name;
std::string gli_story_title;

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
