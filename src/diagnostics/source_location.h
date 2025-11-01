//===--- source_location.h - Source Location -------------------------*- C++ -*-===//

#ifndef PAW_SOURCE_LOCATION_H
#define PAW_SOURCE_LOCATION_H

#include <string>

namespace pawc {

struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;
};

} // namespace pawc

#endif // PAW_SOURCE_LOCATION_H
