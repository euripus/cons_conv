#ifndef EXPORTER_H
#define EXPORTER_H

#include "CmdLineOptions.h"
#include "InternalRep.h"
#include <memory>

class Exporter
{
public:
    Exporter()          = default;
    virtual ~Exporter() = default;

    virtual void WriteFile(std::string const & basic_fname, InternalData const & rep) const = 0;

    static std::unique_ptr<Exporter> GetExporter(CmdLineOptions const & cmd);
};

#endif   // EXPORTER_H
