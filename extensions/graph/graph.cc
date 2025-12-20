extern "C"
{
#include "lmdb.h"
}

#include "../system.h"
#include "../bitarray/bitarray.h"
#include "../iter/iter.h"
#include "../graphdb/db.h"
#include "../graphdb/schema.h"
#include "../graphdb/envpool.h"
#include "../graphdb/transaction.h"
#include "../graphdb/graphdb.h"
#include "feature.h"
#include "graph.h"

extensions::graph::Graph::operator std::string()
{
    std::string ret = "";
    ret += "V=";
    this->vertices([&](feature::index_t v){ ret += std::to_string(v) + ", "; });
    ret += "\nE=";
    this->edges([&](feature::index_t e1, feature::index_t e2)
                {
                    ret += "(" + std::to_string(e1) + "," + std::to_string(e2) + ")" + ", ";
                });
    ret += "\n";
    return ret;
}

void extensions::graph::Graph::vertices(vertices_visitor_t visitor, size_t start, size_t stop, size_t step)
{
    auto vertices = vertices_.accessor<bitarray::cell_t, 1UL>();
    size_t sz = bitarray::size(vertices);
    if(!stop) stop = sz;
    for(size_t i = start; i < stop; i += step)
    {
        if(!bitarray::get(vertices, i)) continue;
        visitor(i);
    }
}
void extensions::graph::Graph::edges(edges_visitor_t visitor, size_t start, size_t stop, size_t step)
{
    auto vertices = vertices_.accessor<bitarray::cell_t, 1UL>();
    auto edges = edges_.accessor<bitarray::cell_t, 2UL>();
    size_t sz = bitarray::size(vertices);
    if(!stop) stop = sz*sz;

    while(start < stop)
    {
        size_t v = start / sz;  // the row of the adj mtx
        size_t e = start % sz;  // the index of the edge in a row of the adj mtx
        size_t cellV = v / bitarray::cellsize;  // the cell of the vertex in the vertex set
        size_t cellE = e / bitarray::cellsize;  // the cell of the edge in the adj mtx
        auto row = edges[v];
        if(!bitarray::get(vertices, v)) goto CONTINUE;
        if(!bitarray::get(row, e)) goto CONTINUE;
        visitor(v,e);
CONTINUE:
    if(0 == vertices[cellV])
    {
        // If the cell is zero, the vertex of the current iteration is
        // the first vertex of the cell, hence skip the whole cell.
        size_t leap = (sz * bitarray::cellsize) / step;
        start += step * std::max(leap, (size_t)1);
    }
    else if(0 == row[cellE])
    {
        size_t leap = (bitarray::cellsize / step);
        start += step * std::max(leap, (size_t)1);
    }
    else
        start += step;
    }
}

void extensions::graph::Graph::vertices(vertices_visitor_t visitor, size_t start, size_t stop, size_t step) const
{
    const_cast<Graph*>(this)->vertices(visitor, start, stop, step);
}
void extensions::graph::Graph::edges(edges_visitor_t visitor, size_t start, size_t stop, size_t step) const
{
    const_cast<Graph*>(this)->edges(visitor, start, stop, step);
}

bool extensions::graph::Graph::vertex(feature::index_t i)
{
    return bitarray::get(vertices_, i);
}
extensions::graph::feature::index_t extensions::graph::Graph::vertex(feature::index_t index, bool truth)
{
    const feature::index_t max = bitarray::size(vertices_) - 1;

    assertm(max >= index, "Index ", index, " is out of bounds [0,", max, ").");
    feature::index_t ret = index;

    if(truth)
    {

        /**
         * Finds the next available slot in the bitmap, following the open
         * addressing algorithm. If there is a collision at the given index
         * do a) select a random number r in [0, N*PAGE_SIZE*CHAR_BIT]
         * where N is the number of pages, b) if there is a collision at r
         * move forward until the end of the page (PAGE_SIZE*CHAR_BIT).
         * If a free slot cannot be found, expand the bitmap by one page
         * and place the vertex in the new page at r%(PAGE_SIZE*CHAR_BIT).
         */

        bool needs_expansion = false;
        feature::index_t step = 0;
        feature::index_t slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<feature::index_t> d(0, max);

        while(true)
        {
            if(ret + step <= max && !bitarray::get(vertices_, ret + step)) break;
            if((ret || step) && !((ret + step) % slice))
            {
                needs_expansion = true;
                step = 0;
                ret = max + 1 + (ret % slice);
                break;
            }
            if(ret == index)
            {
                ret = d(e);
                step = 0;
            }
            else
                step += 1;
        }
        if(needs_expansion)
        {
            torch::Tensor vertices = torch::full(vertices_.sizes()[0] + extensions::graphdb::schema::page_size,
                                                    0,
                                                    vertices_.options());
            torch::Tensor edges = torch::full({bitarray::size(vertices), vertices.sizes()[0]},
                                                0,
                                                edges_.options());
            vertices.slice(0 /* rows */,
                            0 /* start row */,
                            vertices_.sizes()[0] /* end row */) = vertices_.slice(0, 0, vertices_.sizes()[0]);
            edges.slice(0 /* rows */,
                        0 /* start row */,
                        edges_.sizes()[0] /* end row */)
                    .slice(1 /* columns */,
                        0 /* start column */,
                        edges_.sizes()[1] /* end column */) = edges_.slice(0, 0, edges_.sizes()[0])
                                                                    .slice(1, 0, edges_.sizes()[1]);
            vertices_ = vertices;
            edges_ = edges;
        }
        ret = ret + step;
        bitarray::set(vertices_, ret, truth);
    }
    else  // !truth
    {
        bitarray::set(vertices_, ret, truth);
        auto adjm = edges_.accessor<bitarray::cell_t, 2>();
        for(size_t i = 0; i < bitarray::size(vertices_); ++i)
        {
            if(i == ret)
            {
                for(size_t k = 0; k < adjm[i].size(0); ++k)
                    adjm[i][k] = 0;
            }
            else
                bitarray::set(adjm[i], ret, truth);
        }
    }

    return ret;
}
bool extensions::graph::Graph::edge(feature::index_t i, feature::index_t j)
{
    return bitarray::get(edges_[i], j);
}
void extensions::graph::Graph::edge(feature::index_t i, feature::index_t j, bool truth)
{
    assertm(bitarray::get(vertices_, i), i);
    assertm(bitarray::get(vertices_, j), j);

    auto tensor = edges_[i];
    bitarray::set(tensor, j, truth);
}

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    auto c = py::class_<extensions::graph::Graph, extensions::ptr_t<extensions::graph::Graph>>(m, "Graph");
    extensions::graph::Graph::def(c);
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
    extensions::graph::feature::FEATURE_PYBIND11_MODULE_IMPL(m);
}
