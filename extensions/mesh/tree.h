#ifndef EXTENSIONS_MESH_TREE_H
#define EXTENSIONS_MESH_TREE_H

namespace extensions { namespace mesh { class Mesh; }}

namespace extensions { namespace mesh
{
    class Tree
        : public Mesh
    {
    public:  // virtual methods
        virtual mesh_ptr p(uint8_t dimension, mesh_ptr p) override
        {
            switch(dimension)
            {
            case dim::y: return child(p);
            default: assertm(false, "Cannot handle dimension!");
            }
        }
        virtual mesh_ptr p(uint8_t dimension) override
        {
            switch(dimension)
            {
            case dim::x: return sibling();
            case dim::y: return child();
            default: assertm(false, "Cannot handle dimension!");
            }
        }
        virtual mesh_ptr n(uint8_t dimension, mesh_ptr p) override
        {
            assertm(!dimension && (!p^!p), "Cannot handle dimension!");
            return nullptr;
        }
        virtual mesh_ptr n(uint8_t dimension) override
        {
            switch(dimension)
            {
            case dim::y: return parent();
            default: assertm(false, "Cannot handle dimension!");
            }
        }

    public:  // copy/move semantics
        Tree()
            : Mesh{}
        {}
        Tree(Tree const& other) = delete;
        Tree(Tree&& other) = delete;
        Tree& operator=(Tree const& other) = delete;
        Tree& operator=(Tree&& other) = delete;
    protected:  // tree accessors
        mesh_ptr parent(mesh_ptr p)
        {
            return Mesh::n(dim::y, p);
        }
        
        mesh_ptr parent()
        {
            return Mesh::n(dim::y);
        }
        
        mesh_ptr child(mesh_ptr p)
        {
            mesh_ptr ret = nullptr;
            if(p && Mesh::of<Tree>(child()))
                ret = Mesh::of<Tree>(child())->sibling(p);
            else
            {
                ret = Mesh::p(dim::y, p);
                if(Mesh::of<Tree>(p))
                    Mesh::of<Tree>(p)->parent(shared_from_this());
            }
            return ret;
        }

        mesh_ptr child()
        {
            return Mesh::p(dim::y);
        }

        mesh_ptr sibling(mesh_ptr p)
        {
            mesh_ptr ret = nullptr;
            if(p && Mesh::of<Tree>(sibling()))
                ret = Mesh::of<Tree>(sibling())->sibling(p);
            else
            {
                ret = Mesh::p(dim::x, p);
                if(Mesh::of<Tree>(p) && parent())
                    Mesh::of<Tree>(p)->parent(parent());
            }
            return ret;
        }

        mesh_ptr sibling()
        {
            return Mesh::p(dim::x);
        }
    };

}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_TREE_H
