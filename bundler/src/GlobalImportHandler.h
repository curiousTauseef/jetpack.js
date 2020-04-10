//
// Created by Duzhong Chen on 2020/4/3.
//

#pragma once

#include <vector>
#include <set>
#include "Utils.h"
#include "parser/SyntaxNodes.h"
#include "UniqueNameGenerator.h"
#include "codegen/CodeGen.h"

namespace jetpack {

    struct GlobalImportInfo {
    public:
        std::int32_t id = -1;

        bool has_namespace = false;
        IMString ns_import_name;

        bool has_default = false;
        IMString default_local_name;

        IMString path;

        std::vector<IMString> names;

        HashMap<IMString, IMString> alias_map;

    };

    /**
     * Manage global import declarations
     */
    class GlobalImportHandler {
    public:
        void HandleImport(const Sp<ImportDeclaration>& import);

        bool IsImportExternal(const Sp<ImportDeclaration>& import);

        void DistributeNameToImportVars(const std::shared_ptr<UniqueNameGenerator>&,
                                        const std::vector<GlobalImportInfo*>& infos);

        void GenAst(const std::shared_ptr<UniqueNameGenerator>&);

        void GenCode(const CodeGen::Config &config, std::ostream& os);

        std::vector<Sp<ImportDeclaration>> imports;

        /**
         * use set of pointers to mark external imports
         */
        std::set<std::uintptr_t> external_import_ptrs;

        HashMap<IMString, GlobalImportInfo> import_infos;

        std::vector<Sp<ImportDeclaration>> gen_import_decls;

        std::int32_t import_counter = 0;

    };

}
