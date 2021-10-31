/*$Id: //depot/prj/geas/master/code/geasglk.cc#10 $
  geasglk.cc

  User interface bridge from Geas Core to Glk.

  Copyright (C) 2006 David Jones.  Distribution or modification in any
  form permitted.

  Some code is taken from the public domain
  http://www.eblong.com/zarf/glk/model.c written by Andrew Plotkin.

  By the way, I can't write C++.  Sorry about that.


  Glk Window arrangment.

    +---------+
    |    B    |
    +---------+
    |    M    |
    |         |
    +---------+
    |    I    |
    +---------+

  B is a one line "banner window", showing the game name and author.  Kept
  in the global variable, it's optional, null if unavailable.
  optional.
  M is the main window where the text of the game appears.  Kept in the
  global variable mainglkwin.
  I is a one line "input window" where the user inputs their commands.
  Kept in the global variable inputwin, it's optional, and if not separate
  is set to mainglkwin.

  Maybe in future revisions there will be a status window (including a
  compass rose).
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>

#include "GeasRunner.hh"

class GeasGlkInterface : public GeasInterface
{
protected:
    virtual std::string get_file (const std::string &) const;
    virtual GeasResult print_normal (const std::string &);
    virtual GeasResult print_newline ();

    virtual void set_foreground (const std::string &);
    virtual void set_background (const std::string &);
    virtual GeasResult set_style (const GeasFontStyle &);

    virtual std::string get_string ();
    virtual uint make_choice (const std::string &, std::vector<std::string>);

  virtual std::string absolute_name (const std::string &, const std::string &) const;
public:
    GeasGlkInterface() { ; }
};

static void glk_put_cstring(const char *);

extern "C" {

#include <assert.h>
#include "glk.h"

winid_t mainglkwin;
winid_t inputwin;
winid_t bannerwin;
strid_t inputwinstream;

extern const char *storyfilename;  /* defined in geasglkterm.c */
extern int use_inputwindow;

static int ignore_lines = 0;  /* count of lines to ignore in game output */

static std::string banner;
static void draw_banner();

void glk_main(void)
{
    char err_buf[1024];
    char cur_buf[1024];
    glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_ReverseColor, 1);
    /* Open the main window. */
    mainglkwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    if (!mainglkwin) {
        /* It's possible that the main window failed to open. There's
            nothing we can do without it, so exit. */
        return; 
    }
    glk_set_window(mainglkwin);

    if (!storyfilename) {
	sprintf(err_buf,"No game name or more than one game name given.\n"
			"Try -h for help.\n");
	glk_put_string(err_buf);
        return;
    }

    glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
    bannerwin = glk_window_open(mainglkwin,
                                winmethod_Above | winmethod_Fixed,
                                1, wintype_TextGrid, 0);

    if (use_inputwindow)
        inputwin = glk_window_open(mainglkwin,
                                   winmethod_Below | winmethod_Fixed,
                                   1, wintype_TextBuffer, 0);
    else
        inputwin = NULL;

    if (!inputwin)
        inputwin = mainglkwin;

    inputwinstream = glk_window_get_stream(inputwin);

    if (!glk_gestalt(gestalt_Timer, 0)) {
	sprintf(err_buf,"\nNote -- The underlying Glk library does not support"
                        " timers.  If this game tries to use timers, then some"
                        " functionality may not work correctly.\n\n");
	glk_put_string(err_buf);
    }

    GeasRunner *gr = GeasRunner::get_runner(new GeasGlkInterface());
    gr->set_game(storyfilename);
    banner = gr->get_banner();
    draw_banner();

    glk_request_timer_events(1000);

    char buf[200];

    while(gr->is_running()) {
        if (inputwin != mainglkwin)
            glk_window_clear(inputwin);
        else
            glk_put_cstring("\n");
        sprintf(cur_buf, "> ");
        glk_put_string_stream(inputwinstream, cur_buf);

        glk_request_line_event(inputwin, buf, (sizeof buf) - 1, 0);

        event_t ev;
        ev.type = evtype_None;

        while(ev.type != evtype_LineInput) {
            glk_select(&ev);

            switch(ev.type) {
            case evtype_LineInput:
                if(ev.win == inputwin) {
                    std::string cmd = std::string(buf, ev.val1);
                    if(inputwin == mainglkwin)
                        ignore_lines = 2;
                    gr->run_command(cmd);
                }
                break;

            case evtype_Timer:
                gr->tick_timers();
                break;

            case evtype_Arrange:
            case evtype_Redraw:
                draw_banner();
                break;
            }
        }
    }
}

} /* extern "C" */

void
draw_banner()
{
  glui32 width;
  int index;
  if (bannerwin)
    {
      glk_window_clear(bannerwin);
      glk_window_move_cursor(bannerwin, 0, 0);
      strid_t stream = glk_window_get_stream(bannerwin);

      glk_set_style_stream(stream, style_User1);
      glk_window_get_size (bannerwin, &width, NULL);
      for (index = 0; index < width; index++)
        glk_put_char_stream (stream, ' ');
      glk_window_move_cursor(bannerwin, 1, 0);

      if (banner.empty())
        glk_put_string_stream(stream, (char*)"Geas 0.4");
      else
        glk_put_string_stream(stream, (char*)banner.c_str());
    }
}

void
glk_put_cstring(const char *s)
{
    /* The cast to remove const is necessary because glk_put_string
     * receives a "char *" despite the fact that it could equally well use
     * "const char *". */
    glk_put_string((char *)s);
}

GeasResult
GeasGlkInterface::print_normal (const std::string &s)
{
    if(!ignore_lines)
      {
	glk_put_cstring(s.c_str());
      }
    return r_success;
}

GeasResult
GeasGlkInterface::print_newline ()
{
    if (!ignore_lines)
      {
	glk_put_cstring("\n");
      }
    else
      {
	ignore_lines--;
      }
    return r_success;
}


GeasResult
GeasGlkInterface::set_style (const GeasFontStyle &style)
{
    // Glk styles are defined before the window opens, so at this point we can only
    // pick the most suitable style, not define a new one.
    glui32 match;
    if (style.is_italic && style.is_bold)
      {
	match = style_Alert;
      }
    else if (style.is_italic)
      {
	match = style_Emphasized;
      }
    else if (style.is_bold)
      {
	match = style_Subheader;
      }
    else if (style.is_underlined)
      {
	match = style_User2;
      }
    else
      {
	match = style_Normal;
      }

    glk_set_style_stream(glk_window_get_stream(mainglkwin), match);
    return r_success;
}

void
GeasGlkInterface::set_foreground (const std::string &s)
{ 
    if (s != "") 
    {
    }
}

void
GeasGlkInterface::set_background (const std::string &s)
{ 
    if (s != "") 
    {
    }
}


/* Code lifted from GeasWindow.  Should be common.  Maybe in
 * GeasInterface?
 */
std::string
GeasGlkInterface::get_file (const std::string &fname) const
{
  std::ifstream ifs;
  ifs.open(fname.c_str(), std::ios::in | std::ios::binary);
  if (! ifs.is_open())
    {
      glk_put_cstring("Couldn't open ");
      glk_put_cstring(fname.c_str());
      glk_put_char(0x0a);
      return "";
    }
  std::string rv;
  char ch;
  ifs.get(ch);
  while (!ifs.eof())
    { 
      rv += ch;
      ifs.get(ch);
    } 
  return rv;
}

std::string
GeasGlkInterface::get_string ()
{
  char buf[200];
  glk_request_line_event(inputwin, buf, (sizeof buf) - 1, 0);
  while(1) {
    event_t ev;

    glk_select(&ev);

    if (ev.type == evtype_LineInput && ev.win == inputwin) {
      return std::string(buf, ev.val1);
    }
    /* All other events, including timer, are deliberately
     * ignored.
     */
  }
}

uint
GeasGlkInterface::make_choice (const std::string &label, std::vector<std::string> v)
{
    size_t n;

    glk_window_clear(inputwin);

    glk_put_cstring(label.c_str());
    glk_put_char(0x0a);
    n = v.size();
    for(size_t i=0; i<n; ++i)
      {
	std::stringstream t;
	std::string s;
	t << i+1;
	t >> s;
	glk_put_cstring(s.c_str());
	glk_put_cstring(": ");
	glk_put_cstring(v[i].c_str());
	glk_put_cstring("\n");
      }

    std::stringstream t;
    std::string s;
    std::string s1;
    t << n;
    t >> s;
    s1 = "Choose [1-" + s + "]> ";
    glk_put_string_stream(inputwinstream, (char *)(s1.c_str()));

    int choice = atoi(get_string().c_str());
    if(choice < 1)
      {
	choice = 1;
      }
    if((size_t)choice > n)
      {
	choice = (int)n;
      }

    std::stringstream u;
    u << choice;
    u >> s;
    s1 = "Chosen: " +  s + "\n";
    glk_put_cstring(s1.c_str());

    return choice - 1;
}

std::string GeasGlkInterface::absolute_name (const std::string &rel_name, const std::string &parent) const {
  std::cerr << "absolute_name ('" << rel_name << "', '" << parent << "')\n";
  if (parent[0] != '/')
    {
      return rel_name;
    }

  if (rel_name[0] == '/')
    {
      std::cerr << "  --> " << rel_name << "\n";
      return rel_name;
    }
  std::vector<std::string> path;
  uint dir_start = 1, dir_end;
  while (dir_start < parent.length())
    {
      dir_end = dir_start;
      while (dir_end < parent.length() && parent[dir_end] != '/')
	{
	  dir_end ++;
	}
      path.push_back (parent.substr (dir_start, dir_end - dir_start));
      dir_start = dir_end + 1;
    }
  path.pop_back();
  dir_start = 0;
  std::string tmp;
  while (dir_start < rel_name.length())
    {
      dir_end = dir_start;
      while (dir_end < rel_name.length() && rel_name[dir_end] != '/')
	{
	  dir_end ++;
	}
      tmp = rel_name.substr (dir_start, dir_end - dir_start);
      dir_start = dir_end + 1;
      if (tmp == ".")
	{
	  continue;
	}
      else if (tmp == "..")
	{
	  path.pop_back();
	}
      else
	{
	  path.push_back (tmp);
	}
    }
  std::string rv;
  for (const auto &i: path)
    {
      rv = rv + "/" + i;
    }
  std::cerr << " ---> " << rv << "\n";
  return rv;
}




#if 0
void GeasGlkInterface::debug_print (std::string s) { ; }

GeasResult GeasGlkInterface::pause (int msec) { return r_success; }
#endif
