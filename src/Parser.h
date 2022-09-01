#ifndef PARSER_H
#define PARSER_H

#include "Converter.h"
#include <memory>
#include <string>

class Parser
{
public:
    // supported file formats
    enum class FileType
    {
        Unknown,
        TYPE_DAE,
        TYPE_OBJ,
    };

    Parser()          = default;
    virtual ~Parser() = default;

    virtual void                       Parse(std::string const & fname, CmdLineOptions const & cmd) = 0;
    virtual std::unique_ptr<Converter> GetConverter() const                                         = 0;

    static std::unique_ptr<Parser> GetParser(FileType ft);
};

Parser::FileType CheckFileExtension(std::string const & name);

#endif   // PARSER_H
