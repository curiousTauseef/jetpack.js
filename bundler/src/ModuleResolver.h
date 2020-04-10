//
// Created by Duzhong Chen on 2020/3/20.
//

#pragma once

#include <nlohmann/json.hpp>
#include <tsl/ordered_map.h>
#include <ThreadPool.h>
#include <condition_variable>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <fstream>
#include <mutex>
#include <parser/Parser.hpp>

#include "./UniqueNameGenerator.h"
#include "GlobalImportHandler.h"
#include "codegen/CodeGen.h"

namespace jetpack {

    template<class Key, class T, class Ignore, class Allocator,
        class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
        class AllocatorPair = typename std::allocator_traits<Allocator>::template rebind_alloc<std::pair<Key, T>>,
        class ValueTypeContainer = std::vector<std::pair<Key, T>, AllocatorPair>>
    using ordered_map = tsl::ordered_map<Key, T, Hash, KeyEqual, AllocatorPair, ValueTypeContainer>;
    using json = nlohmann::basic_json<ordered_map>;

    class ModuleResolver;

    class WorkerError {
    public:
        std::string file_path;
        std::string error_content;

    };

    class ModuleResolveException : std::exception {
    public:
        std::string file_path;
        std::string error_content;

        ModuleResolveException(const std::string& path, const std::string& content);

        virtual void PrintToStdErr();

    };

    class WokerErrorCollection : public ModuleResolveException {
    public:
        std::vector<WorkerError> errors;

        WokerErrorCollection(): ModuleResolveException("", "") {}

        void PrintToStdErr() override;

    };

    struct RenamerCollection {
    public:
        std::vector<Sp<MinifyNameGenerator>> content;
        std::shared_ptr<UnresolvedNameCollector> idLogger;

        std::mutex mutex_;

    };

    class ModuleFile {
    public:
        /**
         * Unique id in module resolver
         */
        std::int32_t id = 0;

        /**
         * Abosolute path
         */
        std::string path;

        IMString default_export_name;

        std::weak_ptr<ModuleResolver> module_resolver;

        Sp<Module> ast;

        /**
         * relative path -> absolute path
         */
        HashMap<std::string, std::string> resolved_map;

        /**
         * Temp for parallel codegen
         */
        std::string codegen_result;

        /**
         * For Postorder traversal
         */
        bool visited_mark = false;

        /**
         * For Postorder traversal
         */
        std::vector<std::weak_ptr<ModuleFile>> ref_mods;

        void RenameInnerScopes(RenamerCollection& col);
        Sp<MinifyNameGenerator> RenameInnerScopes(Scope& scope, UnresolvedNameCollector* idLogger);

        void CodeGenFromAst(const CodeGen::Config &config);

        IMString GetModuleVarName();

        inline ExportManager& GetExportManager() {
            return ast->scope->export_manager;
        }

    };

    /**
     * Parsing source file in different threads.
     * The error messages would be collected.
     */
    class ModuleResolver : public std::enable_shared_from_this<ModuleResolver> {
    public:
        static IMString ReadFileStream(const std::string& filename);

        ModuleResolver() : mod_counter_(0) {
            name_generator = ReadableNameGenerator::Make();
            id_logger_ = std::make_shared<UnresolvedNameCollector>();
        }

        void BeginFromEntry(const parser::ParserContext::Config& config,
                            std::string base_path,
                            std::string origin_path);

        void BeginFromEntryString(const parser::ParserContext::Config& config,
                                  const char16_t* str);

        void ParseFileFromPath(const parser::ParserContext::Config& config,
                               const std::string& path);

        void ParseFile(const parser::ParserContext::Config& config,
                       Sp<ModuleFile>);

        inline void SetTraceFile(bool val) {
            trace_file = val;
        }

        inline bool GetTraceFile() const {
            return trace_file;
        }

        void PrintStatistic();

        void PrintErrors();

        void CodeGenAllModules(const CodeGen::Config& config, const std::string& out_path);

        void RenameAllInnerScopes();

        void MergeModules(const Sp<ModuleFile>& mf, std::ofstream& out_path);

        inline void ClearAllVisitedMark() {
            for (auto& tuple : modules_map_) {
                tuple.second->visited_mark = false;
            }
        }

        inline void SetNameGenerator(std::shared_ptr<UniqueNameGenerator> generator) {
            name_generator = std::move(generator);
        }

        inline std::int32_t ModCount() const {
            return mod_counter_;
        }

        std::optional<std::string> FindPathOfPackageJson(const std::string& entry_path);

        HashMap<std::string, Sp<ModuleFile>> modules_map_;

        json GetImportStat();
        std::vector<std::tuple<Sp<ModuleFile>, IMString>> GetAllExportVars();

        void RenameAllRootLevelVariable();

        inline Sp<ModuleFile> GetEntryModule() {
            return entry_module;
        }

    private:
        void TraverseModulePushExportVars(
                std::vector<std::tuple<Sp<ModuleFile>, IMString>>& arr,
                const Sp<ModuleFile>&,
                HashSet<IMString>* white_list);

        void RenameAllRootLevelVariableTraverser(const Sp<ModuleFile>& mf,
                                                 std::int32_t& counter);

        void HandleNewLocationAdded(const parser::ParserContext::Config& config,
                                    const Sp<ModuleFile>& mf,
                                    bool is_import,
                                    const std::string& path);

    public:
        void ReplaceExports(const Sp<ModuleFile>& mf);

    private:
        void EnqueueOne(std::function<void()> unit);
        void FinishOne();

        void TraverseRenameAllImports(const Sp<ModuleFile>& mf);

        void ReplaceImports(const Sp<ModuleFile>& mf);

        void RenameExternalImports(const Sp<ModuleFile>& mf, const Sp<ImportDeclaration>& import_decl);

        void HandleImportDeclaration(const Sp<ModuleFile>& mf,
                                     Sp<ImportDeclaration>& import_decl,
                                     std::vector<Sp<VariableDeclaration>>& result);

        bool IsExternalImportModulePath(const std::string& path);

        std::optional<Sp<LocalExportInfo>>
        FindLocalExportByPath(const std::string& path, const IMString& export_name, std::set<std::int32_t>& visited);

        Sp<ExportNamedDeclaration> GenFinalExportDecl(const std::vector<std::tuple<Sp<ModuleFile>, IMString>>&);

        GlobalImportHandler global_import_handler_;

        std::shared_ptr<UniqueNameGenerator> name_generator;

        std::shared_ptr<UnresolvedNameCollector> id_logger_;

        std::mutex map_mutex_;

        Sp<ModuleFile> entry_module;

        std::unique_ptr<ThreadPool> thread_pool_;

        std::vector<WorkerError> worker_errors_;
        std::mutex error_mutex_;

        std::int32_t enqueued_files_count_ = 0;
        std::int32_t finished_files_count_ = 0;

        std::mutex main_lock_;
        std::condition_variable main_cv_;

        std::atomic<std::int32_t> mod_counter_;

        bool trace_file = true;

    };

}
