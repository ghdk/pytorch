#ifndef EXTENSIONS_MESH_CHILDREN_TRAVERSAL_H
#define EXTENSIONS_MESH_CHILDREN_TRAVERSAL_H

namespace extensions { namespace mesh { class Mesh; }}
namespace extensions { namespace mesh { class Tree; }}
namespace extensions { namespace mesh { class MultiTree; }}
namespace extensions { namespace mesh { class PropertyTree; }}
namespace extensions { namespace iter { template<typename ValueType> class GeneratedIterator; }}
    
namespace extensions { namespace mesh
{

class ChildrenTraversal
{
private:  // typedefs
    using ret_t = GeneratedIterator<Mesh::mesh_ptr>::generator_t::result_type;
public:  // methods
    ret_t operator()(void)
    {
        if(mesh_ == state_)
            state_ = state_->p(Mesh::dim::y);
        else if(nullptr != state_ && mesh_ == state_->n(Mesh::dim::y))
            state_ = state_->p(Mesh::dim::x);
        return (nullptr != state_? ret_t{state_} : std::nullopt);
    }

public:  // copy/move semantics
    explicit ChildrenTraversal(Mesh::mesh_ptr mesh)
        : mesh_{mesh}, state_{mesh}
    {
        assertm(nullptr != mesh);
    }
    ChildrenTraversal(ChildrenTraversal const& other) = delete;
    ChildrenTraversal(ChildrenTraversal&& other) = delete;
    ChildrenTraversal& operator=(ChildrenTraversal const& other) = delete;
    ChildrenTraversal& operator=(ChildrenTraversal&& other) = delete;
private:  // members
    Mesh::mesh_ptr mesh_;
    Mesh::mesh_ptr state_;

#ifdef PYDEF
public:  // python
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<Mesh::mesh_ptr>());
        c.def("__call__", &T::operator());
        return c;
    }
#endif  // PYDEF
};

}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_CHILDREN_TRAVERSAL_H
