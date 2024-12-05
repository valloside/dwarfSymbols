#pragma once

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>
#include "cxxabi.h"
#include <string>
#include <cstdint>

namespace dw
{
    // spit by the last "/"
    std::pair<std::string_view, std::string_view> splitPath(std::string_view fullPath)
    {
        size_t      lastSlashPos = fullPath.find_last_of("/\\");
        if (lastSlashPos != std::string::npos)
            return {fullPath.substr(0, lastSlashPos), fullPath.substr(lastSlashPos + 1)};
        else
            return {"", fullPath};
    }

    // spit by the last "."
    std::pair<std::string_view, std::string_view> splitExtension(std::string_view path)
    {
        size_t      lastDotPos = path.find_last_of('.');
        if (lastDotPos != std::string::npos && lastDotPos != path.length() - 1)
            return {path.substr(0, lastDotPos), path.substr(lastDotPos + 1)};
        else
            return {path, ""};
    }

    /**
     * @brief return a string_view of the DW_TAG_xxx value
     * @param tag DW_TAG_xxx value
     * @return string_view
     */
    std::string_view tagToString(uint16_t tag)
    {
        const char *tempStr = nullptr;
        dwarf_get_AT_name(tag, &tempStr);
        return tempStr;
    }

    std::string cxx_demangler(const std::string &symbol)
    {
        char *demangled;
        int   status = 0;
        demangled = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
        if (!demangled)
            return symbol;
        std::string result(demangled);
        free(demangled);
        return result;
    }

} // namespace dw