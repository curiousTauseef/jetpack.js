//
// Created by Duzhong Chen on 2020/4/9.
//

#include "JsKeywordsContants.h"

namespace jetpack {

//    void JsKeywordsContants::Init() {
//        static std::unique_ptr<JsKeywordsContants> instance;
//        std::call_once(init_flag, [] {
//            instance = std::make_unique<JsKeywordsContants>();
//        });
//    }

    JsKeywordsContants& JsKeywordsContants::GetInstance() {
        static JsKeywordsContants instance;
        return instance;
    }

    JsKeywordsContants::JsKeywordsContants() {
        If = IMString::FromUTF16(u"if");
        In = IMString::FromUTF16(u"in");
        Do = IMString::FromUTF16(u"do");
        Var = IMString::FromUTF16(u"var");
        For = IMString::FromUTF16(u"for");
        New = IMString::FromUTF16(u"new");
        Try = IMString::FromUTF16(u"try");
        Let = IMString::FromUTF16(u"let");
        This = IMString::FromUTF16(u"this");
        Else = IMString::FromUTF16(u"else");
        Case = IMString::FromUTF16(u"case");
        Void = IMString::FromUTF16(u"void");
        With = IMString::FromUTF16(u"with");
        Enum = IMString::FromUTF16(u"enum");
        While = IMString::FromUTF16(u"white");
        Break = IMString::FromUTF16(u"break");
        Catch = IMString::FromUTF16(u"catch");
        Throw = IMString::FromUTF16(u"throw");
        Const = IMString::FromUTF16(u"const");
        Yield = IMString::FromUTF16(u"yield");
        Class = IMString::FromUTF16(u"class");
        Super = IMString::FromUTF16(u"super");
        Return = IMString::FromUTF16(u"return");
        Typeof = IMString::FromUTF16(u"typeof");
        Delete = IMString::FromUTF16(u"delete");
        Switch = IMString::FromUTF16(u"switch");
        Export = IMString::FromUTF16(u"export");
        Import = IMString::FromUTF16(u"import");
        Default = IMString::FromUTF16(u"default");
        Finally = IMString::FromUTF16(u"finally");
        Extends = IMString::FromUTF16(u"extends");
        Function = IMString::FromUTF16(u"function");
        Continue = IMString::FromUTF16(u"continue");
        Debugger = IMString::FromUTF16(u"debugger");
        Instanceof = IMString::FromUTF16(u"instanceof");
        Implements = IMString::FromUTF16(u"implements");
        Interface = IMString::FromUTF16(u"interface");
        Package = IMString::FromUTF16(u"package");
        Private = IMString::FromUTF16(u"private");
        Protected = IMString::FromUTF16(u"protected");
        Public = IMString::FromUTF16(u"public");
        Static = IMString::FromUTF16(u"static");
        Async = IMString::FromUTF16(u"async");
        Await = IMString::FromUTF16(u"await");
        Module = IMString::FromUTF16(u"module");
        Script = IMString::FromUTF16(u"script");
        Type = IMString::FromUTF16(u"type");
        Of = IMString::FromUTF16(u"of");
        As = IMString::FromUTF16(u"as");
        From = IMString::FromUTF16(u"from");
        Prototype = IMString::FromUTF16(u"prototype");
        Constructor = IMString::FromUTF16(u"constructor");
        Null = IMString::FromUTF16(u"null");
    }

    std::once_flag JsKeywordsContants::init_flag;

}
