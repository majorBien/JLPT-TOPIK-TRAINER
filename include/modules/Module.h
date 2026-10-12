// Base class for main-menu modules. Adding a feature = subclass + one line in App.
#pragma once

#include <string>

#include "AppContext.h"

class Module {
public:
    virtual ~Module() = default;
    virtual std::string japaneseTitle() const = 0;  // short, e.g. "文字・語彙"
    virtual std::string title() const = 0;          // English menu label
    virtual std::string badge(const AppContext&) const { return {}; }  // e.g. "12 due"
    virtual void run(AppContext& ctx) = 0;
};
