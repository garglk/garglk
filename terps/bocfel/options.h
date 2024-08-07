// vim: set ft=cpp:

#ifndef ZTERP_CONFIG_H
#define ZTERP_CONFIG_H

#include <bitset>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "stack.h"

using Parser = std::function<void(const std::string &)>;

struct OptValue {
    enum class Type { Flag, Number, Value } type;
    std::string desc;
    Parser parser;
};

struct Options {
    Options();

    unsigned long eval_stack_size = DEFAULT_STACK_SIZE;
    unsigned long call_stack_size = DEFAULT_CALL_DEPTH;
    bool disable_color = false;
    bool disable_config = false;
    bool disable_timed = false;
    bool disable_sound = false;
    bool enable_escape = false;
    std::unique_ptr<std::string> escape_string = std::make_unique<std::string>("1m");
    bool disable_fixed = false;
    bool assume_fixed = false;
    bool disable_graphics_font = false;
    bool enable_alt_graphics = false;
    bool show_help = false;
    bool disable_history_playback = false;
    bool show_id = false;
    bool disable_term_keys = false;
    std::unique_ptr<std::string> username = nullptr;
    bool disable_meta_commands = false;
    unsigned long int_number = 1; // DEC
    unsigned char int_version = 'C';
    bool disable_patches = false;
    bool replay_on = false;
    std::unique_ptr<std::string> replay_name = nullptr;
    bool record_on = false;
    std::unique_ptr<std::string> record_name = nullptr;
    bool transcript_on = false;
    std::unique_ptr<std::string> transcript_name = nullptr;
    unsigned long undo_slots = 100;
    bool show_version = false;
    bool disable_abbreviations = false;
    bool enable_censorship = false;
    bool overwrite_transcript = false;
    bool override_undo = false;
    std::unique_ptr<unsigned long> random_seed = nullptr;
    std::unique_ptr<std::string> random_device = nullptr;

    bool autosave = false;
    bool persistent_transcript = false;
    std::unique_ptr<std::string> editor = nullptr;
    bool warn_on_v6 = true;
    bool redirect_v6_windows = true;
    bool disable_v6_hacks = false;
    double v6_hack_max_scale = 4.0;

    void read_config();
    void process_arguments(int argc, char **argv);
    void help();

    std::vector<std::string> &errors() {
        return m_errors;
    }

private:
    std::unordered_map<std::string, Parser> m_from_config;
    std::unordered_map<char, OptValue> m_opts;
    std::vector<std::string> m_errors;


    int getopt(int argc, char *const argv[]);
    void add_parser(char flag, std::string desc, bool use_config, std::string name, const Parser &parser, OptValue::Type type);
    std::vector<std::pair<char, OptValue>> sorted_opts();
};

// BadOption means an invalid option was provided (i.e. one that does
// not exist).
//
// BadValue means an invalid value was provided to an otherwise valid
// option (e.g. out of range).
//
// • BadValue by itself does not cause help to be printed: the user
//   provided a valid option, but not a valid value. The list of options
//   won’t be particularly useful.
// • BadOption and BadValue both cause the program to exit with a
//   failure status.
namespace ArgStatus {
enum {
    BadOption,
    BadValue,
};
}

extern Options options;
extern std::bitset<3> arg_status;

#endif
