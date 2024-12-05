#pragma once
#include <Timer.hpp>

namespace dwarfUtils
{
    std::string simplifyPath(const std::string &path)
    {
        if (path.empty())
            return path;
        std::vector<std::string> components;
        std::istringstream       iss(path);
        std::string              component;

        while (std::getline(iss, component, '/'))
        {
            if (component.empty() || component == ".")
            {
                continue;
            }
            else if (component == ".." && !components.empty())
            {
                components.pop_back();
            }
            else
            {
                components.push_back(component);
            }
        }
        std::string simplified_path;
        for (const std::string &comp : components)
        {
            simplified_path += (simplified_path.empty() ? "" : "/") + comp;
        }
        if (simplified_path.empty())
        {
            simplified_path = "/";
        }
        else if (path[0] == '/')
        {
            simplified_path = '/' + simplified_path;
        }
        return simplified_path;
    }

    std::string escape_json_string(const std::string &str)
    {
        std::string escaped;
        for (char c : str)
        {
            switch (c)
            {
            case '\"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
            }
        }
        return escaped;
    }

    void custom_format(const nlohmann::json &j, std::ofstream &out, int indent = 0)
    {
        const std::string indent_str(indent, ' ');           // 固定缩进字符串
        const std::string child_indent_str(indent + 4, ' '); // 子项的缩进

        if (j.is_object())
        {
            out << "{\n";
            bool first = true;
            for (auto it = j.begin(); it != j.end(); ++it)
            {
                if (!first) out << ",\n";
                first = false;
                out << std::format("{}\"{}\": ", child_indent_str, escape_json_string(it.key()));
                custom_format(it.value(), out, indent + 4);
            }
            out << "\n"
                << indent_str << "}";
        }
        else if (j.is_array())
        {
            // 检查数组元素类型
            bool all_numbers = std::all_of(j.begin(), j.end(), [](const nlohmann::json &elem) {
                return elem.is_number();
            });

            if (all_numbers)
            {
                // 数组全为数字，单行输出
                out << "[";
                for (size_t i = 0; i < j.size(); ++i)
                {
                    if (i > 0) out << ", ";
                    custom_format(j[i], out, 0);
                }
                out << "]";
            }
            else
            {
                // 数组包含非数字元素，多行输出
                out << "[\n";
                for (size_t i = 0; i < j.size(); ++i)
                {
                    if (i > 0) out << ",\n";
                    out << child_indent_str;
                    custom_format(j[i], out, indent + 4);
                }
                out << "\n"
                    << indent_str << "]";
            }
        }
        else
        {
            // 基本类型直接输出
            out << j;
        }
    }

} // namespace dwarfUtils