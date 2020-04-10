//
// Created by Duzhong Chen on 2020/3/22.
//

#pragma once

#include <memory>
#include <robin_hood.h>
#include "IMString.h"
#include "Utils.h"
#include "../parser/NodeTypes.h"

namespace jetpack {

    class ImportIdentifierInfo {
    public:
        bool is_namespace;
        IMString local_name;
        IMString source_name;
        IMString module_name;

    };

    /**
     * Ref: https://github.com/vincentdchan/webpack-deep-scope-analysis-plugin/blob/master/packages/deep-scope-analyser/src/importManager.ts
     */
    class ImportManager {
    public:
        enum class EC {
            Ok = 0,

            UnknownSpecifier = -1,

        };

        ImportManager() = default;
        ImportManager(const ImportManager&) = delete;

        ImportManager& operator=(const ImportManager&) = delete;

        robin_hood::unordered_map<IMString, ImportIdentifierInfo> id_map;

        EC ResolveImportDecl(const std::shared_ptr<ImportDeclaration>&);

    };

}
