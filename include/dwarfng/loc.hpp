#pragma once
#include <libdwarf/libdwarf.h>
#include <string>
#include <vector>

namespace dw
{
    struct LocationOp
    {
        Dwarf_Small    op = 0;
        Dwarf_Unsigned opd1 = 0;
        Dwarf_Unsigned opd2 = 0;
        Dwarf_Unsigned opd3 = 0;

        std::string toString() const
        {
            const char *name;
            int   res = dwarf_get_OP_name(op, &name);
            if (res == DW_DLV_OK)
            {
                std::string str(name);
                if (this->opd1 != 0)
                    str = std::format("{} {}", str, (int64_t)this->opd1);
                if (this->opd2 != 0)
                    str = std::format("{} {}", str, (int64_t)this->opd2);
                if (this->opd3 != 0)
                    str = std::format("{} {}", str, (int64_t)this->opd3);
                return str;
            }
            return "";
        }
    };

    using LocList = std::vector<LocationOp>;

} // namespace dw