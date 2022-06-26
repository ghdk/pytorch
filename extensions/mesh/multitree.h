#ifndef EXTENSIONS_MESH_MULTITREE_H
#define EXTENSIONS_MESH_MULTITREE_H

namespace extensions { namespace mesh { class Mesh; }}
namespace extensions { namespace mesh { class Tree; }}

namespace extensions { namespace mesh
{
    class MultiTree
        : public Tree
    {
    public:  // typedefs
        using multitree_ptr = ptr_t<MultiTree>;
    public:  // virtual methods
        virtual mesh_ptr p(uint8_t dimension, mesh_ptr p) override
        {
            switch(dimension)
            {
            case dim::y: return child(p);
            default: return Tree::p(dimension, p);
            }
        }
        virtual mesh_ptr p(uint8_t dimension) override
        {
            return Tree::p(dimension);
        }
        virtual mesh_ptr n(uint8_t dimension, mesh_ptr p) override
        {
            return Tree::n(dimension, p);
        }
        virtual mesh_ptr n(uint8_t dimension) override
        {
            switch(dimension)
            {
            case dim::x: return image();
            default: return Tree::n(dimension);
            }
        }
        
    public:  // copy/move semantics
        MultiTree()
            : Tree{}
        {}
        MultiTree(MultiTree const& other) = delete;
        MultiTree(MultiTree&& other) = delete;
        MultiTree& operator=(MultiTree const& other) = delete;
        MultiTree& operator=(MultiTree&& other) = delete;
    protected:  // tree accessors        
        mesh_ptr child(mesh_ptr p)
        {
            return Tree::child(ref(p));
        }

        mesh_ptr image(mesh_ptr p)
        {
            mesh_ptr ret = nullptr;
            if(p && Mesh::of<MultiTree>(image()))
                ret = Mesh::of<MultiTree>(image())->image(p);
            else
                ret = Mesh::n(dim::x, p);
            return ret;
        }

        mesh_ptr image()
        {
            return Mesh::n(dim::x);
        }
        
        mesh_ptr deref()
        {
            multitree_ptr img = Mesh::of<MultiTree>(shared_from_this());
            if(img)
            {
                multitree_ptr ref = Mesh::of<MultiTree>(img->Tree::child());
                if(ref
                   && ref->parent() != img
                   && ref->image())
                    return ref;
            }
            return img;
        }

        mesh_ptr ref(mesh_ptr p)
        {
            mesh_ptr tgt = p;
            if(tgt && Mesh::of<MultiTree>(tgt) && Mesh::of<MultiTree>(tgt)->parent())
            {
                // the target node has a parent, hence belongs to a branch,
                // we need to create an image instead
                multitree_ptr img = std::make_shared<MultiTree>();
                img->Mesh::p(dim::y, tgt);
                Mesh::of<MultiTree>(tgt)->image(img);
                tgt = img;
            }
            return tgt;
        }

#ifdef PYDEF
    public:
        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;
            c.def("deref", &T::deref);
            return c;
        }
#endif  // PYDEF
    };

}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_MULTITREE_H
