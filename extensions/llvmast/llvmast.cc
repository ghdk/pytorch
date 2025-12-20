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

        assertm(extensions::iter::in(std::array<std::string_view,7>{Ancestry::LinkageSpecDecl,
                                                                    Ancestry::TranslationUnitDecl,
                                                                    Ancestry::NamespaceDecl,
                                                                    Ancestry::FriendDecl,
                                                                    Ancestry::FunctionTemplateDecl,
                                                                    Ancestry::CXXMethodDecl,
                                                                    Ancestry::CXXDestructorDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitFunctionDecl(const FunctionDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,6>{Ancestry::FunctionDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,12>{Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                     Ancestry::FieldDecl,
                                                                     Ancestry::TypedefDecl,
                                                                     Ancestry::FunctionDecl,
                                                                     Ancestry::ParmVarDecl,
                                                                     Ancestry::TypeAliasDecl,
                                                                     Ancestry::VarDecl,
                                                                     Ancestry::TemplateTypeParmDecl,
                                                                     Ancestry::NonTypeTemplateParmDecl,
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
        }

        {
            auto parent = second_back();
            if(!parent.has_value()) goto RETURN;
            assertm(parent.value()->traversed_ && parent.value()->visited_, ancestry_);

            extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
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

        assertm(extensions::iter::in(std::array<std::string_view,8>{Ancestry::TranslationUnitDecl,
                                                                    Ancestry::CXXRecordDecl,
                                                                    Ancestry::LinkageSpecDecl,
                                                                    Ancestry::NamespaceDecl,
                                                                    Ancestry::ClassTemplateDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::CXXMethodDecl,
                                                                    Ancestry::FunctionDecl},
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

        assertm(parent.kind_.empty(), ancestry_);

        ancestry_.pop_back();
        return ret;
    }
    bool VisitRecordDecl(const RecordDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,4>{Ancestry::RecordDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,5>{Ancestry::TranslationUnitDecl,
                                                                    Ancestry::LinkageSpecDecl,
                                                                    Ancestry::NamespaceDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::CXXRecordDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,5>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::NamespaceDecl,
                                                                    Ancestry::FunctionTemplateDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,6>{Ancestry::NamespaceDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::FunctionTemplateDecl,
                                                                    Ancestry::FriendDecl,
                                                                    Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitCXXMethodDecl(const CXXMethodDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,4>{Ancestry::CXXMethodDecl,
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

        assertm(parent.kind_.empty(), ancestry_);

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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::TranslationUnitDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,4>{Ancestry::NamespaceDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::NamespaceDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitClassTemplateSpecializationDecl(const ClassTemplateSpecializationDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::ClassTemplateSpecializationDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,9>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::TypeAliasTemplateDecl,
                                                                    Ancestry::NamespaceDecl,
                                                                    Ancestry::FunctionDecl,
                                                                    Ancestry::TranslationUnitDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::NamespaceDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,3>{Ancestry::TranslationUnitDecl,
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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,6>{Ancestry::NamespaceDecl,
                                                                    Ancestry::FriendDecl,
                                                                    Ancestry::CXXRecordDecl,
                                                                    Ancestry::TranslationUnitDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,3>{Ancestry::CXXRecordDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::FunctionTemplateDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,14>{Ancestry::TranslationUnitDecl,
                                                                     Ancestry::LinkageSpecDecl,
                                                                     Ancestry::FunctionDecl,
                                                                     Ancestry::NamespaceDecl,
                                                                     Ancestry::VarDecl,
                                                                     Ancestry::ClassTemplateSpecializationDecl,
                                                                     Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                     Ancestry::DecompositionDecl,
                                                                     Ancestry::VarTemplateDecl,
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
        assertm(extensions::iter::in(std::array<std::string_view,3>{Ancestry::VarDecl,
                                                                    Ancestry::ParmVarDecl,
                                                                    Ancestry::DecompositionDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,2>{Ancestry::FunctionDecl,
                                                                    Ancestry::CXXMethodDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitDecompositionDecl(const DecompositionDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::DecompositionDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::FunctionTemplateDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitCXXDeductionGuideDecl(const CXXDeductionGuideDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::CXXDeductionGuideDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,6>{Ancestry::NamespaceDecl,
                                                                    Ancestry::FriendDecl,
                                                                    Ancestry::ClassTemplateSpecializationDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::TranslationUnitDecl,
                                                                    Ancestry::CXXRecordDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitFunctionTemplateDecl(const FunctionTemplateDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::FunctionTemplateDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,9>{Ancestry::FunctionTemplateDecl,
                                                                    Ancestry::ClassTemplateDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::TypeAliasTemplateDecl,  // indirect
                                                                    Ancestry::VarTemplateDecl,
                                                                    Ancestry::VarDecl,
                                                                    Ancestry::CXXMethodDecl,  // indirect
                                                                    Ancestry::CXXConstructorDecl,  // indirect
                                                                    Ancestry::CXXRecordDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitTemplateTypeParmDecl(const TemplateTypeParmDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::TemplateTypeParmDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::NamespaceDecl},  // indirect
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitVarTemplateDecl(const VarTemplateDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::VarTemplateDecl},
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

        assertm(extensions::iter::in(std::array<std::string_view,7>{Ancestry::ClassTemplateDecl,
                                                                    Ancestry::ClassTemplatePartialSpecializationDecl,
                                                                    Ancestry::CXXConstructorDecl,  // indirect
                                                                    Ancestry::CXXMethodDecl,  // indirect
                                                                    Ancestry::TypeAliasTemplateDecl,
                                                                    Ancestry::FunctionTemplateDecl,
                                                                    Ancestry::VarTemplateDecl},
                                     parent.kind_), ancestry_);

        ancestry_.pop_back();
        return ret;
    }

    bool VisitNonTypeTemplateParmDecl(const NonTypeTemplateParmDecl *declaration)
    {
        assertm(extensions::iter::in(std::array<std::string_view,1>{Ancestry::NonTypeTemplateParmDecl},
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
        /**
         * The same token might appear under different files. In that case we want to
         * apply ref counting over the token, but create a new vertex for the new
         * location.
         */
        using key_l = extensions::graphdb::schema::list_key_t;
        using key_f = extensions::graphdb::schema::vertex_feature_key_t;
        key_f key{
            extensions::graphdb::schema::LIST_TAIL_MAX,
            extensions::graphdb::schema::LIST_TAIL_MAX,
            extensions::graphdb::schema::LIST_TAIL_MAX
        };

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

    bool Breakpoint(std::string_view kind, bool forward_to_AST_visitor)
    {
        const std::string_view location = "";
        const size_t lineno = 0;
        if(extensions::ends_with(ancestry_.back().location_, location) && ancestry_.back().lineno_ == lineno)
            std::cerr << kind << ":" << ancestry_ << std::endl;
        return forward_to_AST_visitor;
    }

private:
    struct Ancestry
    {
        static constexpr std::string_view FunctionDecl = "FunctionDecl";
        static constexpr std::string_view ParmVarDecl = "ParmVarDecl";
        static constexpr std::string_view VarDecl = "VarDecl";
        static constexpr std::string_view RecordDecl = "RecordDecl";
        static constexpr std::string_view TypedefDecl = "TypedefDecl";
        static constexpr std::string_view FieldDecl = "FieldDecl";
        static constexpr std::string_view FunctionTemplateDecl = "FunctionTemplateDecl";
        static constexpr std::string_view TranslationUnitDecl = "TranslationUnitDecl";
        static constexpr std::string_view NamespaceDecl = "NamespaceDecl";
        static constexpr std::string_view ClassTemplateDecl = "ClassTemplateDecl";
        static constexpr std::string_view ClassTemplateSpecializationDecl = "ClassTemplateSpecializationDecl";
        static constexpr std::string_view ClassTemplatePartialSpecializationDecl = "ClassTemplatePartialSpecializationDecl";
        static constexpr std::string_view TypeAliasDecl = "TypeAliasDecl";
        static constexpr std::string_view TypeAliasTemplateDecl = "TypeAliasTemplateDecl";
        static constexpr std::string_view LinkageSpecDecl = "LinkageSpecDecl";
        static constexpr std::string_view FriendDecl = "FriendDecl";
        static constexpr std::string_view DecompositionDecl = "DecompositionDecl";
        static constexpr std::string_view TemplateTypeParmDecl = "TemplateTypeParmDecl";
        static constexpr std::string_view VarTemplateDecl = "VarTemplateDecl";
        static constexpr std::string_view NonTypeTemplateParmDecl = "NonTypeTemplateParmDecl";
        static constexpr std::string_view CXXRecordDecl = "CXXRecordDecl";
        static constexpr std::string_view CXXConstructorDecl = "CXXConstructorDecl";
        static constexpr std::string_view CXXDestructorDecl = "CXXDestructorDecl";
        static constexpr std::string_view CXXMethodDecl = "CXXMethodDecl";
        static constexpr std::string_view CXXConversionDecl = "CXXConversionDecl";
        static constexpr std::string_view CXXDeductionGuideDecl = "CXXDeductionGuideDecl";

        Ancestry(std::string_view kind)
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
        std::string_view kind_;  // captures the type of the AST node; When a Visit* method is reached for
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

std::ostream& operator<<(std::ostream& strm, FindNamedClassVisitor::Ancestry const& other)
{
    std::ostringstream out;
    out << other.location_ << ":" << other.lineno_ << ":" << other.token_;
    out << " " << other.vertex_ << "," << other.traversed_ << "," << other.visited_;
    out << " " << other.kind_;
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
