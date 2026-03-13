#include "../system.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

using namespace clang;
using namespace clang::tooling;

extern "C"
{
#include "lmdb.h"
}

#include "../bitarray/bitarray.h"
#include "../iter/iter.h"
#include "../iter/generated_iterator.h"
#include "../graphdb/db.h"
#include "../graphdb/schema.h"
#include "../graphdb/envpool.h"
#include "../graphdb/transaction.h"
#include "../graphdb/graphdb.h"
#include "../graph/feature.h"
#include "../graph/graph.h"
#include "../graphdb/envpool.h"

#include "schema.h"

/**
 * Tutorial `<https://clang.llvm.org/docs/RAVFrontendAction.html>`_
 * API and doc
 *   `</llvm-project/clang/include/clang/AST/RecursiveASTVisitor.h>`_
 *   `<https://discourse.llvm.org/t/new-design-of-recursiveastvisitor/16068>`_
 */

class FindNamedClassVisitor : public RecursiveASTVisitor<FindNamedClassVisitor>
{
public:
    bool TraverseFunctionDecl(FunctionDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::FunctionDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseFunctionDecl(declaration);
        ret = Breakpoint(Ancestry::FunctionDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,9>{Ancestry::LinkageSpecDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::NamespaceDecl,
                                                          Ancestry::FriendDecl,
                                                          Ancestry::FunctionDecl,  // indirect
                                                          Ancestry::FunctionTemplateDecl,
                                                          Ancestry::CXXMethodDecl,
                                                          Ancestry::CXXDestructorDecl,
                                                          Ancestry::CXXConstructorDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitFunctionDecl(const FunctionDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,6>{Ancestry::FunctionDecl,
                                                          Ancestry::CXXMethodDecl,
                                                          Ancestry::CXXConstructorDecl,
                                                          Ancestry::CXXDestructorDecl,
                                                          Ancestry::CXXConversionDecl,
                                                          Ancestry::CXXDeductionGuideDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        extensions::graphdb::schema::list_key_t::value_type vtx = 0;
        extensions::graphdb::schema::vertex_feature_key_t key = {extensions::rsse::schema::GRAPH.graph(),
                                                                 vtx,
                                                                 extensions::rsse::schema::feature::NAME};
        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::READ);
            auto key = IsTokenInTheDB(txn, token, location);
            if(   key.attribute()  // ie. no key in the DB has 0 as an attribute
               && extensions::rsse::schema::GRAPH.graph() == key.graph())
            {
                ancestry_.back().vertex_ = key.vertex();
                return true;
            }
        }

        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
            vtx = extensions::graphdb::graph::is_available(txn, extensions::rsse::schema::GRAPH, vtx);
            key.vertex() = vtx;
            ancestry_.back().vertex_ = vtx;

            key.attribute() = extensions::rsse::schema::feature::NAME;
            extensions::graphdb::feature::write(txn.vertex_, key, token);

            key.attribute() = extensions::rsse::schema::feature::RULE;
            extensions::graphdb::feature::write(txn.vertex_, key, extensions::rsse::schema::rule::FUNCTION_DECL);

            key.attribute() = extensions::rsse::schema::feature::FILE;
            extensions::graphdb::feature::write(txn.vertex_, key, location);

            extensions::graphdb::graph::vertex(txn, extensions::rsse::schema::GRAPH, vtx, true);
        }
        return true;
    }

    bool TraverseParmVarDecl(ParmVarDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::ParmVarDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseParmVarDecl(declaration);
        ret = Breakpoint(Ancestry::ParmVarDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,18>{Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::FieldDecl,
                                                           Ancestry::TypedefDecl,
                                                           Ancestry::FunctionDecl,
                                                           Ancestry::ParmVarDecl,
                                                           Ancestry::TypeAliasDecl,
                                                           Ancestry::VarDecl,
                                                           Ancestry::TemplateTypeParmDecl,
                                                           Ancestry::DecompositionDecl,  // indirect
                                                           Ancestry::VarTemplateSpecializationDecl,  // indirect
                                                           Ancestry::NonTypeTemplateParmDecl,
                                                           Ancestry::StaticAssertDecl,  // indirect
                                                           Ancestry::CXXRecordDecl,  // indirect
                                                           Ancestry::CXXConversionDecl,  // indirect
                                                           Ancestry::CXXDestructorDecl,  // indirect
                                                           Ancestry::CXXConstructorDecl,
                                                           Ancestry::CXXMethodDecl,
                                                           Ancestry::CXXDeductionGuideDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitParmVarDecl(const ParmVarDecl *declaration)
    {
        assertm(Ancestry::ParmVarDecl == ancestry_.back().kind_, ancestry_.back());
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        extensions::graphdb::schema::list_key_t::value_type vtx = 0;
        extensions::graphdb::schema::vertex_feature_key_t key = {extensions::rsse::schema::GRAPH.graph(),
                                                                 vtx,
                                                                 extensions::rsse::schema::feature::NAME};
        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::READ);
            auto key = IsTokenInTheDB(txn, token, location);
            if(   key.attribute()  // ie. no key in the DB has 0 as an attribute
               && extensions::rsse::schema::GRAPH.graph() == key.graph())
            {
                ancestry_.back().vertex_ = key.vertex();
                goto RETURN;
            }
        }

        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
            vtx = extensions::graphdb::graph::is_available(txn, extensions::rsse::schema::GRAPH, vtx);
            key.vertex() = vtx;
            ancestry_.back().vertex_ = vtx;

            key.attribute() = extensions::rsse::schema::feature::NAME;
            extensions::graphdb::feature::write(txn.vertex_, key, token);

            key.attribute() = extensions::rsse::schema::feature::RULE;
            extensions::graphdb::feature::write(txn.vertex_, key, extensions::rsse::schema::rule::PARM_VAR_DECL);

            key.attribute() = extensions::rsse::schema::feature::FILE;
            extensions::graphdb::feature::write(txn.vertex_, key, location);

            extensions::graphdb::graph::vertex(txn, extensions::rsse::schema::GRAPH, vtx, true);

            auto parent = second_back();
            if(!parent.has_value()) goto RETURN;
            assertm(parent.value()->traversed_ && parent.value()->visited_, ancestry_);

            extensions::graphdb::graph::edge(txn, extensions::rsse::schema::GRAPH, parent.value()->vertex_, ancestry_.back().vertex_, true);
            extensions::graphdb::graph::edge(txn, extensions::rsse::schema::GRAPH, ancestry_.back().vertex_, parent.value()->vertex_, true);
        }

    RETURN:
        return true;
    }

    bool TraverseCXXRecordDecl(CXXRecordDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXRecordDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXRecordDecl(declaration);
        ret = Breakpoint(Ancestry::CXXRecordDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,13>{Ancestry::TranslationUnitDecl,
                                                           Ancestry::LinkageSpecDecl,
                                                           Ancestry::NamespaceDecl,
                                                           Ancestry::ClassTemplateDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::ClassTemplateSpecializationDecl,
                                                           Ancestry::FunctionDecl,
                                                           Ancestry::FriendDecl,
                                                           Ancestry::VarDecl,  // indirect
                                                           Ancestry::CXXConstructorDecl,  // indirect
                                                           Ancestry::CXXRecordDecl,
                                                           Ancestry::CXXConversionDecl,
                                                           Ancestry::CXXMethodDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool TraverseRecordDecl(RecordDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::RecordDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseRecordDecl(declaration);
        ret = Breakpoint(Ancestry::RecordDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::TranslationUnitDecl,
                                                          Ancestry::RecordDecl,
                                                          Ancestry::FunctionDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitRecordDecl(const RecordDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,4>{Ancestry::RecordDecl,
                                                          Ancestry::CXXRecordDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        extensions::graphdb::schema::list_key_t::value_type vtx = 0;
        extensions::graphdb::schema::vertex_feature_key_t key = {extensions::rsse::schema::GRAPH.graph(),
                                                                 vtx,
                                                                 extensions::rsse::schema::feature::NAME};
        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::READ);
            auto key = IsTokenInTheDB(txn, token, location);
            if(   key.attribute()  // ie. no key in the DB has 0 as an attribute
               && extensions::rsse::schema::GRAPH.graph() == key.graph())
            {
                ancestry_.back().vertex_ = key.vertex();
                return true;
            }
        }
        {
            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
            vtx = extensions::graphdb::graph::is_available(txn, extensions::rsse::schema::GRAPH, vtx);
            key.vertex() = vtx;
            ancestry_.back().vertex_ = vtx;
            
            key.attribute() = extensions::rsse::schema::feature::NAME;
            extensions::graphdb::feature::write(txn.vertex_, key, token);

            key.attribute() = extensions::rsse::schema::feature::RULE;
            extensions::graphdb::feature::write(txn.vertex_, key, extensions::rsse::schema::rule::RECORD_DECL);

            key.attribute() = extensions::rsse::schema::feature::FILE;
            extensions::graphdb::feature::write(txn.vertex_, key, location);

            extensions::graphdb::graph::vertex(txn, extensions::rsse::schema::GRAPH, vtx, true);
        }
        return true;
    }

    
    bool TraverseTypedefDecl(TypedefDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::TypedefDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseTypedefDecl(declaration);
        ret = Breakpoint(Ancestry::TypedefDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,11>{Ancestry::TranslationUnitDecl,
                                                           Ancestry::LinkageSpecDecl,
                                                           Ancestry::NamespaceDecl,
                                                           Ancestry::FunctionDecl,  // indirect
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::ClassTemplateSpecializationDecl,
                                                           Ancestry::VarDecl,  // indirect
                                                           Ancestry::CXXMethodDecl,  // indirect
                                                           Ancestry::CXXRecordDecl,
                                                           Ancestry::CXXConversionDecl,  // indirect
                                                           Ancestry::CXXConstructorDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitTypedefDecl(const TypedefDecl *declaration)
    {
        assertm(Ancestry::TypedefDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseFieldDecl(FieldDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::FieldDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseFieldDecl(declaration);
        ret = Breakpoint(Ancestry::FieldDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,4>{Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::RecordDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitFieldDecl(const FieldDecl *declaration)
    {
        assertm(Ancestry::FieldDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseCXXConstructorDecl(CXXConstructorDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXConstructorDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXConstructorDecl(declaration);
        ret = Breakpoint(Ancestry::CXXConstructorDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,6>{Ancestry::TranslationUnitDecl,
                                                          Ancestry::NamespaceDecl,
                                                          Ancestry::FunctionTemplateDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitCXXConstructorDecl(const CXXConstructorDecl *declaration)
    {
        assertm(Ancestry::CXXConstructorDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseCXXDestructorDecl(CXXDestructorDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXDestructorDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXDestructorDecl(declaration);
        ret = Breakpoint(Ancestry::CXXDestructorDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,5>{Ancestry::NamespaceDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitCXXDestructorDecl(const CXXDestructorDecl *declaration)
    {
        assertm(Ancestry::CXXDestructorDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseCXXMethodDecl(CXXMethodDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXMethodDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXMethodDecl(declaration);
        ret = Breakpoint(Ancestry::CXXMethodDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,7>{Ancestry::NamespaceDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::FunctionTemplateDecl,
                                                          Ancestry::FriendDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitCXXMethodDecl(const CXXMethodDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,4>{Ancestry::CXXMethodDecl,
                                                          Ancestry::CXXConstructorDecl,
                                                          Ancestry::CXXDestructorDecl,
                                                          Ancestry::CXXConversionDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseTranslationUnitDecl(TranslationUnitDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::TranslationUnitDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseTranslationUnitDecl(declaration);
        ret = Breakpoint(Ancestry::CXXMethodDecl, ret);

        assertm(!parent.kind_, ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitTranslationUnitDecl(const TranslationUnitDecl *declaration)
    {
        assertm(Ancestry::TranslationUnitDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseNamespaceDecl(NamespaceDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::NamespaceDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseNamespaceDecl(declaration);
        ret = Breakpoint(Ancestry::NamespaceDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::TranslationUnitDecl,
                                                          Ancestry::LinkageSpecDecl,
                                                          Ancestry::NamespaceDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitNamespaceDecl(const NamespaceDecl *declaration)
    {
        assertm(Ancestry::NamespaceDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::ClassTemplatePartialSpecializationDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseClassTemplatePartialSpecializationDecl(declaration);
        ret = Breakpoint(Ancestry::ClassTemplatePartialSpecializationDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,5>{Ancestry::NamespaceDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitClassTemplatePartialSpecializationDecl(const ClassTemplatePartialSpecializationDecl *declaration)
    {
        assertm(Ancestry::ClassTemplatePartialSpecializationDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::ClassTemplateSpecializationDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseClassTemplateSpecializationDecl(declaration);
        ret = Breakpoint(Ancestry::ClassTemplateSpecializationDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::NamespaceDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::LinkageSpecDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitClassTemplateSpecializationDecl(const ClassTemplateSpecializationDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,2>{Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseTypeAliasDecl(TypeAliasDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::TypeAliasDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseTypeAliasDecl(declaration);
        ret = Breakpoint(Ancestry::TypeAliasDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,11>{Ancestry::ClassTemplateSpecializationDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::TypeAliasTemplateDecl,
                                                           Ancestry::NamespaceDecl,
                                                           Ancestry::FunctionDecl,
                                                           Ancestry::TranslationUnitDecl,
                                                           Ancestry::LinkageSpecDecl,
                                                           Ancestry::VarDecl,  // indirect
                                                           Ancestry::CXXRecordDecl,
                                                           Ancestry::CXXConstructorDecl,
                                                           Ancestry::CXXMethodDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitTypeAliasDecl(const TypeAliasDecl *declaration)
    {
        assertm(Ancestry::TypeAliasDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseTypeAliasTemplateDecl(TypeAliasTemplateDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::TypeAliasTemplateDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseTypeAliasTemplateDecl(declaration);
        ret = Breakpoint(Ancestry::TypeAliasTemplateDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,4>{Ancestry::NamespaceDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitTypeAliasTemplateDecl(const TypeAliasTemplateDecl *declaration)
    {
        assertm(Ancestry::TypeAliasTemplateDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        std::string token = declaration->getQualifiedNameAsString();
        if(!token.size()) token = NULLSTR;
        ancestry_.back().token_ = token;

        return true;
    }

    bool TraverseLinkageSpecDecl(LinkageSpecDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::LinkageSpecDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseLinkageSpecDecl(declaration);
        ret = Breakpoint(Ancestry::LinkageSpecDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::TranslationUnitDecl,
                                                          Ancestry::LinkageSpecDecl,
                                                          Ancestry::NamespaceDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitLinkageSpecDecl(const LinkageSpecDecl *declaration)
    {
        assertm(Ancestry::LinkageSpecDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseFriendDecl(FriendDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::FriendDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseFriendDecl(declaration);
        ret = Breakpoint(Ancestry::FriendDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitFriendDecl(const FriendDecl *declaration)
    {
        assertm(Ancestry::FriendDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseClassTemplateDecl(ClassTemplateDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::ClassTemplateDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseClassTemplateDecl(declaration);
        ret = Breakpoint(Ancestry::ClassTemplateDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,7>{Ancestry::NamespaceDecl,
                                                          Ancestry::FriendDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::LinkageSpecDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitClassTemplateDecl(const ClassTemplateDecl *declaration)
    {
        assertm(Ancestry::ClassTemplateDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseCXXConversionDecl(CXXConversionDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXConversionDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXConversionDecl(declaration);
        ret = Breakpoint(Ancestry::CXXConversionDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,5>{Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::FunctionTemplateDecl,
                                                          Ancestry::NamespaceDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitCXXConversionDecl(const CXXConversionDecl *declaration)
    {
        assertm(Ancestry::CXXConversionDecl == ancestry_.back().kind_, ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseVarDecl(VarDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::VarDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseVarDecl(declaration);
        ret = Breakpoint(Ancestry::VarDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,15>{Ancestry::TranslationUnitDecl,
                                                           Ancestry::LinkageSpecDecl,
                                                           Ancestry::FunctionDecl,
                                                           Ancestry::NamespaceDecl,
                                                           Ancestry::VarDecl,
                                                           Ancestry::ClassTemplateSpecializationDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::DecompositionDecl,
                                                           Ancestry::VarTemplateDecl,
                                                           Ancestry::FieldDecl,  // indirect
                                                           Ancestry::CXXDestructorDecl,
                                                           Ancestry::CXXConversionDecl,
                                                           Ancestry::CXXRecordDecl,
                                                           Ancestry::CXXConstructorDecl,
                                                           Ancestry::CXXMethodDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitVarDecl(const VarDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,5>{Ancestry::VarDecl,
                                                          Ancestry::ParmVarDecl,
                                                          Ancestry::DecompositionDecl,
                                                          Ancestry::VarTemplateSpecializationDecl,  // indirect
                                                          Ancestry::VarTemplatePartialSpecializationDecl},  // indirect
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseDecompositionDecl(DecompositionDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::DecompositionDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseDecompositionDecl(declaration);
        ret = Breakpoint(Ancestry::DecompositionDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,5>{Ancestry::FunctionDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::VarDecl,
                                                          Ancestry::CXXConstructorDecl,  // indirect
                                                          Ancestry::CXXMethodDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitDecompositionDecl(const DecompositionDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::DecompositionDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseCXXDeductionGuideDecl(CXXDeductionGuideDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::CXXDeductionGuideDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseCXXDeductionGuideDecl(declaration);
        ret = Breakpoint(Ancestry::CXXDeductionGuideDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::FunctionTemplateDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitCXXDeductionGuideDecl(const CXXDeductionGuideDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::CXXDeductionGuideDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseFunctionTemplateDecl(FunctionTemplateDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::FunctionTemplateDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseFunctionTemplateDecl(declaration);
        ret = Breakpoint(Ancestry::FunctionTemplateDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,7>{Ancestry::NamespaceDecl,
                                                          Ancestry::FriendDecl,
                                                          Ancestry::ClassTemplateSpecializationDecl,
                                                          Ancestry::ClassTemplatePartialSpecializationDecl,
                                                          Ancestry::TranslationUnitDecl,
                                                          Ancestry::LinkageSpecDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitFunctionTemplateDecl(const FunctionTemplateDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::FunctionTemplateDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseTemplateTypeParmDecl(TemplateTypeParmDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::TemplateTypeParmDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseTemplateTypeParmDecl(declaration);
        ret = Breakpoint(Ancestry::TemplateTypeParmDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,12>{Ancestry::FunctionTemplateDecl,
                                                           Ancestry::ClassTemplateDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::TypeAliasTemplateDecl,  // indirect
                                                           Ancestry::VarTemplateDecl,
                                                           Ancestry::VarDecl,
                                                           Ancestry::VarTemplatePartialSpecializationDecl,
                                                           Ancestry::CXXConversionDecl,  // indirect
                                                           Ancestry::CXXDestructorDecl,  // indirect
                                                           Ancestry::CXXMethodDecl,  // indirect
                                                           Ancestry::CXXConstructorDecl,  // indirect
                                                           Ancestry::CXXRecordDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitTemplateTypeParmDecl(const TemplateTypeParmDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::TemplateTypeParmDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseVarTemplateDecl(VarTemplateDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::VarTemplateDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseVarTemplateDecl(declaration);
        ret = Breakpoint(Ancestry::VarTemplateDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,3>{Ancestry::TranslationUnitDecl,
                                                          Ancestry::NamespaceDecl,  // indirect
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitVarTemplateDecl(const VarTemplateDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::VarTemplateDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseNonTypeTemplateParmDecl(NonTypeTemplateParmDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::NonTypeTemplateParmDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseNonTypeTemplateParmDecl(declaration);
        ret = Breakpoint(Ancestry::NonTypeTemplateParmDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,10>{Ancestry::ClassTemplateDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,
                                                           Ancestry::TypeAliasTemplateDecl,
                                                           Ancestry::FunctionTemplateDecl,
                                                           Ancestry::VarTemplateDecl,
                                                           Ancestry::VarDecl,  // indirect
                                                           Ancestry::VarTemplatePartialSpecializationDecl,
                                                           Ancestry::CXXConstructorDecl,  // indirect
                                                           Ancestry::CXXMethodDecl,  // indirect
                                                           Ancestry::CXXDestructorDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitNonTypeTemplateParmDecl(const NonTypeTemplateParmDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::NonTypeTemplateParmDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseStaticAssertDecl(StaticAssertDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::StaticAssertDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseStaticAssertDecl(declaration);
        ret = Breakpoint(Ancestry::StaticAssertDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,11>{Ancestry::NamespaceDecl,
                                                           Ancestry::TranslationUnitDecl,
                                                           Ancestry::ClassTemplatePartialSpecializationDecl,  // indirect
                                                           Ancestry::ClassTemplateSpecializationDecl,
                                                           Ancestry::VarDecl,  // indirect
                                                           Ancestry::FunctionDecl,  // indirect
                                                           Ancestry::CXXConversionDecl,  // indirect
                                                           Ancestry::CXXDestructorDecl,  // indirect
                                                           Ancestry::CXXConstructorDecl,  // indirect
                                                           Ancestry::CXXMethodDecl,  // indirect
                                                           Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitStaticAssertDecl(const StaticAssertDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::StaticAssertDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseVarTemplatePartialSpecializationDecl(VarTemplatePartialSpecializationDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::VarTemplatePartialSpecializationDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseVarTemplatePartialSpecializationDecl(declaration);
        ret = Breakpoint(Ancestry::VarTemplatePartialSpecializationDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,2>{Ancestry::NamespaceDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitVarTemplatePartialSpecializationDecl(const VarTemplatePartialSpecializationDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,1>{Ancestry::VarTemplatePartialSpecializationDecl},
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

    bool TraverseVarTemplateSpecializationDecl(VarTemplateSpecializationDecl *declaration)
    {
        auto parent = !ancestry_.empty() ? ancestry_.back() : Ancestry();

        ancestry_.push_back({Ancestry::VarTemplateSpecializationDecl});
        ancestry_.back().traversed_ = true;
        bool ret = RecursiveASTVisitor::TraverseVarTemplateSpecializationDecl(declaration);
        ret = Breakpoint(Ancestry::VarTemplateSpecializationDecl, ret);

        assertm(extensions::iter::in(std::array<size_t,2>{Ancestry::NamespaceDecl,
                                                          Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitVarTemplateSpecializationDecl(const VarTemplateSpecializationDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<size_t,2>{Ancestry::VarTemplateSpecializationDecl,
                                                          Ancestry::VarTemplatePartialSpecializationDecl},  // indirect
                                     ancestry_.back().kind_), ancestry_);
        ancestry_.back().visited_ = true;

        auto [location,lineno] = LocationOf(declaration);
        ancestry_.back().location_ = location;
        ancestry_.back().lineno_ = lineno;

        ancestry_.back().token_ = NULLSTR;

        return true;
    }

public:
    explicit FindNamedClassVisitor(ASTContext *Context, extensions::graphdb::Environment const& env)
    : Context(Context)
    , env_{env}
    , ancestry_{}
    {}

private:
    /**
     * LLVM seems to set some nodes to "(anonymous)", but there are cases such as
     * void(*signal(int, void (*)(int)))(int);
     * 
     * which result in a nested ParmVarDecl, the first one having an empty string,
     * while the second is set to "(anonymous)". In such cases we set the token
     * to NULLSTR.
     */
    static constexpr std::string_view NULLSTR = "(null)";

private:
    extensions::graphdb::schema::vertex_feature_key_t IsTokenInTheDB(
        extensions::graphdb::schema::TransactionNode const& txn,
        std::string_view token,
        std::string_view location)
    {
        // Keep a history of <token,location> pairs to confirm that when a pair is in the history
        // the it should have been found by the logic above.
        using record_t = std::pair<std::string, std::string>;
        struct hasher
        {
            std::size_t operator()(record_t record) const
            {
                auto a = std::hash<typename record_t::first_type>{}(std::get<0>(record));
                auto b = std::hash<typename record_t::second_type>{}(std::get<1>(record));
                return a ^ b;
            }
        };
        static std::unordered_set<record_t, hasher> history;
        bool should_have_been_found = 0 != history.count(record_t{token, location});
        // if(!should_have_been_found) history.insert(record_t{token, location});  // uncomment to debug with in-memory history

        /**
         * The same token might appear under different files. In that case we want to
         * apply ref counting over the token, but create a new vertex for the new
         * location.
         */
        using key_l = extensions::graphdb::schema::list_key_t;
        using key_f = extensions::graphdb::schema::vertex_feature_key_t;
        key_f key;

        extensions::graphdb::hash::visitor_t<key_f> visitor =
        [&](key_l h4sh, key_f k3y)
        {
            int ret;
            ret = extensions::graphdb::feature::visit(txn.vertex_, k3y,
                        [&](key_l iter, size_t size, size_t hAsh) -> int
                        {
                            bool present = extensions::graphdb::list::holds(txn.vertex_.list_, iter, token, hAsh);
                            return  present ? MDB_SUCCESS : MDB_NOTFOUND;
                        });
            if(MDB_SUCCESS == ret)
            {
                k3y.attribute() = extensions::rsse::schema::feature::FILE;
                ret = extensions::graphdb::feature::visit(txn.vertex_, k3y,
                            [&](key_l iter, size_t size, size_t hAsh) -> int
                            {
                                bool present = extensions::graphdb::list::holds(txn.vertex_.list_, iter, location, hAsh);
                                return  present ? MDB_SUCCESS : MDB_NOTFOUND;
                            });
            }
            if(MDB_SUCCESS == ret)
            {
                key = k3y;
                key.attribute() = extensions::rsse::schema::feature::NAME;
            }
            return ret;
        };
        extensions::graphdb::schema::list_key_t hash = {extensions::graphdb::hash::make(token), 0};
        extensions::graphdb::hash::visit(txn.vertex_.hash_, hash, visitor);
        assertm(!should_have_been_found || key.attribute(), token, hash, location);  // no key in the DB has attribute = 0
        return key;
    }

    template <typename T>
    std::pair<std::string_view, size_t> LocationOf(T *declaration)
    {
        // llvm::StringRef does not define a value_type, nor an operator<<.
        std::string_view file;
        const SourceManager &SourceMgr = Context->getSourceManager();
        FullSourceLoc FullLocation = Context->getFullLoc(declaration->getBeginLoc());
        if(FullLocation.isValid())
            file = {SourceMgr.getFilename(FullLocation).data(), SourceMgr.getFilename(FullLocation).size()};
        // for some symbols the location is a null string
        if(!file.size()) file = "sys_";
        size_t lineno = FullLocation.getLineNumber();
        return {file,lineno};
    }

    template <typename T>
    std::string Unravel(T *node)
    {
        std::string ret = "";
        auto parents = Context->getParents(*node);
        for(auto parent : parents)
        {
            ret += Trace(&parent);
            if(ret.size()) ret += ",";
            ret += parent.getNodeKind().asStringRef();
        }
        return ret;
    }

    bool Breakpoint(size_t kind, bool forward_to_AST_visitor)
    {
        const std::string_view location = "";
        const size_t lineno = 0;
        if(extensions::ends_with(ancestry_.back().location_, location) && ancestry_.back().lineno_ == lineno)
            std::cerr << Ancestry::map[kind] << ":" << ancestry_ << std::endl;
        return forward_to_AST_visitor;
    }

private:
    struct Ancestry
    {
        static constexpr size_t FunctionDecl = 1000;
        static constexpr size_t ParmVarDecl = 1001;
        static constexpr size_t VarDecl = 1002;
        static constexpr size_t RecordDecl = 1003;
        static constexpr size_t TypedefDecl = 1004;
        static constexpr size_t FieldDecl = 1005;
        static constexpr size_t FunctionTemplateDecl = 1006;
        static constexpr size_t TranslationUnitDecl = 1007;
        static constexpr size_t NamespaceDecl = 1008;
        static constexpr size_t ClassTemplateDecl = 1009;
        static constexpr size_t ClassTemplateSpecializationDecl = 1010;
        static constexpr size_t ClassTemplatePartialSpecializationDecl = 1011;
        static constexpr size_t TypeAliasDecl = 1012;
        static constexpr size_t TypeAliasTemplateDecl = 1013;
        static constexpr size_t LinkageSpecDecl = 1014;
        static constexpr size_t FriendDecl = 1015;
        static constexpr size_t DecompositionDecl = 1016;
        static constexpr size_t TemplateTypeParmDecl = 1017;
        static constexpr size_t VarTemplateDecl = 1018;
        static constexpr size_t NonTypeTemplateParmDecl = 1019;
        static constexpr size_t StaticAssertDecl = 1020;
        static constexpr size_t VarTemplatePartialSpecializationDecl = 1021;
        static constexpr size_t VarTemplateSpecializationDecl = 1022;
        static constexpr size_t CXXRecordDecl = 2000;
        static constexpr size_t CXXConstructorDecl = 2001;
        static constexpr size_t CXXDestructorDecl = 2002;
        static constexpr size_t CXXMethodDecl = 2003;
        static constexpr size_t CXXConversionDecl = 2004;
        static constexpr size_t CXXDeductionGuideDecl = 2005;

        static std::unordered_map<size_t, std::string_view> map;

        Ancestry(size_t kind)
        : vertex_{}
        , traversed_{}
        , visited_{}
        , kind_{kind}
        , token_{}
        , location_{}
        , lineno_{}
        {}

        Ancestry() = default;

        size_t vertex_;  // the vertex index as it was captured in the DB
        bool traversed_;  // true when the node is traversed
        bool visited_;  // true when the node is visited
        size_t kind_;  // captures the type of the AST node; When a Visit* method is reached for
                       // which the Traverse* is not defined, the Trace() will not help!
        std::string token_;
        std::string location_;
        size_t lineno_;

        friend std::ostream& operator<<(std::ostream& strm, Ancestry const& other);
    };

    using ancestry_set_t = std::list<Ancestry>;

    std::optional<typename ancestry_set_t::const_reverse_iterator> second_back()
    {
        auto iter_b = ancestry_.crbegin();
        auto iter_e = ancestry_.crend();
        auto iter_n = iter_b != iter_e ? std::next(iter_b) : iter_e;
        return iter_b != iter_e ? std::optional(iter_n) : std::nullopt;
    }

private:
    ASTContext *Context;
    extensions::graphdb::Environment const& env_;
    ancestry_set_t ancestry_;

    friend std::ostream& operator<<(std::ostream& strm, Ancestry const& other);
    friend std::ostream& operator<<(std::ostream& strm, std::list<FindNamedClassVisitor::Ancestry> const& other);
};

std::unordered_map<size_t, std::string_view> FindNamedClassVisitor::Ancestry::map = 
{
    {FunctionDecl, "FunctionDecl"},
    {ParmVarDecl, "ParmVarDecl"},
    {VarDecl, "VarDecl"},
    {RecordDecl, "RecordDecl"},
    {TypedefDecl, "TypedefDecl"},
    {FieldDecl, "FieldDecl"},
    {FunctionTemplateDecl, "FunctionTemplateDecl"},
    {TranslationUnitDecl, "TranslationUnitDecl"},
    {NamespaceDecl, "NamespaceDecl"},
    {ClassTemplateDecl, "ClassTemplateDecl"},
    {ClassTemplateSpecializationDecl, "ClassTemplateSpecializationDecl"},
    {ClassTemplatePartialSpecializationDecl, "ClassTemplatePartialSpecializationDecl"},
    {TypeAliasDecl, "TypeAliasDecl"},
    {TypeAliasTemplateDecl, "TypeAliasTemplateDecl"},
    {LinkageSpecDecl, "LinkageSpecDecl"},
    {FriendDecl, "FriendDecl"},
    {DecompositionDecl, "DecompositionDecl"},
    {TemplateTypeParmDecl, "TemplateTypeParmDecl"},
    {VarTemplateDecl, "VarTemplateDecl"},
    {NonTypeTemplateParmDecl, "NonTypeTemplateParmDecl"},
    {StaticAssertDecl, "StaticAssertDecl"},
    {VarTemplatePartialSpecializationDecl, "VarTemplatePartialSpecializationDecl"},
    {VarTemplateSpecializationDecl, "VarTemplateSpecializationDecl"},
    {CXXRecordDecl, "CXXRecordDecl"},
    {CXXConstructorDecl, "CXXConstructorDecl"},
    {CXXDestructorDecl, "CXXDestructorDecl"},
    {CXXMethodDecl, "CXXMethodDecl"},
    {CXXConversionDecl, "CXXConversionDecl"},
    {CXXDeductionGuideDecl, "CXXDeductionGuideDecl"}
};

std::ostream& operator<<(std::ostream& strm, FindNamedClassVisitor::Ancestry const& other)
{
    std::ostringstream out;
    out << other.location_ << ":" << other.lineno_ << ":" << other.token_;
    out << " " << other.vertex_ << "," << other.traversed_ << "," << other.visited_;
    out << " " << FindNamedClassVisitor::Ancestry::map[other.kind_];
    return strm << out.str();
}

std::ostream& operator<<(std::ostream& strm, std::list<FindNamedClassVisitor::Ancestry> const& other)
{
    std::ostringstream out;
    for(auto const& node : other)
        out << node << " ";
    return strm << out.str();
}

class FindNamedClassConsumer : public clang::ASTConsumer
{
public:
    virtual void HandleTranslationUnit(clang::ASTContext &Context)
    {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
public:
    explicit FindNamedClassConsumer(ASTContext *Context, extensions::graphdb::Environment const& env)
    : Visitor(Context, env)
    , env_{env}
    {}

private:
    FindNamedClassVisitor Visitor;
    extensions::graphdb::Environment const& env_;
};

class FindNamedClassAction : public clang::ASTFrontendAction
{
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile)
    {
        return std::make_unique<FindNamedClassConsumer>(&Compiler.getASTContext(), env_);
    }

public:

    explicit FindNamedClassAction(extensions::graphdb::Environment const& env)
    : env_{env}
    {}

    extensions::graphdb::Environment const& env_;
};

template <typename T>
std::unique_ptr<FrontendActionFactory> factory(extensions::graphdb::Environment const& env)
{
    class DBFrontendActionFactory : public FrontendActionFactory
    {
    public:
        std::unique_ptr<FrontendAction> create() override
        {
            return std::make_unique<T>(env_);
        }
    public:
        explicit DBFrontendActionFactory(extensions::graphdb::Environment const& env)
        : env_{env}
        {
            extensions::graphdb::schema::TransactionNode txn(env, extensions::graphdb::flags::txn::WRITE);
            extensions::graphdb::graph::init(txn, extensions::rsse::schema::GRAPH);
        }
        extensions::graphdb::Environment const& env_;
    };

    return std::unique_ptr<FrontendActionFactory>(new DBFrontendActionFactory(env));
}

// After the '--' flags are forwarded to the compiler, for AST -Xclang -ast-dump -fsyntax-only,
// clang -v for default flags.
static llvm::cl::OptionCategory MyToolCategory("llvmast options");

int llvmast(int argc, char const** argv)
{
    extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(extensions::rsse::schema::DB_FILE);

    auto OptionsParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!OptionsParser) {
      llvm::errs() << toString(OptionsParser.takeError());
      return 1;
    }
    
    ClangTool Tool(OptionsParser->getCompilations(),
                   OptionsParser->getSourcePathList());
    Tool.run(factory<FindNamedClassAction>(env).get());

    return 0;
}

static std::unique_ptr<char*[]> argv;

// The IDE doesn't like the PYBIND11_MODULE macro, hence we redirect it
// to this function which is easier to read.
void PYBIND11_MODULE_IMPL(py::module_ m)
{
    m.attr("DB_FILE") = py::cast(extensions::rsse::schema::DB_FILE);
    m.attr("GRAPH") = py::cast(extensions::rsse::schema::GRAPH.graph());

    {
        auto mf = m.def_submodule("feature", "");
        mf.attr("RULE") = py::cast(extensions::rsse::schema::feature::RULE);
        mf.attr("NAME") = py::cast(extensions::rsse::schema::feature::NAME);
        mf.attr("FILE") = py::cast(extensions::rsse::schema::feature::FILE);

        auto my = m.def_submodule("rule", "");
        my.attr("RECORD_DECL")   = py::cast(extensions::rsse::schema::rule::RECORD_DECL);
        my.attr("FUNCTION_DECL") = py::cast(extensions::rsse::schema::rule::FUNCTION_DECL);
        my.attr("PARM_VAR_DECL") = py::cast(extensions::rsse::schema::rule::PARM_VAR_DECL);
    }

    m.def("llvmast",
          +[](std::vector<std::string> pyargv)
          {
            int argc = pyargv.size();
            argv = std::unique_ptr<char*[]>{new char*[argc]};
            for(int i = 0; i < argc; ++i)
                argv[i] = pyargv[i].data();
            llvmast(argc, const_cast<char const**>(argv.get()));
          },
          py::call_guard<py::scoped_ostream_redirect,
                         py::scoped_estream_redirect>());

    {
        /**
         * Test submodule
         */

        auto mt = m.def_submodule("test", "");
    }
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
