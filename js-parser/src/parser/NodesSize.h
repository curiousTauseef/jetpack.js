/* Generated by Ruby Script! */


#pragma once

#include <algorithm>
#include "BaseNodes.h"
#include "SyntaxNodes.h"

namespace rocket_bundle::node_size {

    constexpr int nodes_size_array[] = {
         sizeof(SyntaxNode),
         sizeof(Expression),
         sizeof(Statement),
         sizeof(Pattern),
         sizeof(Declaration),
         sizeof(ArrayExpression),
         sizeof(ArrayPattern),
         sizeof(ArrowFunctionExpression),
         sizeof(AssignmentExpression),
         sizeof(AssignmentPattern),
         sizeof(AwaitExpression),
         sizeof(BinaryExpression),
         sizeof(BlockStatement),
         sizeof(BreakStatement),
         sizeof(CallExpression),
         sizeof(CatchClause),
         sizeof(ClassBody),
         sizeof(ClassDeclaration),
         sizeof(ClassExpression),
         sizeof(ConditionalExpression),
         sizeof(ContinueStatement),
         sizeof(DebuggerStatement),
         sizeof(Directive),
         sizeof(DoWhileStatement),
         sizeof(EmptyStatement),
         sizeof(ExportAllDeclaration),
         sizeof(ExportDefaultDeclaration),
         sizeof(ExportNamedDeclaration),
         sizeof(ExportSpecifier),
         sizeof(ExpressionStatement),
         sizeof(ForInStatement),
         sizeof(ForOfStatement),
         sizeof(ForStatement),
         sizeof(FunctionDeclaration),
         sizeof(FunctionExpression),
         sizeof(Identifier),
         sizeof(IfStatement),
         sizeof(Import),
         sizeof(ImportDeclaration),
         sizeof(ImportDefaultSpecifier),
         sizeof(ImportNamespaceSpecifier),
         sizeof(ImportSpecifier),
         sizeof(LabeledStatement),
         sizeof(Literal),
         sizeof(MetaProperty),
         sizeof(MethodDefinition),
         sizeof(Module),
         sizeof(NewExpression),
         sizeof(ObjectExpression),
         sizeof(ObjectPattern),
         sizeof(Property),
         sizeof(RegexLiteral),
         sizeof(RestElement),
         sizeof(ReturnStatement),
         sizeof(Script),
         sizeof(SequenceExpression),
         sizeof(SpreadElement),
         sizeof(MemberExpression),
         sizeof(Super),
         sizeof(SwitchCase),
         sizeof(SwitchStatement),
         sizeof(TaggedTemplateExpression),
         sizeof(TemplateElement),
         sizeof(TemplateLiteral),
         sizeof(ThisExpression),
         sizeof(ThrowStatement),
         sizeof(TryStatement),
         sizeof(UnaryExpression),
         sizeof(UpdateExpression),
         sizeof(VariableDeclaration),
         sizeof(VariableDeclarator),
         sizeof(WhileStatement),
         sizeof(WithStatement),
         sizeof(YieldExpression),
         sizeof(ArrowParameterPlaceHolder),
         sizeof(JSXClosingElement),
         sizeof(JSXElement),
         sizeof(JSXEmptyExpression),
         sizeof(JSXExpressionContainer),
         sizeof(JSXIdentifier),
         sizeof(JSXMemberExpression),
         sizeof(JSXAttribute),
         sizeof(JSXNamespacedName),
         sizeof(JSXOpeningElement),
         sizeof(JSXSpreadAttribute),
         sizeof(JSXText),
         sizeof(TSParameterProperty),
         sizeof(TSDeclareFunction),
         sizeof(TSDeclareMethod),
         sizeof(TSQualifiedName),
         sizeof(TSCallSignatureDeclaration),
         sizeof(TSConstructSignatureDeclaration),
         sizeof(TSPropertySignature),
         sizeof(TSMethodSignature),
         sizeof(TSIndexSignature),
         sizeof(TSAnyKeyword),
         sizeof(TSBooleanKeyword),
         sizeof(TSBigIntKeyword),
         sizeof(TSNeverKeyword),
         sizeof(TSNullKeyword),
         sizeof(TSNumberKeyword),
         sizeof(TSObjectKeyword),
         sizeof(TSStringKeyword),
         sizeof(TSSymbolKeyword),
         sizeof(TSUndefinedKeyword),
         sizeof(TSUnknownKeyword),
         sizeof(TSVoidKeyword),
         sizeof(TSThisType),
         sizeof(TSFunctionType),
         sizeof(TSConstructorType),
         sizeof(TSTypeReference),
         sizeof(TSTypePredicate),
         sizeof(TSTypeQuery),
         sizeof(TSTypeLiteral),
         sizeof(TSArrayType),
         sizeof(TSTupleType),
         sizeof(TSOptionalType),
         sizeof(TSRestType),
         sizeof(TSUnionType),
         sizeof(TSIntersectionType),
         sizeof(TSConditionalType),
         sizeof(TSInferType),
         sizeof(TSParenthesizedType),
         sizeof(TSTypeOperator),
         sizeof(TSIndexedAccessType),
         sizeof(TSMappedType),
         sizeof(TSLiteralType),
         sizeof(TSExpressionWithTypeArguments),
         sizeof(TSInterfaceDeclaration),
         sizeof(TSInterfaceBody),
         sizeof(TSTypeAliasDeclaration),
         sizeof(TSAsExpression),
         sizeof(TSTypeAssertion),
         sizeof(TSEnumDeclaration),
         sizeof(TSEnumMember),
         sizeof(TSModuleDeclaration),
         sizeof(TSModuleBlock),
         sizeof(TSImportType),
         sizeof(TSImportEqualsDeclaration),
         sizeof(TSExternalModuleReference),
         sizeof(TSNonNullExpression),
         sizeof(TSExportAssignment),
         sizeof(TSNamespaceExportDeclaration),
         sizeof(TSTypeAnnotation),
         sizeof(TSTypeParameterInstantiation),
         sizeof(TSTypeParameterDeclaration),
         sizeof(TSTypeParameter),

    };

    constexpr int max_node_size = *std::max_element(std::begin(nodes_size_array), std::end(nodes_size_array));

}

