#ifndef _SENDPROXY_VARIANT_H
#define _SENDPROXY_VARIANT_H

#include <variant>
#include <string>
#include "mathlib/vector.h"
#include "basehandle.h"

using ProxyVariant = std::variant<int, float, std::string, Vector, CBaseHandle>;

// helper type for the visitor
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif