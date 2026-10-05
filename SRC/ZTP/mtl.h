#ifndef ZTP_MTL_H
#define ZTP_MTL_H

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ztp_mtl
{
inline std::string trim(const std::string & value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

inline std::vector<std::string> tokens(const std::string & line)
{
    std::vector<std::string> result;
    std::string token;
    bool quoted = false;
    for (char character : line)
    {
        if (character == '"') quoted = !quoted;
        else if (character == '#' && !quoted) break;
        else if ((character == ' ' || character == '\t' || character == '\r') && !quoted)
        {
            if (!token.empty()) { result.push_back(token); token.clear(); }
        }
        else token += character;
    }
    if (!token.empty()) result.push_back(token);
    return result;
}

inline std::string join(const std::vector<std::string> & values, std::size_t first)
{
    std::string result;
    for (; first < values.size(); ++first)
    {
        if (!result.empty()) result += ' ';
        result += values[first];
    }
    return result;
}

inline std::string directory(const std::string & path)
{
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? "" : path.substr(0, separator + 1);
}

inline std::string resolve(const std::string & base, const std::string & path)
{
    if (path.empty()) return "";
    if (path[0] == '/' || path[0] == '\\' || (path.size() > 1 && path[1] == ':')) return path;
    return base + path;
}

inline bool numeric(const std::string & value)
{
    char * end = NULL;
    std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0';
}

inline std::string diffuseMap(const std::vector<std::string> & values)
{
    std::size_t index = 1;
    while (index < values.size() && !values[index].empty() && values[index][0] == '-')
    {
        const std::string option = values[index++];
        if (option == "-o" || option == "-s" || option == "-t")
        {
            unsigned int count = 0;
            while (index < values.size() && count < 3 && numeric(values[index])) { ++index; ++count; }
            if (count == 0) return "";
        }
        else
        {
            const unsigned int count = option == "-mm" ? 2 :
                (option == "-blendu" || option == "-blendv" || option == "-cc" ||
                 option == "-clamp" || option == "-texres" || option == "-bm" ||
                 option == "-imfchan" || option == "-type" || option == "-boost") ? 1 : 0;
            if (count == 0 || index + count >= values.size()) return "";
            index += count;
        }
    }
    return join(values, index);
}

typedef std::map<std::string, std::string> MaterialTextures;

inline void parse(std::istream & input, const std::string & folder, MaterialTextures & materials)
{
    std::string line, material;
    while (std::getline(input, line))
    {
        const std::vector<std::string> values = tokens(line);
        if (values.empty()) continue;
        if (values[0] == "newmtl")
        {
            material = join(values, 1);
            if (!material.empty()) materials[material] = "";
        }
        else if (values[0] == "map_Kd" && !material.empty())
            materials[material] = resolve(folder, diffuseMap(values));
    }
}

inline MaterialTextures load(std::istream & obj, const std::string & objPath, std::ostream & log)
{
    MaterialTextures materials;
    std::string line;
    bool declared = false;
    obj.clear();
    obj.seekg(0, std::ios::beg);
    while (std::getline(obj, line))
    {
        const std::vector<std::string> values = tokens(line);
        if (values.empty() || values[0] != "mtllib") continue;
        declared = true;
        for (std::size_t index = 1; index < values.size(); ++index)
        {
            const std::string path = resolve(directory(objPath), values[index]);
            std::ifstream mtl(path.c_str());
            if (mtl) parse(mtl, directory(path), materials);
            else log << "WARNING: Cannot read MTL " << path << "; using legacy textures.\n";
        }
    }
    if (!declared)
    {
        const std::size_t extension = objPath.find_last_of('.');
        const std::string path = objPath.substr(0, extension) + ".mtl";
        std::ifstream mtl(path.c_str());
        if (mtl) parse(mtl, directory(path), materials);
    }
    obj.clear();
    obj.seekg(0, std::ios::beg);
    return materials;
}
}

#endif