#pragma once
#include <dwarfng/dwarfng.hpp>
#include <nlohmann/json.hpp>
#include <print>
#include <fstream>
#include "dawrfInfoUtils.hpp"

class dwarf2json
{
    using Json = nlohmann::json;
    dw::file mDbg;
    Json     mOutputJson;

    std::string mDeclFileFilter;

public:
    dwarf2json(std::string_view filePath) :
        mDbg(filePath) {}

    int start(std::string_view filter = "")
    {
        this->mDeclFileFilter = filter;
        if (!this->mDbg.isOpen())
            return -1;
        for (auto &&compileUnit : this->mDbg.getCUs())
        {
            this->parseCU(compileUnit);
            std::println("Finished: {}", compileUnit.getName());
            compileUnit.clearCachedChildren();
        }
        return 0;
    }

    int dumpData()
    {
        static TimerToken token;
        Timer             timer{token};
        std::ofstream     file("out.json");
        if (file.is_open())
        {
            dwarfUtils::custom_format(this->mOutputJson, file);
            std::println("File output to out.json");
            file.close();
            return 0;
        }
        return -1;
    }

private:
    void parseCU(dw::CU &compileUnit)
    {
        for (auto &&child : compileUnit.getChildren(this->mDbg))
        {
            this->parseDIE(compileUnit, child);
        }
    }

#pragma region parseDIE

    void parseDIE(dw::CU &compileUnit, dw::die &DIE)
    {
        static TimerToken token;
        Timer             timer{token};
        uint16_t          tagId = DIE.getTAG();
        switch (tagId)
        {
        case DW_TAG_namespace: {
            std::string_view dieName = DIE.getName();
            if (dieName == "std" || dieName.starts_with("__"))
                break;
        }
        case DW_TAG_class_type:
        case DW_TAG_structure_type:
        case DW_TAG_lexical_block:
            for (auto &&childDIE : DIE.getChildren(this->mDbg))
            {
                this->parseDIE(compileUnit, childDIE);
            }
            break;
        case DW_TAG_subprogram:
            this->parseFunction(compileUnit, DIE);
            break;
        case DW_TAG_enumeration_type:
            this->parseEnum(compileUnit, DIE);
            break;
        case DW_TAG_union_type:
            this->parseUnion(compileUnit, DIE);
            break;
        case DW_TAG_variable:
            this->parseVariable(compileUnit, DIE);
            break;
        case DW_TAG_member:
            this->parseVariable(compileUnit, DIE, true);
            break;
        case DW_TAG_typedef:
            this->parseTypedef(compileUnit, DIE);
            break;
        case DW_TAG_inheritance:
            this->parseInheritance(compileUnit, DIE);
            break;
        case DW_TAG_GNU_template_parameter_pack:
        case DW_TAG_template_type_param:
        case DW_TAG_template_value_param:
            this->parseClassTemplateParams(compileUnit, DIE);
            break;
        default:
            break;
        }
    }

#pragma region parseFunction

    void parseFunction(dw::CU &compileUnit, dw::die &funcDIE)
    {
        static TimerToken token;
        Timer             timer{token};

        if (funcDIE.getName().starts_with("__"))
            return;

        const dw::attr *hasSpecification = funcDIE.findAttrByType(DW_AT_specification);
        if (hasSpecification)
        {
            dw::die *specificFunc = this->mDbg.findDIEbyOffset(hasSpecification->get<uint64_t>());
            if (!specificFunc)
                return;
            std::vector<std::string> path = this->findWhereToStore(compileUnit, *specificFunc);
            if (path.empty())
                return;

            const dw::attr *decl_line = specificFunc->findAttrByType(DW_AT_decl_line);
            std::string     storeKey =
                std::format("{:05}-func: {}", decl_line ? decl_line->get<uint64_t>() : 0, specificFunc->getName("`anonymous`"));

            // 获取形参名
            std::vector<std::string> paramNames;
            std::vector<dw::die *>   laterToParse;
            for (auto &&localInfoDIE : funcDIE.getChildren(this->mDbg))
            {
                uint16_t tagId = localInfoDIE.getTAG();
                switch (tagId)
                {
                case DW_TAG_formal_parameter:
                case DW_TAG_unspecified_parameters:
                    paramNames.emplace_back(localInfoDIE.getName("/*Unnamed*/"));
                    break;
                case DW_TAG_GNU_formal_parameter_pack:
                    paramNames.emplace_back("...args");
                    break;
                default:
                    laterToParse.emplace_back(&localInfoDIE);
                    break;
                }
            }

            // 保存数据
            Json *out = &this->mOutputJson;
            for (auto &&it : path)
            {
                out = &(*out)[it];
            }
            if (out->find(storeKey) == out->end())
                this->parseFunction(compileUnit, *specificFunc);

            Json &funcInfo = (*out)[storeKey];

            if (!paramNames.empty())
                funcInfo["2-param_name"] = paramNames;

            const dw::attr *attr = funcDIE.findAttrByType(DW_AT_linkage_name);
            if (attr)
                funcInfo["0-linkage"] = attr->get<std::string_view>();

            funcInfo["otherOffset"] = funcDIE.getOffset();

            for (auto &&die : laterToParse)
            {
                this->parseDIE(compileUnit, *die);
            }
        }
        else
        {
            Json                     funcInfo;
            std::vector<std::string> path = this->findWhereToStore(compileUnit, funcDIE);
            if (path.empty())
                return;

            const dw::attr *decl_line = funcDIE.findAttrByType(DW_AT_decl_line);
            std::string     storeKey =
                std::format("{:05}-func: {}", decl_line ? decl_line->get<uint64_t>() : 0, funcDIE.getName("`anonymous`"));

            funcInfo["offset"] = funcDIE.getOffset();
            bool isRef = false, isRvRef = false;
            for (auto &&attr : funcDIE.getAttrs())
            {
                uint16_t typeId = attr.getType();
                switch (typeId)
                {
                case DW_AT_name:
                    funcInfo["0-name"] = attr.get<std::string_view>();
                    break;
                case DW_AT_linkage_name:
                    funcInfo["0-linkage"] = attr.get<std::string_view>();
                    break;
                case DW_AT_external:
                    funcInfo["0-external"] = 1;
                    break;
                case DW_AT_accessibility:
                    funcInfo["1-accessibility"] = attr.get<uint64_t>();
                    break;
                case DW_AT_defaulted:
                    funcInfo["1-default"] = attr.get<uint64_t>();
                    break;
                case DW_AT_deleted:
                    funcInfo["1-deleted"] = 1;
                    break;
                case DW_AT_decl_line:
                    funcInfo["0-decl_pos"][0] = attr.get<uint64_t>();
                    break;
                case DW_AT_decl_column:
                    funcInfo["0-decl_pos"][1] = attr.get<uint64_t>();
                    break;
                case DW_AT_virtuality:
                    funcInfo["1-virtual"] = attr.get<uint64_t>();
                    break;
                case DW_AT_inline:
                    funcInfo["1-inline"] = attr.get<uint64_t>();
                    break;
                case DW_AT_vtable_elem_location:
                    funcInfo["1-vtable_loc"] = attr.get<dw::LocList>()[0].opd1;
                    break;
                case DW_AT_reference:
                    funcInfo["1-ref_decorate"] = 1;
                    break;
                case DW_AT_rvalue_reference:
                    funcInfo["1-r_ref_decorate"] = 1;
                    break;
                case DW_AT_artificial:
                    funcInfo["1-artificial"] = 1;
                default:
                    break;
                }
            }
            // 获取返回值
            funcInfo.emplace("1-type", this->getTypeInfo(funcDIE, ""));

            // 获取形参信息和模板参数信息
            std::vector<std::string> paramTypes;
            std::vector<std::string> paramNames;
            std::vector<std::string> templateParams;
            std::vector<dw::die *>   laterToParse;
            for (auto &&localInfoDIE : funcDIE.getChildren(this->mDbg))
            {
                uint16_t tagId = localInfoDIE.getTAG();
                switch (tagId)
                {
                case DW_TAG_formal_parameter:
                    if (localInfoDIE.findAttrByType(DW_AT_artificial))
                    {
                        uint8_t isConst = 0;
                        paramTypes.emplace_back(this->getTypeInfo(localInfoDIE, "{obj_ptr}", &isConst));
                        if (isConst & 1)
                            funcInfo["const_decorate"] = 1;
                    }
                    else
                    {
                        paramTypes.emplace_back(this->getTypeInfo(localInfoDIE, "{}"));
                    }
                    paramNames.emplace_back(localInfoDIE.getName("/*Unnamed*/"));
                    break;
                case DW_TAG_unspecified_parameters:
                    paramTypes.emplace_back("...");
                    paramNames.emplace_back(localInfoDIE.getName("/*Unnamed*/"));
                    break;
                case DW_TAG_GNU_formal_parameter_pack:
                    paramNames.emplace_back("...args");
                    break;
                case DW_TAG_template_type_param:
                    templateParams.emplace_back(localInfoDIE.getName("/*Unnamed*/"));
                    break;
                case DW_TAG_template_value_param:
                    templateParams.emplace_back(this->getTypeInfo(localInfoDIE, localInfoDIE.getName("/*Unnamed*/")));
                    break;
                case DW_TAG_GNU_template_parameter_pack:
                    templateParams.emplace_back(std::format("...{}", localInfoDIE.getName("/*Unnamed*/")));
                    break;
                default:
                    laterToParse.emplace_back(&localInfoDIE);
                    break;
                }
            }
            if (!paramTypes.empty())
                funcInfo.emplace("2-param_type", std::move(paramTypes));

            if (!templateParams.empty())
                funcInfo.emplace("2-template_param", std::move(templateParams));

            if (funcInfo.find("2-param_name") == funcInfo.end())
                funcInfo.emplace("2-param_name", std::move(paramNames));

            // 保存数据
            Json *out = &this->mOutputJson;
            for (auto &&it : path)
            {
                out = &(*out)[it];
            }
            out->emplace(storeKey, std::move(funcInfo));

            for (auto &&die : laterToParse)
            {
                this->parseDIE(compileUnit, *die);
            }
        }
    }

#pragma region parseEnum

    void parseEnum(dw::CU &compileUnit, dw::die &enumDIE)
    {
        static TimerToken token;
        Timer             timer{token};
        Json              enumInfo;

        std::vector<std::string> path = this->findWhereToStore(compileUnit, enumDIE);
        if (path.empty())
            return;

        enumInfo["offset"] = enumDIE.getOffset();

        for (auto &&attr : enumDIE.getAttrs())
        {
            uint16_t               attrType = attr.getType();
            const dw::attr::value &value = attr.getValue();
            switch (attrType)
            {
            case DW_AT_name:
                enumInfo["0-name"] = attr.get<std::string_view>();
                break;
            case DW_AT_enum_class:
                enumInfo["0-enum_class"] = 1;
                break;
            case DW_AT_decl_line:
                enumInfo["0-decl_pos"][0] = attr.getValueAsInt<uint64_t>();
                break;
            case DW_AT_decl_column:
                enumInfo["0-decl_pos"][1] = attr.getValueAsInt<uint64_t>();
                break;
            default:
                break;
            }
        }

        // 获取基础类型
        enumInfo.emplace("1-type", this->getTypeInfo(enumDIE, ""));

        // 获取枚举项
        for (auto &&enumerator : enumDIE.getChildren(this->mDbg))
        {
            if (enumerator.getTAG() == DW_TAG_enumerator)
            {
                const dw::attr *enumVal = enumerator.findAttrByType(DW_AT_const_value);
                if (enumVal->getValue().index() == 1)
                    enumInfo["content"].emplace(enumerator.getName(), enumVal->get<uint64_t>());
                else
                    enumInfo["content"].emplace(enumerator.getName(), enumVal->get<int64_t>());
            }
        }

        const dw::attr *decl_line = enumDIE.findAttrByType(DW_AT_decl_line);
        std::string     storeKey =
            std::format("{:05}-enum: {}", decl_line ? decl_line->get<uint64_t>() : 0, enumDIE.getName("`anonymous`"));

        // 保存数据
        Json *out = &this->mOutputJson;
        for (auto &&it : path)
        {
            out = &(*out)[it];
        }
        out->emplace(storeKey, std::move(enumInfo));
    }

#pragma region parseUnion

    void parseUnion(dw::CU &compileUnit, dw::die &unionDIE)
    {
        static TimerToken        token;
        Timer                    timer{token};
        Json                     unionInfo;
        std::vector<std::string> path = this->findWhereToStore(compileUnit, unionDIE);
        if (path.empty())
            return;

        unionInfo["offset"] = unionDIE.getOffset();
        for (auto &&attr : unionDIE.getAttrs())
        {
            uint16_t               attrType = attr.getType();
            const dw::attr::value &value = attr.getValue();
            switch (attrType)
            {
            case DW_AT_name:
                unionInfo["0-name"] = attr.get<std::string_view>();
                break;
            case DW_AT_decl_line:
                unionInfo["0-decl_pos"][0] = attr.get<uint64_t>();
                break;
            case DW_AT_decl_column:
                unionInfo["0-decl_pos"][1] = attr.get<uint64_t>();
                break;
            case DW_AT_byte_size:
                unionInfo["0-byte_size"] = attr.get<uint64_t>();
                break;
            default:
                break;
            }
        }

        // 保存数据
        Json *out = &this->mOutputJson;
        for (auto &&it : path)
        {
            out = &(*out)[it];
        }
        out->emplace(std::format("union: {}", unionDIE.getName("`anonymous`")), std::move(unionInfo));

        for (auto &&child : unionDIE.getChildren(this->mDbg))
        {
            this->parseDIE(compileUnit, child);
        }
    }

#pragma region parseVariable

    void parseVariable(dw::CU &compileUnit, dw::die &varDIE, bool memberVariable = false)
    {
        static TimerToken token;
        Timer             timer{token};
        Json              variableInfo;

        const dw::attr *hasSpecification = varDIE.findAttrByType(DW_AT_specification);
        if (hasSpecification)
        {
            dw::die *specificVar = this->mDbg.findDIEbyOffset(hasSpecification->get<uint64_t>());
            if (!specificVar)
                return;
            std::vector<std::string> path = this->findWhereToStore(compileUnit, *specificVar);
            if (path.empty())
                return;

            memberVariable = specificVar->getTAG() == DW_TAG_member;

            const dw::attr *decl_line = specificVar->findAttrByType(DW_AT_decl_line);
            std::string     storeKey = std::format("{:05}-{}: {}",
                                               decl_line ? decl_line->get<uint64_t>() : 0,
                                               memberVariable ? "memb" : "var",
                                                   specificVar->getName("`Unnamed`"));

            Json *out = &this->mOutputJson;
            for (auto &&it : path)
            {
                out = &(*out)[it];
            }
            if (out->find(storeKey) == out->end())
                this->parseVariable(compileUnit, *specificVar, memberVariable);
            Json &varJson = (*out)[storeKey];

            for (auto &&attr : varDIE.getAttrs())
            {
                uint16_t typeId = attr.getType();
                switch (typeId)
                {
                case DW_AT_location:
                    varJson.emplace("1-location", attr.get<dw::LocList>()[0].toString());
                    break;
                case DW_AT_linkage_name:
                    varJson["1-linkage"] = attr.get<std::string_view>();
                    break;
                }
            }
        }
        else
        {
            std::vector<std::string> path = this->findWhereToStore(compileUnit, varDIE);
            if (path.empty())
                return;

            // 获取属性
            variableInfo["offset"] = varDIE.getOffset();
            for (auto &&attr : varDIE.getAttrs())
            {
                uint16_t attrType = attr.getType();
                switch (attrType)
                {
                case DW_AT_name:
                    variableInfo["0-name"] = attr.get<std::string_view>();
                    break;
                case DW_AT_decl_line:
                    variableInfo["0-decl_pos"][0] = attr.getValueAsInt<uint64_t>();
                    break;
                case DW_AT_decl_column:
                    variableInfo["0-decl_pos"][1] = attr.getValueAsInt<uint64_t>();
                    break;
                case DW_AT_data_member_location:
                    variableInfo["1-member_location"] = attr.getValueAsInt<uint64_t>();
                    break;
                case DW_AT_declaration:
                    variableInfo["0-declaration"] = 1;
                    break;
                case DW_AT_external:
                    variableInfo["0-external"] = 1;
                    break;
                case DW_AT_accessibility:
                    variableInfo["1-accessibility"] = attr.getValueAsInt<uint64_t>();
                    break;
                case DW_AT_inline:
                    variableInfo["1-inline"] = attr.getValueAsInt<uint64_t>();
                    break;
                case DW_AT_location:
                    variableInfo.emplace("1-location", attr.get<dw::LocList>()[0].toString());
                    break;
                case DW_AT_linkage_name:
                    variableInfo["1-linkage"] = attr.get<std::string_view>();
                    break;
                case DW_AT_const_value:
                    if (attr.index() == 1 || attr.index() == 2)
                        variableInfo["1-const_val"] = attr.getValueAsInt<uint64_t>();
                    else if (attr.index() == 3 || attr.index() == 4)
                        variableInfo["1-const_val"] = attr.getValueAsInt<int64_t>();
                    break;
                case DW_AT_bit_size:
                    variableInfo["1-bit_size"] = attr.get<uint64_t>();
                    break;
                case DW_AT_bit_offset:
                    variableInfo["1-bit_offset"] = attr.get<uint64_t>();
                    break;
                default:
                    break;
                }
            }

            // 获取类型
            variableInfo.emplace("1-type", this->getTypeInfo(varDIE, varDIE.getName("`Unnamed`")));

            const dw::attr *decl_line = varDIE.findAttrByType(DW_AT_decl_line);
            std::string     storeKey = std::format("{:05}-{}: {}",
                                               decl_line ? decl_line->get<uint64_t>() : 0,
                                               memberVariable ? "memb" : "var",
                                                   varDIE.getName("`Unnamed`"));

            // 保存数据
            Json *out = &this->mOutputJson;
            for (auto &&it : path)
            {
                out = &(*out)[it];
            }
            out->emplace(storeKey, std::move(variableInfo));
        }
    }

#pragma region parseTypedef

    void parseTypedef(dw::CU &compileUnit, dw::die &typedefDIE)
    {
        static TimerToken token;
        Timer             timer{token};
        Json              typedefInfo;

        std::vector<std::string> path = this->findWhereToStore(compileUnit, typedefDIE);
        if (path.empty())
            return;

        typedefInfo["offset"] = typedefDIE.getOffset();
        for (auto &&attr : typedefDIE.getAttrs())
        {
            uint16_t attrType = attr.getType();
            switch (attrType)
            {
            case DW_AT_name:
                typedefInfo["0-name"] = attr.get<std::string_view>();
                break;
            case DW_AT_decl_line:
                typedefInfo["0-decl_pos"][0] = attr.get<uint64_t>();
                break;
            case DW_AT_decl_column:
                typedefInfo["0-decl_pos"][1] = attr.get<uint64_t>();
                break;
            }
        }

        // 获取原始类型
        typedefInfo.emplace("1-ori_type", this->getTypeInfo(typedefDIE));

        const dw::attr *decl_line = typedefDIE.findAttrByType(DW_AT_decl_line);
        std::string     storeKey =
            std::format("{:05}-typedef: {}", decl_line ? decl_line->get<uint64_t>() : 0, typedefDIE.getName("`anonymous`"));

        // 保存数据
        Json *out = &this->mOutputJson;
        for (auto &&it : path)
        {
            out = &(*out)[it];
        }
        out->emplace(storeKey, std::move(typedefInfo));
    }

#pragma region parseInheritance

    void parseInheritance(dw::CU &compileUnit, dw::die &inheriDIE)
    {
        static TimerToken token;
        Timer             timer{token};

        dw::die                 &parentDIE = *inheriDIE.getParentDIE();
        std::vector<std::string> path = this->findWhereToStore(compileUnit, parentDIE);
        if (path.empty())
            return;

        path.emplace_back(std::format("{}: {}", parentDIE.getTAG() == DW_TAG_class_type ? "class" : "struct", parentDIE.getName("`anonymous`")));

        const dw::attr *data_loc = inheriDIE.findAttrByType(DW_AT_data_member_location);
        const dw::attr *accessibility = inheriDIE.findAttrByType(DW_AT_accessibility);
        std::string     storeKey = std::format("{:05}-{}", data_loc ? data_loc->get<uint64_t>() : 0, this->getTypeInfo(inheriDIE, ""));
        Json           *out = &this->mOutputJson;
        for (auto &&it : path)
        {
            out = &(*out)[it];
        }
        (*out)["0-inheri"].emplace(storeKey, accessibility ? accessibility->get<uint64_t>() : 0);
    }

#pragma region parseClassTemplateParams

    void parseClassTemplateParams(dw::CU &compileUnit, dw::die &templateDIE)
    {
        static TimerToken        token;
        Timer                    timer{token};
        std::vector<std::string> templateInfo;

        dw::die                 *parent = templateDIE.getParentDIE();
        std::vector<std::string> path = this->findWhereToStore(compileUnit, *templateDIE.getParentDIE());
        if (path.empty())
            return;

        path.emplace_back(std::format("{}: {}", parent->getTAG() == DW_TAG_class_type ? "class" : "struct", parent->getName("`anonymous`")));

        // 保存数据
        Json *out = &this->mOutputJson;
        for (auto &&it : path)
        {
            out = &(*out)[it];
        }
        Json &out_ = (*out)["0-template_param"];
        if (out_.empty())
        {
            uint16_t tagId = templateDIE.getTAG();
            if (tagId == DW_TAG_template_type_param)
                templateInfo.emplace_back(templateDIE.getName("/*Unnamed*/"));
            else if (tagId == DW_TAG_template_value_param)
                templateInfo.emplace_back(this->getTypeInfo(templateDIE, templateDIE.getName("/*Unnamed*/")));
            else if (tagId == DW_TAG_GNU_template_parameter_pack)
                templateInfo.emplace_back(std::format("...", templateDIE.getName("/*Unnamed*/")));
            out_ = templateInfo;
        }
    }

#pragma region findWhereToStore

    /**
     * @brief 查找json存放路径
     *
     * @param die 需要查找的die，其父级应当是命名空间、类、结构体、union或CU，自身不包括在return值中
     * @return e.g. {"/src/foo.h" , "nmspc: mce", "nmspc: `anonymous``", "class: Foo /16", "struct: Bar /16"} \n
     *           或 {"unknownRoute"} 表示查找出错 或 {} 表示无需存放
     */
    std::vector<std::string> findWhereToStore(dw::CU &compileUnit, const dw::die &DIE)
    {
        static TimerToken token;
        Timer             timer{token};
        const dw::attr   *attr = DIE.findAttrByType(DW_AT_decl_file);
        if (!attr)
            return {};

        uint64_t                        declFileIdx = attr->getValueAsInt<uint64_t>();
        const std::vector<std::string> &declFiles = compileUnit.getSrcfiles(this->mDbg);
        if (declFileIdx > declFiles.size())
            return {};

        std::string declFile = dwarfUtils::simplifyPath(declFiles[declFileIdx - 1]);
        if (!declFile.starts_with(this->mDeclFileFilter))
            return {};

        std::vector<std::string> ret;
        for (const dw::die *parentDIE = DIE.getParentDIE(); parentDIE; parentDIE = parentDIE->getParentDIE())
        {
            uint16_t         tag = parentDIE->getTAG();
            std::string_view name = parentDIE->getName("`anonymous`");
            switch (tag)
            {
            case DW_TAG_namespace:
                ret.emplace_back(std::format("namespace: {}", name));
                break;
            case DW_TAG_class_type:
                ret.emplace_back(std::format("class: {}", name));
                break;
            case DW_TAG_structure_type:
                ret.emplace_back(std::format("struct: {}", name));
                break;
            case DW_TAG_union_type:
                ret.emplace_back("content");
                ret.emplace_back(std::format("union: {}", name));
                break;
            case DW_TAG_subprogram: {
                const dw::attr *specificationAttr = parentDIE->findAttrByType(DW_AT_specification);
                if (specificationAttr)
                {
                    uint64_t specificationOffset = specificationAttr->get<uint64_t>();
                    dw::die *specification = this->mDbg.findDIEbyOffset(specificationOffset);
                    if (!specification)
                        break;
                    const dw::attr *decl_line = specification->findAttrByType(DW_AT_decl_line);
                    ret.emplace_back("local_info");
                    ret.emplace_back(std::format("{:05}-func: {}", decl_line ? decl_line->get<uint64_t>() : 0, specification->getName()));
                    const dw::attr *attr = specification->findAttrByType(DW_AT_decl_file);
                    if (attr)
                        declFileIdx = attr->getValueAsInt<uint64_t>();
                    declFile = dwarfUtils::simplifyPath(declFiles[declFileIdx - 1]);
                    parentDIE = specification;
                }
                else
                {
                    const dw::attr *decl_line = parentDIE->findAttrByType(DW_AT_decl_line);
                    ret.emplace_back("local_info");
                    ret.emplace_back(std::format("{:05}-func: {}", decl_line ? decl_line->get<uint64_t>() : 0, parentDIE->getName()));
                }
                break;
            }
            case DW_TAG_lexical_block: {
                ret.emplace_back(std::format("{}-lexical_block", parentDIE->getOffset()));
            }
            case DW_TAG_compile_unit:
                break;
            }
        }
        ret.emplace_back(declFile);
        std::reverse(ret.begin(), ret.end());
        return ret;
    }

#pragma region getTypeInfo
    /**
     * @brief DW_AT_type的类型信息
     *
     * @param die 变量die或函数die
     * @param varName 传入变量名或者占位符
     * @return 格式化完毕的信息, e.g. `"volatile const int *{}[10][20]"`
     */
    std::string getTypeInfo(const dw::die &die, std::string_view varName = "{}", uint8_t *constOrVolatile = nullptr)
    {
        static TimerToken token;
        Timer             timer{token};
        const dw::attr   *typeAttr = die.findAttrByType(DW_AT_type);
        if (!typeAttr)
            return std::format("void {}", varName);

        uint64_t    typeDIEoffset = typeAttr->get<uint64_t>();
        bool        isConst = false, isVolatile = false;
        std::string typeName{varName};
        int8_t      readDirection = 1;
        for (dw::die *typeDIE = this->mDbg.findDIEbyOffset(typeDIEoffset); typeDIE;)
        {
            uint16_t         tagId = typeDIE->getTAG();
            std::string_view name = typeDIE->getName();
            if (!name.empty())
            {
                typeName = std::format("{} {}", this->completeNameScope(*typeDIE), typeName);
                break;
            }
            bool noVoidType = false;
            switch (tagId)
            {
            case DW_TAG_const_type:
                isConst = true;
                break;
            case DW_TAG_volatile_type:
                isVolatile = true;
                break;
            case DW_TAG_pointer_type:
                typeName = "*" + typeName;
                readDirection = -1;
                break;
            case DW_TAG_reference_type:
                typeName = "&" + typeName;
                readDirection = -1;
                break;
            case DW_TAG_rvalue_reference_type:
                typeName = "&&" + typeName;
                readDirection = -1;
                break;
            case DW_TAG_restrict_type:
                typeName = "__restrict " + typeName;
                readDirection = -1;
                break;
            case DW_TAG_array_type:
                if (readDirection == -1)
                    typeName = std::format("({})", typeName);
                for (auto &&child : typeDIE->getChildren(this->mDbg))
                {
                    if (child.getTAG() == DW_TAG_subrange_type)
                    {
                        const dw::attr *countAttr = child.findAttrByType(DW_AT_count);
                        const dw::attr *countAttr2 = child.findAttrByType(DW_AT_upper_bound);
                        if (countAttr)
                        {
                            typeName += std::format("[{}]", countAttr->get<uint64_t>());
                        }
                        else if (countAttr2)
                        {
                            typeName += std::format("[{}]", countAttr2->get<uint64_t>() + 1);
                        }
                        else
                        {
                            typeName += "[no_range]";
                        }
                    }
                }
                readDirection = 1;
                break;
            case DW_TAG_ptr_to_member_type:
                this->parsePtrToMemberType(*typeDIE, typeName);
                readDirection = -1;
                break;
            case DW_TAG_subroutine_type:
                if (readDirection == -1)
                    typeName = std::format("({})", typeName);
                this->parseSubroutineType(*typeDIE, typeName);
                readDirection = 1;
                break;
            case DW_TAG_union_type:
                typeName = std::format("`anony_union_{}` {}", typeDIE->getOffset(), typeName);
                noVoidType = true;
                break;
            case DW_TAG_class_type:
                typeName = std::format("`anony_class_{}` {}", typeDIE->getOffset(), typeName);
                noVoidType = true;
                break;
            case DW_TAG_structure_type:
                typeName = std::format("`anony_struct_{}` {}", typeDIE->getOffset(), typeName);
                noVoidType = true;
                break;
            case DW_TAG_enumeration_type:
                typeName = std::format("`anony_enum_{}` {}", typeDIE->getOffset(), typeName);
                noVoidType = true;
                break;
            }
            const dw::attr *nextTypeAttr = typeDIE->findAttrByType(DW_AT_type);
            if (!nextTypeAttr)
            {
                if (!noVoidType)
                    typeName = "void " + std::move(typeName);
                break;
            }
            typeDIE = this->mDbg.findDIEbyOffset(nextTypeAttr->get<uint64_t>());
        }
        if (constOrVolatile)
            *constOrVolatile = isVolatile * 2 + isConst;
        // (volatile) (const) (typename, with &&/&/*/__restrict/[array])
        return std::format("{}{}{}", isVolatile ? "volatile " : "", isConst ? "const " : "", typeName);
    }

#pragma region completeNameScope
    /**
     * @brief 补全命名作用域
     *
     * @param die die
     * @return e.g. `shared_ptr<int>` -> `std::shared_ptr<int>`
     */
    std::string completeNameScope(const dw::die &die)
    {
        static TimerToken token;
        Timer             timer{token};
        std::string       nameStr = std::format("{}", die.getName());
        for (const dw::die *iterDie = die.getParentDIE(); iterDie; iterDie = iterDie->getParentDIE())
        {
            const uint16_t   tagId = iterDie->getTAG();
            std::string_view name = iterDie->getName();
            switch (tagId)
            {
            case DW_TAG_namespace:
                if (nameStr.empty())
                    nameStr = std::format("`anon_nmsp_{}`::{}", iterDie->getOffset(), nameStr);
                break;
            case DW_TAG_class_type:
                if (nameStr.empty())
                    nameStr = std::format("`anon_class_{}`::{}", iterDie->getOffset(), nameStr);
                break;
            case DW_TAG_structure_type:
                if (nameStr.empty())
                    nameStr = std::format("`anon_struct_{}`::{}", iterDie->getOffset(), nameStr);
                break;
            case DW_TAG_union_type:
                if (nameStr.empty())
                    nameStr = std::format("`anon_union_{}`::{}", iterDie->getOffset(), nameStr);
                break;
            case DW_TAG_enumeration_type:
                if (nameStr.empty())
                    nameStr = std::format("`anon_enum_{}`::{}", iterDie->getOffset(), nameStr);
                break;
            case DW_TAG_compile_unit:
                return nameStr;
            default:
                break;
            }
            nameStr = std::format("{}::{}", name, nameStr);
        }
        return std::string{die.getName()};
    }

#pragma region PtrToMember
    /**
     * @brief 当类型信息出现指向成员函数或成员变量的指针时，由这个处理
     *
     * @param ptrToMembDie
     * @param retName 例：`varName` -> `StructA::*varName`
     */
    void parsePtrToMemberType(const dw::die &ptrToMembDie, std::string &typeName)
    {
        const dw::attr *containingType = ptrToMembDie.findAttrByType(DW_AT_containing_type);
        if (!containingType)
        {
            typeName = "`err_type`::*" + typeName;
            return;
        }
        uint64_t       ctTypeOffset = containingType->get<uint64_t>();
        const dw::die *ctTypeDie = this->mDbg.findDIEbyOffset(ctTypeOffset);
        if (!ctTypeDie)
        {
            typeName = std::format("`err_type_{}`::*{}", ctTypeOffset, typeName);
            return;
        }
        typeName = std::format("{}::*{}", this->completeNameScope(*ctTypeDie), typeName);
    }

#pragma region SubroutineType
    /**
     * @brief
     *
     * @param subroutineDie
     * @param typeName e.g. `(StructA::*varName)` -> `(StructA::*varName)(int, int)`
     */
    void parseSubroutineType(const dw::die &subroutineDie, std::string &typeName)
    {
        typeName += "(";
        bool isConstFunction = false;
        for (auto &&child : subroutineDie.getChildren(this->mDbg))
        {
            uint16_t tagId = child.getTAG();
            switch (tagId)
            {
            case DW_TAG_formal_parameter:
                if (child.findAttrByOffset(DW_AT_artificial))
                {
                    uint8_t isConst = 0;
                    this->getTypeInfo(child, "this", &isConst);
                    isConstFunction = isConst & 1;
                }
                else
                {
                    typeName = std::format("{}{}, ", typeName, this->getTypeInfo(child, ""));
                }
                break;
            case DW_TAG_unspecified_parameters:
                typeName += "..., ";
                break;
            }
        }
        typeName.erase(typeName.length() - 2, 2);

        bool isRef = subroutineDie.findAttrByOffset(DW_AT_reference) != nullptr;
        bool isRvalueRef = subroutineDie.findAttrByOffset(DW_AT_rvalue_reference) != nullptr;

        // {(StructA::*varName)(int, int}){ const}{ &/&&}
        typeName = std::format("{}){}{}", typeName, isConstFunction ? " const" : "", isRef ? " &" : (isRvalueRef ? " &&" : ""));
    }
};