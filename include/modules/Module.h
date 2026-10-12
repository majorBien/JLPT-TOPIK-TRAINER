// Base class for main-menu modules. Adding a feature = subclass + one line in App.
#pragma once

#include <string>

#include "AppContext.h"

class Module {
public:
    virtual ~Module() = default;
    // Key into LanguageProfile::modules, which holds the native and English titles.
    virtual std::string key() const = 0;
    std::string nativeTitle(const AppContext& ctx) const { return ctx.lang.moduleNative(key()); }
    std::string title(const AppContext& ctx) const { return ctx.lang.moduleEnglish(key()); }
    virtual std::string badge(const AppContext&) const { return {}; }  // e.g. "12 due"
    virtual void run(AppContext& ctx) = 0;
};
