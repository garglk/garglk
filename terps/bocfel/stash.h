// vim: set ft=cpp:

#ifndef ZTERP_STASH_H
#define ZTERP_STASH_H

#include <functional>

void stash_register(std::function<void()> backup, std::function<bool()> restore, std::function<void()> free);

class Stash {
public:
    Stash() = default;
    Stash(const Stash &) = delete;
    Stash &operator=(const Stash &) = delete;
    ~Stash();

    void backup();
    bool exists() const;
    bool restore();

private:
    void free();
    bool m_have_stash = false;
};

#endif
