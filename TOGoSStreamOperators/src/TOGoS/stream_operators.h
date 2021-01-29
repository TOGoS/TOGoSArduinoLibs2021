#ifndef _TOGOS_STREAM_OPERATORS_H
#define _TOGOS_STREAM_OPERATORS_H

#include <Print.h>

namespace TOGoS {
    class StringView;
}

Print & operator<<(Print &p, const TOGoS::StringView& sv);
inline Print & operator<<(Print &p, char thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, int thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, unsigned int thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, long thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, unsigned long thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, double thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, const char *thing) { p.print(thing); return p; }
inline Print & operator<<(Print &p, const Printable &obj) { obj.printTo(p); return p; }

#endif
