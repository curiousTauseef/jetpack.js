//
// Created by Duzhong Chen on 2020/4/9.
//

#pragma once

#include <thread>
#include <mutex>
#include <memory>

#include <IMString.h>

namespace jetpack {

    /**
     * Boost
     */
    class JsKeywordsContants {
    public:
        static JsKeywordsContants& GetInstance();

        JsKeywordsContants();

        IMString If;
        IMString In;
        IMString Do;
        IMString Var;
        IMString For;
        IMString New;
        IMString Try;
        IMString Let;
        IMString This;
        IMString Else;
        IMString Case;
        IMString Void;
        IMString With;
        IMString Enum;
        IMString While;
        IMString Break;
        IMString Catch;
        IMString Throw;
        IMString Const;
        IMString Yield;
        IMString Class;
        IMString Super;
        IMString Return;
        IMString Typeof;
        IMString Delete;
        IMString Switch;
        IMString Export;
        IMString Import;
        IMString Default;
        IMString Finally;
        IMString Extends;
        IMString Function;
        IMString Continue;
        IMString Debugger;
        IMString Instanceof;
        IMString Implements;
        IMString Interface;
        IMString Package;
        IMString Private;
        IMString Protected;
        IMString Public;
        IMString Static;

        IMString Async;
        IMString Await;
        IMString Module;
        IMString Script;
        IMString Type;
        IMString Of;
        IMString As;
        IMString From;
        IMString Prototype;
        IMString Constructor;
        IMString Null;

        static std::once_flag init_flag;

    };

#define S_IF (JsKeywordsContants::GetInstance().If)
#define S_IN (JsKeywordsContants::GetInstance().In)
#define S_DO (JsKeywordsContants::GetInstance().Do)
#define S_VAR (JsKeywordsContants::GetInstance().Var)
#define S_FOR (JsKeywordsContants::GetInstance().For)
#define S_ASYNC (JsKeywordsContants::GetInstance().Async)
#define S_AWAIT (JsKeywordsContants::GetInstance().Await)
#define S_MODULE (JsKeywordsContants::GetInstance().Module)
#define S_SCRIPT (JsKeywordsContants::GetInstance().Script)
#define S_TYPE (JsKeywordsContants::GetInstance().Type)
#define S_DEFAULT (JsKeywordsContants::GetInstance().Default)
#define S_OF (JsKeywordsContants::GetInstance().Of)
#define S_AS (JsKeywordsContants::GetInstance().As)
#define S_FROM (JsKeywordsContants::GetInstance().From)
#define S_PROTOTYPE (JsKeywordsContants::GetInstance().Prototype)
#define S_CONSTRUCTOR (JsKeywordsContants::GetInstance().Constructor)
#define S_INSTANCEOF (JsKeywordsContants::GetInstance().Instanceof)
#define S_NULL (JsKeywordsContants::GetInstance().Null)
//    IMString New;
//    IMString Try;
//    IMString Let;
//    IMString This;
//    IMString Else;
//    IMString Case;
//    IMString Void;
//    IMString With;
//    IMString Enum;
//    IMString While;
//    IMString Break;
//    IMString Catch;
//    IMString Throw;
//    IMString Const;
//    IMString Yield;
//    IMString Class;
//    IMString Super;
//    IMString Return;
//    IMString Typeof;
//    IMString Delete;
//    IMString Switch;
//    IMString Export;
//    IMString Import;
//    IMString Default;
//    IMString Finally;
//    IMString Extends;
//    IMString Function;
//    IMString Continue;
//    IMString Debugger;
//    IMString Instanceof;
//    IMString Implements;
//    IMString Interface;
//    IMString Package;
//    IMString Private;
//    IMString Protected;
//    IMString Public;
//    IMString Static;

}

