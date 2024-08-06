// vim: set ft=cpp:

#ifndef ZTERP_STASH_H
#define ZTERP_STASH_H

#include <memory>

struct Stasher {
    virtual ~Stasher() = default;
    virtual void backup() = 0;
    virtual bool restore() = 0;
    virtual void free() = 0;
};

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

void stash_register(std::unique_ptr<Stasher> stasher);

#endif
