// vim: set ft=cpp:

#ifndef ZTERP_OSDEP_H
#define ZTERP_OSDEP_H

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "screen.h"
#include "types.h"

long zterp_os_filesize(std::FILE *fp);
std::unique_ptr<std::string> zterp_os_rcfile(bool create_parent);
std::unique_ptr<std::string> zterp_os_autosave_name();
std::unique_ptr<std::string> zterp_os_aux_file(const std::string &filename);
void zterp_os_edit_file(const std::string &filename);
std::vector<char> zterp_os_edit_notes(const std::vector<char> &notes);
void zterp_os_show_transcript(const std::vector<char> &transcript);

#ifndef ZTERP_GLK
std::pair<unsigned int, unsigned int> zterp_os_get_screen_size();
void zterp_os_init_term();
bool zterp_os_have_style(StyleBit style);
bool zterp_os_have_colors();
void zterp_os_set_style(const Style &style, const Color &fg, const Color &bg);
#endif

#endif
