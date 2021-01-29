#ifndef _TOGTESTLIB_H
#define _TOGTESTLIB_H

#include "TOGTestLib2.h"
#include "TOGoS/Futz.h"

class TOGTestLib {
 public:
  int getValue() {
    TOGTestLib2 ttl2;
    return ttl2.getValue() + TOGoS::getFutzValue();;
  }
};

#endif

