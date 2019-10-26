#include "Parser.h"
#include <cstring>
#include <stdexcept>

#include "./dae_parser/DaeParser.h"
#include "./obj_parser/ObjParser.h"

Parser::FileType CheckFileExtension(const std::string & name)
{
    size_t len = name.length();

    if(len > 4 && strcmp(name.c_str() + (len - 4), ".dae") == 0)
        return Parser::FileType::TYPE_DAE;
    else if(len > 4 && strcmp(name.c_str() + (len - 4), ".obj") == 0)
        return Parser::FileType::TYPE_OBJ;

    return Parser::FileType::Unknown;
}

std::unique_ptr<Parser> Parser::GetParser(Parser::FileType ft)
{
    switch(ft)
    {
        case Parser::FileType::TYPE_DAE: {
            return std::make_unique<DaeParser>();
        }
        case Parser::FileType::TYPE_OBJ: {
            return std::make_unique<ObjParser>();
        }
    }

    throw std::runtime_error("Unknown filetype for parsing.");
}
