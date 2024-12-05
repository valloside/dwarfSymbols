#pragma once

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>
#include <cstdint>
#include <string>
#include <variant>
#include "loc.hpp"

namespace dw
{
    class attr
    {
    public:
        typedef std::variant<std::string_view, uint64_t, uint32_t, int64_t, int32_t, dw::LocList> value;

    private:
        uint64_t mOffset = 0;
        uint16_t mType;
        uint16_t mForm;
        value    mValue;

    public:
        attr(uint64_t offset, value attrValue, Dwarf_Half attrType, Dwarf_Half attrForm) :
            mOffset(offset), mValue(attrValue), mType(attrType), mForm(attrForm) {}

        attr(uint64_t offset, std::string_view attrValue, Dwarf_Half attrType, Dwarf_Half attrForm) :
            mOffset(offset), mValue(attrValue), mType(attrType), mForm(attrForm) {}

        uint64_t getOffset() const
        {
            return this->mOffset;
        }

        uint16_t getType() const
        {
            return this->mType;
        }

        /**
         * get type as string
         * @return `"DW_AT_external"` for example
         */
        std::string_view getName() const
        {
            const char *tempStr = "";
            dwarf_get_AT_name(this->mType, &tempStr);
            return tempStr;
        }

        uint16_t getAttrForm() const
        {
            return this->mForm;
        }

        /**
         * @return `"DW_FORM_strp"` for example
         */
        std::string_view getFormAsString() const
        {
            const char *tempStr = "";
            dwarf_get_FORM_name(this->mForm, &tempStr);
            return tempStr;
        }

        constexpr std::size_t index() const noexcept
        {
            return this->mValue.index();
        }

        template <typename RET>
        constexpr decltype(auto) get() const noexcept
        {
            return std::get<RET>(this->mValue);
        }

        const value &getValue() const
        {
            return this->mValue;
        }

        std::string getValueAsString() const
        {
            return std::visit(
                [](const auto &var) -> std::string {
                    if constexpr (std::is_same_v<std::decay_t<decltype(var)>, std::string_view>)
                    {
                        return std::string{var};
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint64_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint32_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int64_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int32_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, LocList>)
                    {
                        return var[0].toString();
                    }
                },
                this->mValue);
        }

        template <typename NumberType>
            requires(std::is_integral_v<NumberType>)
        NumberType getValueAsInt() const
        {
            return std::visit(
                [](const auto &var) -> NumberType {
                    if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint64_t>)
                    {
                        return var;
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint32_t>)
                    {
                        return var;
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int64_t>)
                    {
                        return var;
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int32_t>)
                    {
                        return var;
                    }
                    else
                    {
                        return std::numeric_limits<uint64_t>::max();
                    }
                },
                this->mValue);
        }

        bool operator==(const attr rhs) const
        {
            return this->mOffset == rhs.mOffset;
        }

        bool operator==(const uint64_t off) const
        {
            return this->mOffset == off;
        }

        bool operator==(const uint16_t type) const
        {
            return this->mType == type;
        }

        bool operator==(const std::string &name) const
        {
            return this->getFormAsString() == name;
        }
    };

} // namespace dw