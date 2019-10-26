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

    virtual void WriteFile(const std::string & basic_fname, const InternalData & rep) const = 0;

    static std::unique_ptr<Exporter> GetExporter(const CmdLineOptions & cmd);
};

#endif   // EXPORTER_H
