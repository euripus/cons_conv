#ifndef CONVERTER_H
#define CONVERTER_H

#include "CmdLineOptions.h"
#include "InternalRep.h"

class Converter
{
public:
    Converter() {}
    virtual ~Converter() {}

    virtual void Convert()                                                              = 0;
    virtual void ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const = 0;
};

#endif   // CONVERTER_H
