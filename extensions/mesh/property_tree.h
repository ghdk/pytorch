#ifndef EXTENSIONS_MESH_PROPERTY_TREE_H
#define EXTENSIONS_MESH_PROPERTY_TREE_H

namespace extensions { namespace mesh { class Mesh; }}
namespace extensions { namespace mesh { class Tree; }}
namespace extensions { namespace mesh { class MultiTree; }}

namespace extensions { namespace mesh
{
    class PropertyTree
        : public MultiTree
    {
    public:  // typedefs
        using property_tree_ptr = ptr_t<PropertyTree>;
    public:  // virtual methods
        virtual mesh_ptr p(uint8_t dimension, mesh_ptr p) override
        {
            switch(dimension)
            {
            case dim::z: return property(p);
            default: return MultiTree::p(dimension, p);
            }
        }
        virtual mesh_ptr p(uint8_t dimension) override
        {
            switch(dimension)
            {
            case dim::z: return property();
            default: return MultiTree::p(dimension);
            }
        }
        virtual mesh_ptr n(uint8_t dimension, mesh_ptr p) override
        {
            return MultiTree::n(dimension, p);
        }
        virtual mesh_ptr n(uint8_t dimension) override
        {
            switch(dimension)
            {
            case dim::z: return owner();
            default: return MultiTree::n(dimension);
            }
        }

    public:  // copy/move semantics
        PropertyTree()
            : MultiTree{}
        {}
        PropertyTree(PropertyTree const& other) = delete;
        PropertyTree(PropertyTree&& other) = delete;
        PropertyTree& operator=(PropertyTree const& other) = delete;
        PropertyTree& operator=(PropertyTree&& other) = delete;
    protected:  // tree accessors
        mesh_ptr owner(mesh_ptr p)
        {
            return Mesh::n(dim::z, p);
        }
        mesh_ptr owner()
        {
            return Mesh::n(dim::z);
        }
        mesh_ptr property(mesh_ptr p)
        {
            mesh_ptr ret = nullptr;
            if(Mesh::of<PropertyTree>(p) && !Mesh::of<PropertyTree>(p)->owner())
                Mesh::of<PropertyTree>(p)->owner(shared_from_this());
            if(p && Mesh::of<PropertyTree>(property()))
                ret = Mesh::of<PropertyTree>(property())->property(p);
            else
                ret = Mesh::p(dim::z, p);
            return ret;
        }

        mesh_ptr property()
        {
            return Mesh::p(dim::z);
        }
    };

}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_PROPERTY_TREE_H
