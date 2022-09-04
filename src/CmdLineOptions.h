#ifndef CMDLINEOPTIONS_H
#define CMDLINEOPTIONS_H

#include <string>
#include <vector>

struct CmdLineOptions
{
    bool     geometry_optimize;   // cashe optimization
    bool     plain_text_export;   // export file type
    bool     material_export;     // export material
    bool     geometry;            // export geometry
    bool     animation;           // export animation
    bool     relative;            // animation matrix type export
    uint32_t chan;                // texture channel for TBN calculating

    std::vector<std::string> file_list;

    CmdLineOptions() :
        geometry_optimize(false),
        plain_text_export(false),
        material_export(false),
        geometry(false),
        animation(false),
        relative(false),
        chan(0)
    {}
};

bool ParseCmdLine(int argc, char ** argv, CmdLineOptions & cmd) noexcept;

#endif   // CMDLINEOPTIONS_H
