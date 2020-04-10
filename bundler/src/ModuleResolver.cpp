//
// Created by Duzhong Chen on 2020/3/20.
//

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <tsl/ordered_map.h>
#include <parser/ParserCommon.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>

#include "Path.h"
#include "./ModuleResolver.h"
#include "./Utils.h"
#include "./Error.h"
#include "./codegen/CodeGen.h"

namespace jetpack {
    using fmt::format;
    using parser::ParserContext;
    using parser::Parser;

    static const char* PackageJsonName = "package.json";

    inline Sp<Identifier> MakeId(const IMString& content) {
        auto id = std::make_shared<Identifier>();
        id->name = content;
        return id;
    }

    inline Sp<Identifier> MakeId(const std::string& content) {
        return MakeId(IMString::FromUTF8(content));
    }

    inline Sp<SyntaxNode> MakeModuleVar(const IMString& var_name) {
        auto mod_var = std::make_shared<VariableDeclaration>();
        mod_var->kind = VarKind::Const;

        auto declarator = std::make_shared<VariableDeclarator>();
        declarator->id = MakeId(var_name);

        declarator->init = { std::make_shared<ObjectExpression>() };

        mod_var->declarations.push_back(std::move(declarator));
        return mod_var;
    }

    inline Sp<Literal> MakeStringLiteral(const IMString& str) {
        auto lit = std::make_shared<Literal>();
        lit->ty = Literal::Ty::String;
        lit->str_ = str;
        lit->raw = IMString::FromUTF16(u"\"") + str + IMString::FromUTF16(u"\"");
        return lit;
    }

    inline Sp<Literal> MakeNull() {
        auto null_lit = std::make_shared<Literal>();
        null_lit->ty = Literal::Ty::Null;
        null_lit->str_ = S_NULL;
        null_lit->raw = S_NULL;

        return null_lit;
    }

    inline void AddKeyValueToObject(const std::shared_ptr<ObjectExpression>& expr,
                                    const IMString& key,
                                    const IMString& value) {

        auto prop = std::make_shared<Property>();
        prop->key = MakeId(key);
        prop->value = MakeId(value);

        expr->properties.push_back(std::move(prop));
    }

    void ModuleFile::RenameInnerScopes(RenamerCollection& renamer) {
        std::vector<Sp<MinifyNameGenerator>> result;
        result.reserve(ast->scope->children.size());
        for (auto child : ast->scope->children) {
            result.push_back(RenameInnerScopes(*child, renamer.idLogger.get()));
        }

        auto final = MinifyNameGenerator::Merge(result);
        {
            std::lock_guard<std::mutex> lk(renamer.mutex_);
            renamer.content.push_back(std::move(final));
        }
    }

    Sp<MinifyNameGenerator> ModuleFile::RenameInnerScopes(Scope &scope, UnresolvedNameCollector* idLogger) {
        std::vector<Sp<MinifyNameGenerator>> temp;
        temp.reserve(scope.children.size());

        for (auto child : scope.children) {
            temp.push_back(RenameInnerScopes(*child, idLogger));
        }

        std::vector<std::tuple<IMString, IMString>> renames;
        auto renamer = MinifyNameGenerator::Merge(temp);

        for (auto& variable : scope.own_variables) {
            auto new_opt = renamer->Next(variable.first);
            if (new_opt.has_value()) {
                renames.emplace_back(variable.first, *new_opt);
            }
        }

        scope.BatchRenameSymbols(renames);

        return renamer;
    }

    void ModuleFile::CodeGenFromAst(const CodeGen::Config &config) {
        std::stringstream ss;

        if (config.comments) {
            ss << "// " << this->path << std::endl;
        }

        CodeGen codegen(config, ss);
        codegen.Traverse(ast);
        codegen_result = ss.str();
    }

    IMString ModuleFile::GetModuleVarName() {
        std::string tmp = "mod_" + std::to_string(id);
        return IMString::FromUTF8(tmp);
    }

    ModuleResolveException::ModuleResolveException(const std::string& path, const std::string& content)
    : file_path(path), error_content(content) {

    }

    void WokerErrorCollection::PrintToStdErr() {
        for (auto& err : errors) {
            std::cerr << "File: " << err.file_path << std::endl;
            std::cerr << "Error: " << err.error_content << std::endl;
        }
    }

    void ModuleResolveException::PrintToStdErr() {
        std::cerr << "File: " << file_path << std::endl;
        std::cerr << "Error: " << error_content << std::endl;
    }

    void ModuleResolver::BeginFromEntry(const parser::ParserContext::Config& config,
                                        std::string base_path,
                                        std::string target_path) {
        std::string path;
        if (target_path.empty()) {
            return;
        } else if (target_path[0] == Path::PATH_DIV) {
            path = target_path;
        } else {
            Path p(utils::GetRunningDir());
            p.Join(target_path);
            path = p.ToString();
        }

        auto thread_pool_size = std::thread::hardware_concurrency();
        thread_pool_ = std::make_unique<ThreadPool>(thread_pool_size);

        EnqueueOne([this, &config, &path] {
            try {
                ParseFileFromPath(config, path);
            } catch (parser::ParseError& ex) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                worker_errors_.push_back({ path, ex.ErrorMessage() });
            } catch (VariableExistsError& err) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                std::string message = format("variable '{}' has been defined, location: {}:{}",
                                             err.name,
                                             err.exist_var->location.start_.line_,
                                             err.exist_var->location.start_.column_);
                worker_errors_.push_back({ path, std::move(message) });
            } catch (std::exception& ex) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                worker_errors_.push_back({ path, ex.what() });
            }
            FinishOne();
        });

        std::unique_lock<std::mutex> lk(main_lock_);
        main_cv_.wait(lk, [this] {
            return finished_files_count_ >= enqueued_files_count_;
        });

        if (!worker_errors_.empty()) {
            WokerErrorCollection col;
            col.errors = std::move(worker_errors_);
            throw std::move(col);
        }
    }

    void ModuleResolver::BeginFromEntryString(const parser::ParserContext::Config& config,
                                              const char16_t* value) {

        IMString src = IMString::FromUTF16(value);
        auto ctx = std::make_shared<ParserContext>(src, config);
        Parser parser(ctx);

        entry_module = std::make_shared<ModuleFile>();
        entry_module->id = mod_counter_++;
        entry_module->module_resolver = shared_from_this();
        entry_module->path = "memory0";

        parser.import_decl_created_listener.On([this] (const Sp<ImportDeclaration>& import_decl) {
            std::string u8path = import_decl->source->str_.ToString();
            global_import_handler_.HandleImport(import_decl);
        });

        modules_map_[entry_module->path] = entry_module;

        entry_module->ast = parser.ParseModule();

        std::vector<Sp<Identifier>> unresolved_ids;
        entry_module->ast->scope->ResolveAllSymbols(&unresolved_ids);

        id_logger_->InsertByList(unresolved_ids);
    }

    void ModuleResolver::ParseFileFromPath(const parser::ParserContext::Config& config,
                                           const std::string &path) {
        if (!utils::IsFileExist(path)) {
            Path exist_path(path);
            if (!exist_path.EndsWith(".js")) {
                exist_path.slices[exist_path.slices.size() - 1] += ".js";
                ParseFileFromPath(config, exist_path.ToString());
                return;
            }

            WorkerError err { path, std::string("file doen't exist: ") + path };
            worker_errors_.emplace_back(std::move(err));
            return;
        }
        entry_module = std::make_shared<ModuleFile>();
        entry_module->id = mod_counter_++;
        entry_module->module_resolver = shared_from_this();
        entry_module->path = path;

        {
            std::lock_guard<std::mutex> guard(map_mutex_);
            if (modules_map_.find(path) != modules_map_.end()) {
                return;  // exists;
            }
            modules_map_[path] = entry_module;
        }

        ParseFile(config, entry_module);
    }

    bool ModuleResolver::IsExternalImportModulePath(const std::string &path) {
        // TODO: if alias path

        if (path.empty()) {
            return false;
        }

        return path[0] != '.' && path[0] != '/';
    }

    void ModuleResolver::ParseFile(const parser::ParserContext::Config& config,
                                   Sp<ModuleFile> mf) {
        IMString src = ReadFileStream(mf->path);
        auto ctx = std::make_shared<ParserContext>(src, config);
        Parser parser(ctx);

        parser.import_decl_created_listener.On([this, &config, &mf] (const Sp<ImportDeclaration>& import_decl) {
            std::string u8path = import_decl->source->str_.ToString();
            if (IsExternalImportModulePath(u8path)) {
                global_import_handler_.HandleImport(import_decl);
                return;
            }
            HandleNewLocationAdded(config, mf, true, u8path);
        });
        parser.export_named_decl_created_listener.On([this, &config, &mf] (const Sp<ExportNamedDeclaration>& export_decl) {
            if (export_decl->source.has_value()) {
                std::string u8path = (*export_decl->source)->str_.ToString();
                HandleNewLocationAdded(config, mf, false, u8path);
            }
        });
        parser.export_all_decl_created_listener.On([this, &config, &mf] (const Sp<ExportAllDeclaration>& export_decl) {
            std::string u8path = export_decl->source->str_.ToString();
            HandleNewLocationAdded(config, mf, false, u8path);
        });

        mf->ast = parser.ParseModule();

        std::vector<Sp<Identifier>> unresolved_ids;
        mf->ast->scope->ResolveAllSymbols(&unresolved_ids);

        id_logger_->InsertByList(unresolved_ids);
    }

    void ModuleResolver::HandleNewLocationAdded(const jetpack::parser::ParserContext::Config &config,
                                                const Sp<jetpack::ModuleFile> &mf, bool is_import,
                                                const std::string &path) {
        if (!trace_file) return;

        Path module_path(mf->path);
        module_path.Pop();
        module_path.Join(path);

        auto source_path = module_path.ToString();

        if (!utils::IsFileExist(source_path)) {
            if (!module_path.EndsWith(".js")) {
                module_path.slices[module_path.slices.size() - 1] += ".js";
                source_path = module_path.ToString();
            }

            if (!utils::IsFileExist(source_path)) {
                WorkerError err { source_path, std::string("file doesn't exist: ") + source_path };
                worker_errors_.emplace_back(std::move(err));
                return;
            }
        }

        mf->resolved_map[path] = source_path;

        std::shared_ptr<ModuleFile> new_mf;
        {
            std::lock_guard<std::mutex> guard(map_mutex_);
            auto exist_iter = modules_map_.find(source_path);
            if (exist_iter != modules_map_.end()) {  // exists
                mf->ref_mods.push_back(exist_iter->second);
                return;
            }

            new_mf = std::make_shared<ModuleFile>();
            new_mf->id = mod_counter_++;
            new_mf->module_resolver = shared_from_this();
            new_mf->path = std::move(source_path);
            modules_map_[new_mf->path] = new_mf;
        }

        mf->ref_mods.push_back(new_mf);

        EnqueueOne([this, &config, new_mf] {
            try {
                ParseFile(config, new_mf);
            } catch (parser::ParseError& ex) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                worker_errors_.push_back({ new_mf->path, ex.ErrorMessage() });
            } catch (VariableExistsError& err) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                std::string message = format("variable '{}' has been defined, location: {}:{}",
                                             err.name,
                                             err.exist_var->location.start_.line_ + 1,
                                             err.exist_var->location.start_.column_);
                worker_errors_.push_back({ new_mf->path, std::move(message) });
            } catch (std::exception& ex) {
                std::lock_guard<std::mutex> guard(error_mutex_);
                worker_errors_.push_back({ new_mf->path, ex.what() });
            }
            FinishOne();
        });
    }

    IMString ModuleResolver::ReadFileStream(const std::string& filename) {
        std::ifstream t(filename);
        std::string str((std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());
        return IMString::FromUTF8(str);
    }

    void ModuleResolver::PrintStatistic() {
        std::unique_lock<std::mutex> lk(error_mutex_);
        if (!worker_errors_.empty()) {
            PrintErrors();
            return;
        }
        lk.unlock();

        ModuleScope& mod_scope = *entry_module->ast->scope;

        json exports = json::array();
        for (auto& tuple : GetAllExportVars()) {
            exports.push_back(std::get<1>(tuple).ToString());
        }

        json result = json::object();
        result["entry"] = entry_module->path;
        result["importStat"] = GetImportStat();
        result["totalFiles"] = finished_files_count_;
        result["exports"] = std::move(exports);

        if (!worker_errors_.empty()) {
            PrintErrors();
            return;
        }

        std::cout << result.dump(2) << std::endl;
    }

    std::vector<std::tuple<Sp<ModuleFile>, IMString>> ModuleResolver::GetAllExportVars() {
        std::vector<std::tuple<Sp<ModuleFile>, IMString>> result;

        TraverseModulePushExportVars(result, entry_module, nullptr);

        return result;
    }

    /**
     * white_list: only export specific names, example:
     *
     * export { a as b } from './another';
     *
     * Then a would be in white list
     */
    void ModuleResolver::TraverseModulePushExportVars(std::vector<std::tuple<Sp<ModuleFile>, IMString>>& arr,
                                                      const Sp<jetpack::ModuleFile>& mod,
                                                      HashSet<IMString>* white_list) {

        if (mod->visited_mark) {
            return;
        }
        mod->visited_mark = true;

        // 1. handle all local exports
        for (auto& tuple : mod->GetExportManager().local_exports_name) {
            if (white_list && white_list->find(tuple.first) == white_list->end()) {
                continue;
            }
            arr.emplace_back(mod, tuple.first);
        }

        // 2. handle all external exports
        auto external_infos = mod->GetExportManager().CollectExternalInfos();
        for (auto& info : external_infos) {
            auto u8relative_path = info.relative_path.ToString();
            auto resolved_path = mod->resolved_map.find(u8relative_path);
            if (resolved_path == mod->resolved_map.end()) {
                WorkerError err { mod->path, format("resolve path failed: {}", u8relative_path) };
                worker_errors_.emplace_back(std::move(err));
                return;
            }

            auto iter = modules_map_.find(resolved_path->second);
            if (iter == modules_map_.end()) {
                WorkerError err { mod->path, format("module not found: {}", resolved_path->second) };
                worker_errors_.emplace_back(std::move(err));
                return;
            }
            if (info.is_export_all) {
                TraverseModulePushExportVars(arr, iter->second, nullptr);
            } else {
                HashSet<IMString> new_white_list;
                for (auto& item : info.names) {
                    new_white_list.insert(item.source_name);
                }
                TraverseModulePushExportVars(arr, iter->second, &new_white_list);
            }
        }
    }

    void ModuleResolver::PrintErrors() {
        for (auto& error : worker_errors_) {
            std::cerr << "File: " << error.file_path << std::endl;
            std::cerr << "Error: " << error.error_content << std::endl;
        }
    }

    void ModuleResolver::EnqueueOne(std::function<void()> unit) {
        {
            std::lock_guard<std::mutex> lk(main_lock_);
            enqueued_files_count_++;
        }

#ifdef JETPACK_SINGLE_THREAD
        unit();
#else
        thread_pool_->enqueue(std::move(unit));
#endif
    }

    void ModuleResolver::FinishOne() {
        {
            std::lock_guard<std::mutex> lk(main_lock_);
            finished_files_count_++;
        }
        main_cv_.notify_one();
    }

    json ModuleResolver::GetImportStat() {
        json result = json::object();

        auto module_ast = entry_module->ast;
        ModuleScope* mod_scope = module_ast->scope.get();

        auto& import_manager = mod_scope->import_manager;

        for (auto& tuple : import_manager.id_map) {
            result[tuple.first.ToString()] = tuple.second.module_name.ToString();
        }

        return result;
    }

    /**
     * 1. rename all first root variables in all modules
     * 2. replace all export declarations
     * 3. replace all import declarations
     * 4. generate final export declaration
     */
    void ModuleResolver::CodeGenAllModules(const CodeGen::Config& config, const std::string& out_path) {
        enqueued_files_count_ = 0;
        finished_files_count_ = 0;

        auto final_export_vars = GetAllExportVars();

        // distribute root level var name
        if (config.minify) {
            RenameAllInnerScopes();
        }

        global_import_handler_.GenAst(name_generator);  // global import

        RenameAllRootLevelVariable();

        ClearAllVisitedMark();
        TraverseRenameAllImports(entry_module);

        // BEGIN every modules gen their own code
        ClearAllVisitedMark();
        for (auto& tuple : modules_map_) {
            EnqueueOne([this, mod = tuple.second, config] {
                mod->CodeGenFromAst(config);
                FinishOne();
            });
        }

        std::unique_lock<std::mutex> lk(main_lock_);
        main_cv_.wait(lk, [this] {
            return finished_files_count_ >= enqueued_files_count_;
        });

        // END every modules gen their own code

        std::ofstream out(out_path, std::ios::out);
        global_import_handler_.GenCode(config, out);

        ClearAllVisitedMark();
        MergeModules(entry_module, out);

        auto final_export = GenFinalExportDecl(final_export_vars);
        CodeGen codegen(config, out);
        codegen.Traverse(final_export);

        out.close();
    }

    void ModuleResolver::RenameAllInnerScopes() {
        ClearAllVisitedMark();
        RenamerCollection collection;
        collection.idLogger = id_logger_;

        for (auto& tuple : modules_map_) {
            EnqueueOne([this, mod = tuple.second, &collection] {
                mod->RenameInnerScopes(collection);
                FinishOne();
            });
        }
        std::unique_lock<std::mutex> lk(main_lock_);
        main_cv_.wait(lk, [this] {
            return finished_files_count_ >= enqueued_files_count_;
        });
        if (!worker_errors_.empty()) {
            WokerErrorCollection col;
            col.errors = std::move(worker_errors_);
            throw std::move(col);
        }
        std::vector<std::tuple<IMString, IMString>> renames;
        name_generator = MinifyNameGenerator::Merge(collection.content, id_logger_);
    }

    void ModuleResolver::RenameAllRootLevelVariable() {
        ClearAllVisitedMark();

        std::int32_t counter = 0;
        RenameAllRootLevelVariableTraverser(entry_module, counter);
    }

    void ModuleResolver::RenameAllRootLevelVariableTraverser(const std::shared_ptr<ModuleFile> &mf,
                                                             std::int32_t& counter) {
        if (mf->visited_mark) {
            return;
        }
        mf->visited_mark = true;

        for (auto& weak_child : mf->ref_mods) {
            auto child = weak_child.lock();
            RenameAllRootLevelVariableTraverser(child, counter);
        }

        // do your own work
        std::vector<Scope::PVar> variables;
        for (auto& tuple : mf->ast->scope->own_variables) {
            variables.push_back(tuple.second);
        }

        std::sort(std::begin(variables), std::end(variables), [] (const Scope::PVar& p1, const Scope::PVar& p2) {
            return p1->identifiers.size() > p2->identifiers.size();
        });

        // RenameSymbol() will change iterator, call it later
        std::vector<std::tuple<IMString, IMString>> rename_vec;

        // Distribute new name to root level variables
        for (auto& var : variables) {
            auto new_name_opt = name_generator->Next(var->name);

            if (new_name_opt.has_value()) {
                rename_vec.emplace_back(var->name, *new_name_opt);
            }
        }

        mf->ast->scope->BatchRenameSymbols(rename_vec);

        // replace exports to variable declaration
        ReplaceExports(mf);
    }

    /**
     * Turn import and export statements into variable declarations
     */
    void ModuleResolver::ReplaceExports(const std::shared_ptr<ModuleFile>& mf) {
        std::vector<Sp<SyntaxNode>> new_body;

        for (auto& stmt : mf->ast->body) {
            switch (stmt->type) {

                /**
                 * export const name = 3;
                 *
                 * TO
                 *
                 * const name = 3;
                 *
                 * AND
                 *
                 * REMOVE export {}
                 */
                case SyntaxNodeType::ExportNamedDeclaration: {
                    auto export_named_decl = std::dynamic_pointer_cast<ExportNamedDeclaration>(stmt);
                    if (export_named_decl->declaration.has_value()) {  // is local
                        new_body.push_back(*export_named_decl->declaration);
                    }
                    break;
                }

                /**
                 * export default 1 + 1;
                 *
                 * TO
                 *
                 * var default_0 = 1 + 1;
                 */
                case SyntaxNodeType::ExportDefaultDeclaration: {
                    auto export_default_decl = std::dynamic_pointer_cast<ExportDefaultDeclaration>(stmt);
                    IMString new_name = IMString::FromUTF16(u"_default");
                    auto new_name_opt = name_generator->Next(new_name);
                    if (new_name_opt.has_value()) {
                        new_name = *new_name_opt;
                    }

                    mf->default_export_name = new_name;

                    if (export_default_decl->declaration->IsExpression()) {
                        auto exist_id = std::dynamic_pointer_cast<Expression>(export_default_decl->declaration);

                        auto var_decl = std::make_shared<VariableDeclaration>();
                        var_decl->kind = VarKind::Var;

                        auto var_dector = std::make_shared<VariableDeclarator>();

                        auto new_id = MakeId(new_name);
                        var_dector->id = new_id;
                        var_dector->init = { exist_id };

                        mf->ast->scope->CreateVariable(new_id, VarKind::Var);

                        var_decl->declarations.push_back(var_dector);

                        new_body.push_back(var_decl);
                    } else {
                        switch (export_default_decl->declaration->type) {
                            /**
                             * Case 1:
                             *
                             * export default function() {
                             * }
                             *
                             * TO
                             *
                             * function default_0() {
                             * }
                             *
                             * Case 2:
                             *
                             * export default function name() {
                             * }
                             *
                             * TO
                             *
                             * function name() {
                             * }
                             *
                             * const default_0 = name;
                             *
                             */
                            case SyntaxNodeType::FunctionDeclaration: {
                                auto fun_decl = std::dynamic_pointer_cast<FunctionDeclaration>(export_default_decl->declaration);

                                auto new_id = MakeId(new_name);
                                mf->ast->scope->CreateVariable(new_id, VarKind::Var);
                                if (fun_decl->id.has_value()) {
                                    stmt = fun_decl;

                                    auto var_decl = std::make_shared<VariableDeclaration>();
                                    var_decl->kind = VarKind::Var;

                                    auto dector = std::make_shared<VariableDeclarator>();

                                    dector->id = new_id;


                                    auto right_id = MakeId((*fun_decl->id)->name);
                                    mf->ast->scope->CreateVariable(right_id, VarKind::Var);

                                    dector->init = { right_id };

                                    var_decl->declarations.push_back(dector);

                                    new_body.push_back(stmt);
                                    new_body.push_back(var_decl);
                                } else {
                                    fun_decl->id = { MakeId(new_name) };

                                    new_body.push_back(fun_decl);
                                }
                                break;
                            }

                            /**
                             * Similar to FunctionDeclaration
                             */
                            case SyntaxNodeType::ClassDeclaration: {
                                auto cls_decl = std::dynamic_pointer_cast<ClassDeclaration>(export_default_decl->declaration);

                                auto new_id = MakeId(new_name);
                                mf->ast->scope->CreateVariable(new_id, VarKind::Var);

                                if (cls_decl->id.has_value()) {
                                    stmt = cls_decl;

                                    auto var_decl = std::make_shared<VariableDeclaration>();
                                    var_decl->kind = VarKind::Var;

                                    auto dector = std::make_shared<VariableDeclarator>();

                                    dector->id = new_id;

                                    auto right_id = std::make_shared<Identifier>();
                                    right_id->name = (*cls_decl->id)->name;

                                    dector->init = { right_id };

                                    var_decl->declarations.push_back(dector);

                                    new_body.push_back(stmt);
                                    new_body.push_back(var_decl);
                                } else {
                                    cls_decl->id = { new_id };

                                    new_body.push_back(cls_decl);
                                }
                                break;
                            }

                            default:
                                break;

                        }
                    }

                    auto export_info = mf->GetExportManager().local_exports_name[S_DEFAULT];
                    if (export_info) {
                        export_info->local_name = new_name;
                    }

                    break;
                }

                /**
                 * remove export all
                 */
                case SyntaxNodeType::ExportAllDeclaration:
                    continue;

                default:
                    new_body.push_back(stmt);
                    break;

            }
        }

        mf->ast->body = std::move(new_body);
    }

    void ModuleResolver::TraverseRenameAllImports(const Sp<jetpack::ModuleFile> &mf) {
        if (mf->visited_mark) {
            return;
        }
        mf->visited_mark = true;

        for (auto& weak_ptr : mf->ref_mods) {
            auto ptr = weak_ptr.lock();
            TraverseRenameAllImports(ptr);
        }

        ReplaceImports(mf);
    }

    void ModuleResolver::ReplaceImports(const Sp<jetpack::ModuleFile> &mf) {
        std::vector<Sp<SyntaxNode>> new_body;

        for (auto& stmt : mf->ast->body) {
            switch (stmt->type) {
                case SyntaxNodeType::ImportDeclaration: {
                    auto import_decl = std::dynamic_pointer_cast<ImportDeclaration>(stmt);
                    if (global_import_handler_.IsImportExternal(import_decl)) {  // remove from body
                        RenameExternalImports(mf, import_decl);
                        continue;
                    }

                    std::vector<Sp<VariableDeclaration>> result;
                    HandleImportDeclaration(mf, import_decl, result);
                    new_body.insert(std::end(new_body), std::begin(result), std::end(result));
                    continue;
                }

                default:
                    new_body.push_back(stmt);
                    break;

            }
        }

        mf->ast->body = std::move(new_body);
    }

    void ModuleResolver::RenameExternalImports(const Sp<jetpack::ModuleFile> &mf,
                                               const Sp<jetpack::ImportDeclaration> &import_decl) {
        std::vector<std::tuple<IMString, IMString>> renames;

        auto& source_path = import_decl->source->str_;
        auto& info = global_import_handler_.import_infos[source_path];

        for (auto& spec : import_decl->specifiers) {
            switch (spec->type) {
                case SyntaxNodeType::ImportDefaultSpecifier: {
                    auto import_default = spec->As<ImportDefaultSpecifier>();
                    auto& local_name = import_default->local->name;

                    if (local_name != info.default_local_name) {
                        renames.emplace_back(local_name, info.default_local_name);
                    }
                    break;
                }

                case SyntaxNodeType::ImportSpecifier: {
                    auto import_spec = spec->As<ImportSpecifier>();

                    if (auto iter = info.alias_map.find(import_spec->local->name); iter != info.alias_map.end()) {
                        renames.emplace_back(import_spec->local->name, iter->second);
                    }
                    break;
                }

                case SyntaxNodeType::ImportNamespaceSpecifier: {
                    auto import_ns = spec->As<ImportNamespaceSpecifier>();
                    auto& local_name = import_ns->local->name;

                    if (local_name != info.ns_import_name) {
                        renames.emplace_back(local_name, info.ns_import_name);
                    }
                    break;
                }

                default:
                    break;

            }

        }

        mf->ast->scope->BatchRenameSymbols(renames);
    }

    /**
     * import { a, b as c } from './mod1';
     *
     * TO
     *
     * const { a, b: c } = mod_1;
     */
    void ModuleResolver::HandleImportDeclaration(const Sp<ModuleFile>& mf,
                                                 Sp<jetpack::ImportDeclaration> &import_decl,
                                                 std::vector<Sp<VariableDeclaration>>& result) {
        auto full_path_iter = mf->resolved_map.find(import_decl->source->str_.ToString());
        if (full_path_iter == mf->resolved_map.end()) {
            throw ModuleResolveException(
                    mf->path,
                    format("can not resolver path: {}", import_decl->source->str_.ToString()));
        }

        auto target_module = modules_map_[full_path_iter->second];

        if (target_module == nullptr) {
            throw ModuleResolveException(mf->path, format("can not find module: {}", full_path_iter->second));
        }

//        auto target_mode_name = target_module->GetModuleVarName();

        if (import_decl->specifiers.empty()) {
            return;
        }

        if (import_decl->specifiers[0]->type == SyntaxNodeType::ImportNamespaceSpecifier) {
            auto import_ns = std::dynamic_pointer_cast<ImportNamespaceSpecifier>(import_decl->specifiers[0]);

            auto decl = std::make_shared<VariableDeclaration>();
            decl->kind = VarKind::Var;

            auto declarator = std::make_shared<VariableDeclarator>();

            auto new_id = MakeId(import_ns->local->name);

            // debug
//            std::cout << utils::To_UTF8(ast->scope->own_variables[import_ns->local->name].name) << std::endl;

            auto pvar = mf->ast->scope->own_variables[import_ns->local->name];
            if (pvar == nullptr) {
                return;
            }
            pvar->identifiers.push_back(new_id);

            declarator->id = new_id;  // try not to use old ast

            auto obj = std::make_shared<ObjectExpression>();

            {
                auto __proto__ = std::make_shared<Property>();

                __proto__->key = MakeId("__proto__");
                __proto__->value = MakeNull();

                obj->properties.push_back(__proto__);
            }

            {
                auto& relative_path = import_decl->source->str_;
                std::string absolute_path = mf->resolved_map[relative_path.ToString()];

                auto ref_mod = modules_map_[absolute_path];
                if (ref_mod == nullptr) {
                    throw ModuleResolveException(mf->path, format("can not resolve path: {}", absolute_path));
                }

                auto& export_manager = ref_mod->GetExportManager();

                for (auto& tuple : export_manager.local_exports_name) {
                    auto prop = std::make_shared<Property>();

                    prop->kind = VarKind::Get;
                    prop->key = MakeId(tuple.first);

                    auto fun = std::make_shared<FunctionExpression>();
                    auto block = std::make_shared<BlockStatement>();
                    auto ret_stmt = std::make_shared<ReturnStatement>();
                    ret_stmt->argument = { MakeId(tuple.second->local_name) };

                    block->body.push_back(std::move(ret_stmt));
                    fun->body = std::move(block);
                    prop->value = std::move(fun);
                    obj->properties.push_back(std::move(prop));
                }
            }

            declarator->init = { obj };

            decl->declarations.push_back(std::move(declarator));
            result.push_back(std::move(decl));
        } else {
            for (auto& spec : import_decl->specifiers) {
                std::string absolute_path;
                IMString target_export_name;
                IMString import_local_name;
                switch (spec->type) {
                    case SyntaxNodeType::ImportDefaultSpecifier: {
                        auto default_spec = std::dynamic_pointer_cast<ImportDefaultSpecifier>(spec);
                        auto& relative_path = import_decl->source->str_;
                        absolute_path = mf->resolved_map[relative_path.ToString()];
                        target_export_name = S_DEFAULT;
                        import_local_name = default_spec->local->name;
                        break;
                    }

                    case SyntaxNodeType::ImportSpecifier: {
                        auto import_spec = std::dynamic_pointer_cast<ImportSpecifier>(spec);
                        auto& relative_path = import_decl->source->str_;
                        absolute_path = mf->resolved_map[relative_path.ToString()];
                        target_export_name = import_spec->imported->name;
                        import_local_name = import_spec->local->name;
                        break;
                    }

                    default:
                        throw ModuleResolveException(mf->path, "unknown specifier type");

                }

                auto ref_mod = modules_map_[absolute_path];
                if (ref_mod == nullptr) {
                    throw ModuleResolveException(mf->path, format("can not resolve path: {}", absolute_path));
                }

                std::set<std::int32_t> visited_mods;
                auto local_export_opt = FindLocalExportByPath(absolute_path, target_export_name, visited_mods);

                if (!local_export_opt.has_value()) {
                    throw ModuleResolveException(
                        mf->path,
                        format("can not find export variable '{}' from {}", target_export_name.ToString(), absolute_path)
                    );
                }

                std::vector<std::tuple<IMString, IMString>> changeset;
                changeset.emplace_back(import_local_name, (*local_export_opt)->local_name);
                if (!mf->ast->scope->BatchRenameSymbols(changeset)) {
                    throw ModuleResolveException(
                        mf->path,
                        format("rename symbol failed: {}", import_local_name.ToString())
                    );
                }
            }
        }
    }

    std::optional<Sp<LocalExportInfo>>
    ModuleResolver::FindLocalExportByPath(const std::string &path,
                                          const IMString& export_name,
                                          std::set<std::int32_t>& visited) {
        auto mod = modules_map_[path];
        if (mod == nullptr) {
            return std::nullopt;
        }

        if (visited.find(mod->id) != visited.end()) {
            return std::nullopt;
        }
        visited.insert(mod->id);

        auto local_iter = mod->GetExportManager().local_exports_name.find(export_name);
        if (local_iter != mod->GetExportManager().local_exports_name.end()) {  // found
            return { local_iter->second };
        }

        // not in local export
        for (auto& tuple : mod->GetExportManager().external_exports_map) {
            auto& relative_path = tuple.second.relative_path;
            auto absolute_path = mod->resolved_map[relative_path.ToString()];

            if (tuple.second.is_export_all) {
                auto tmp_result = FindLocalExportByPath(absolute_path, export_name, visited);
                if (tmp_result.has_value()) {  // the variable you find is in this opt
                    return tmp_result;
                }
            } else {
                for (auto& alias : tuple.second.names) {
                    if (alias.export_name == export_name) {  // eventually find you!
                        return FindLocalExportByPath(absolute_path, alias.source_name, visited);
                    }
                }
            }
        }

        return std::nullopt;
    }

    void ModuleResolver::MergeModules(const Sp<ModuleFile> &mf, std::ofstream &out) {
        if (mf->visited_mark) {
            return;
        }
        mf->visited_mark = true;

        for (auto& ref : mf->ref_mods) {
            auto new_mf = ref.lock();
            MergeModules(new_mf, out);
        }

        out << mf->codegen_result << std::endl;
    }

    Sp<ExportNamedDeclaration> ModuleResolver::GenFinalExportDecl(const std::vector<std::tuple<Sp<ModuleFile>, IMString>>& export_names) {
        auto result = std::make_shared<ExportNamedDeclaration>();

        for (auto& tuple : modules_map_) {
            tuple.second->visited_mark = false;
        }

        for (auto& tuple : export_names) {
            const IMString& export_name = std::get<1>(tuple);
            const Sp<ModuleFile>& mf = std::get<0>(tuple);

            auto iter = mf->GetExportManager().local_exports_name.find(export_name);
            if (iter == mf->GetExportManager().local_exports_name.end()) {
                WorkerError err {
                        mf->path,
                        format("symbol not found failed: {}", export_name.ToString())
                };
                worker_errors_.emplace_back(std::move(err));
                break;
            }
            auto info = iter->second;

            auto spec = std::make_shared<ExportSpecifier>();
            spec->local = MakeId(info->local_name);
            spec->exported = MakeId(export_name);
            result->specifiers.push_back(std::move(spec));
        }

        return result;
    }

    std::optional<std::string> ModuleResolver::FindPathOfPackageJson(const std::string &entry_path) {
        Path _path(entry_path);
        _path.slices.pop_back();

        while (_path.slices.empty()) {
            _path.slices.push_back(PackageJsonName);

            auto full_path = _path.ToString();
            if (utils::IsFileExist(full_path)) {
                return { full_path };
            }

            _path.slices.pop_back();
            _path.slices.pop_back();
        }

        return std::nullopt;
    }

}
