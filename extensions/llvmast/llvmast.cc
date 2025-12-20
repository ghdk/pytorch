#include "../system.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
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

class FindNamedClassVisitor : public RecursiveASTVisitor<FindNamedClassVisitor>
{
public:
    bool VisitFunctionDecl(const FunctionDecl *declaration)
    {
        std::string token = declaration->getQualifiedNameAsString();

        if(token.size())
        {
            {
                extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
                extensions::graphdb::graph::init(txn, extensions::rsse::schema::GRAPH);
            }
            extensions::graphdb::schema::list_key_t::value_type vtx = 0;
            extensions::graphdb::schema::vertex_feature_key_t key = {extensions::rsse::schema::GRAPH.graph(),
                                                                     vtx,
                                                                     extensions::rsse::schema::feature::NAME};
            {
                extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::READ);
                // it doesn't matter that the key is 0 for now, we just want to see if the value is unique.
                auto [_, unique] = extensions::graphdb::feature::write(txn.vertex_, key, token, true);
                if(!unique) return true;
            }

            {
                extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
                vtx = extensions::graphdb::graph::is_available(txn, extensions::rsse::schema::GRAPH, vtx);
                key.vertex() = vtx;

                extensions::graphdb::feature::write(txn.vertex_, key, token, true);

                key.attribute() = extensions::rsse::schema::feature::TYPE;
                extensions::graphdb::feature::write(txn.vertex_, key, extensions::rsse::schema::type::FUNCTION_DECL, false);

                extensions::graphdb::graph::vertex(txn, extensions::rsse::schema::GRAPH, vtx, true);
            }
        }
        return true;
    }

    bool VisitRecordDecl(const RecordDecl *declaration)
    {
        const SourceManager &SourceMgr = Context->getSourceManager();
        FullSourceLoc FullLocation = Context->getFullLoc(declaration->getBeginLoc());
        if (FullLocation.isValid())
        {
            // llvm::StringRef does not define a value_type, nor an operator<<.
            std::string_view token(declaration->getName().data(), declaration->getName().size());
            std::string_view loc(SourceMgr.getFilename(FullLocation).data(), SourceMgr.getFilename(FullLocation).size());

            if(token.size())
            {
                {
                    extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
                    extensions::graphdb::graph::init(txn, extensions::rsse::schema::GRAPH);
                }
                extensions::graphdb::schema::list_key_t::value_type vtx = 0;
                extensions::graphdb::schema::vertex_feature_key_t key = {extensions::rsse::schema::GRAPH.graph(),
                                                                         vtx,
                                                                         extensions::rsse::schema::feature::NAME};
                {
                    extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::READ);
                    auto [_, unique] = extensions::graphdb::feature::write(txn.vertex_, key, token, true);
                    if(!unique) return true;
                }
                {
                    extensions::graphdb::schema::TransactionNode txn(env_, extensions::graphdb::flags::txn::WRITE);
                    vtx = extensions::graphdb::graph::is_available(txn, extensions::rsse::schema::GRAPH, vtx);
                    key.vertex() = vtx;
                    
                    extensions::graphdb::feature::write(txn.vertex_, key, token, true);

                    key.attribute() = extensions::rsse::schema::feature::TYPE;
                    extensions::graphdb::feature::write(txn.vertex_, key, extensions::rsse::schema::type::RECORD_DECL, false);

                    extensions::graphdb::graph::vertex(txn, extensions::rsse::schema::GRAPH, vtx, true);
                }
            }
        }

        return true;
    }

    bool VisitCXXMethodDecl(const CXXMethodDecl *declaration)
    {
        /*
         * VisitFunctionDecl seems to be a superset of this.
         * Leaving this here for the time being just for reference.
         */
        return true;
    }

    bool VisitCXXRecordDecl(const CXXRecordDecl *declaration)
    {
        /*
         * VisitRecordDecl seems to be a superset of this.
         * Leaving this here for the time being just for reference.
         */
        return true;
    }

public:

    explicit FindNamedClassVisitor(ASTContext *Context, extensions::graphdb::Environment const& env)
    : Context(Context)
    , env_{env}
    {}

private:
    ASTContext *Context;
    extensions::graphdb::Environment const& env_;
};

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
        {}
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

        mf.attr("TYPE") = py::cast(extensions::rsse::schema::feature::TYPE);
        mf.attr("NAME") = py::cast(extensions::rsse::schema::feature::NAME);

        auto my = m.def_submodule("type", "");
        my.attr("FILE")          = py::cast(extensions::rsse::schema::type::FILE);
        my.attr("RECORD_DECL")   = py::cast(extensions::rsse::schema::type::RECORD_DECL);
        my.attr("FUNCTION_DECL") = py::cast(extensions::rsse::schema::type::FUNCTION_DECL);
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
