#ifndef EXTENSIONS_MESH_STAR_H
#define EXTENSIONS_MESH_STAR_H

namespace extensions { namespace mesh { class Mesh; }}

namespace extensions { namespace mesh
{
    class Star
        : public Mesh
    {
    public:  // virtual methods

        virtual mesh_ptr p(uint8_t dimension, mesh_ptr p) override
        {
            mesh_ptr ret = nullptr;
            switch(dimension)
            {
            case dim::x:
            {
                ret = Mesh::p(dim::x, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::n(dim::x, shared_from_this());
                break;
            }
            case dim::y:
            {
                ret = Mesh::p(dim::y, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::n(dim::y, shared_from_this());
                break;
            }
            case dim::z:
            {
                ret = Mesh::p(dim::z, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::n(dim::z, shared_from_this());
                break;
            }
            default: assertm(false, "Cannot handle dimension!");
            }
            return ret;
        }
        virtual mesh_ptr p(uint8_t dimension) override
        {
            return Mesh::p(dimension);
        }
        virtual mesh_ptr n(uint8_t dimension, mesh_ptr p) override
        {
            mesh_ptr ret = nullptr;
            switch(dimension)
            {
            case dim::x:
            {
                ret = Mesh::n(dim::x, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::p(dim::x, shared_from_this());
                break;
            }
            case dim::y:
            {
                ret = Mesh::n(dim::y, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::p(dim::y, shared_from_this());
                break;
            }
            case dim::z:
            {
                ret = Mesh::n(dim::z, p);
                if(Mesh::of<Star>(p))
                    Mesh::of<Star>(p)->Mesh::p(dim::z, shared_from_this());
                break;
            }
            default: assertm(false, "Cannot handle dimension!");
            }
            return ret;
        }
        virtual mesh_ptr n(uint8_t dimension) override
        {
            return Mesh::n(dimension);
        }

    public:  // copy/move semantics
        Star()
            : Mesh{}
        {}
        Star(Star const& other) = delete;
        Star(Star&& other) = delete;
        Star& operator=(Star const& other) = delete;
        Star& operator=(Star&& other) = delete;
    };
    
}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_STAR_H
