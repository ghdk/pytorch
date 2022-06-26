#ifndef EXTENSIONS_MESH_MESH_H
#define EXTENSIONS_MESH_MESH_H

namespace extensions { namespace mesh
{

    /**

     * Main issue driving this design is the explosion of types.

     * std::map<int> - The type is poluted by the type of the elements, std::map<int> != std::map<double>
                       mesh_ptr is agnostive of the types of its elements
                     - The std::map, forces ordering on keys
                       mesh_ptr forces no ordering. An ordering algorithm can attach a mesh node on the
                       properties of the original node, hence creating an ordering. We can have an infinite
                       orderings on a set, but each ordering algorithm will need its own ordering type, or
                       a unique id, so that we can identify it in the properties of a node.
                     - The type of the key polutes the type of the map
                       mesh_ptr is agnostic of its elements. Indexes can be attached to the properties of the
                       nodes, a node can have multiple indexes, and distinguished by a unique identifier.
                       The same node can belong to multiple sets, and have a different index at each set.
                     - The set of sets is std::map<std::map<int>>, no type recursion
                       mesh_ptr is a recursive type
      
     * Multiple inheritance - Sets were formed through multiple inheritance, so that their properties could
                              be generalised and be reused. Explosion of types.
                              The new set object, does not care about the types of each elements. The only
                              thing it does is mesh_ptr management. All algorithms and operations that do
                              mesh_ptr management can be part of the set object. APIs that need to provide
                              more than one implementation can be generalised, and implemented as visitors.
                              All functionality that is type dependent will be implemented as visitors.
                              The set iterators/enumerables will be standalone.
                              Hence, i foresee only one Set type. 

     * powerset - Needs to be implemented as an iterator as it cannot commit to a return type, what type
                  should it return? Iterator/Enumerable types.
                  Under the new scheme the powerset will be 
                  ptr_t<Set>Powerset<Set,Enumerable>(ptr_t<Enumerable>). 
                  There will be a very small number of return and enumerable types. Translating the return 
                  type to a different set type is dirt cheep if you don't want to have a second template
                  specialisation.

                  ~~~~ On the API ptr_t<Set>Powerset<Set,Enumerable>(ptr_t<Enumerable>). Returning a ptr_t
                  means the user of the API needs to take the new pointer and attach it to the mesh. If so
                  the API can be simplified by having the user first allocate the new pointer, attach it
                  to the mesh and then pass a ref to the function which is now returning nothing, ie,
                  void PowerSet<Set,Enumerable>(Set&, Enumerable&). Since the powerset now does pointer
                  management and modifies the passes set object, it can be implemented as a class method,
                  or standalone function. The root set, and all subsets will be of the same type Set, if
                  the user needs further customising, they can convert the sets to the desired types, its easy.

                  ~~~~ So we try to decouple the return type of the algorithm, from the type of the
                  container that will hold the elements. In the old implementation the powerset is
                  an iterator so that it returns a sequence of items, but it does not dictate the type
                  of the container that will hold them.

                  Now we also have the mapped enumerable, which can map from one type to another at any
                  time. So even if the powerset was using a fixed type for sets, that type can be wrapped
                  into a mapped iterator, and translated to the required set type.

                  So the next question is can we implement the powerset to something different from an
                  iterator, so that we can make the implementation more natural?

                  ~~~~ On the other hand we can see the powerset having two responsibilities (a) implementing
                  the powerset algorithm and (b) mapping from the source to the target set type. Can we
                  implement the powerset as a fanctor such as
                  mesh_ptr operator*(mesh_ptr)
                  {
                     ptr_t<source> src;  // cast argument
                     ptr_t<target> ret;
                     ...
                  }

                  Hence given a tree we pass it through filter iterator, and we wrap the filter iterator
                  into a mapped iterator, which calls the powerset fanctor and translates all the
                  sets to the mapped type + powerset.

                  FIXME: where will it get the enumerable types from? does the collection need to be enumerable?

                  ~~~~ Since it is a fanctor it can be constructed with an enumerable type. This requires
                  that the enumerable can be given a mesh_ptr after the constructor and before the begin.
                  Or it can be given as a template specialisation of an enumerable type, and have the
                  powerset construct the enumerable. Hence at run time it will only need a mesh_ptr.
                  It doesn't have to receive an enumerable at runtime.

                - Needs to receive an enumerable to abstract away the types of the containers.
                  Now it needs to receive an enumerable as the set type will not have begin/end methods.
                - Needs to provide helper so that a set can be created with the results of the powerset
                  iterator with CRTP so that it can use the type of the target set. Additional types.
                  No need for a helper, it will return a mesh representation of a set.

     * set union - required sets of the same type
                   Deep and shallow unions: Depending on the enumerable passes, the union algorithm
                   can create deep or shallow unions. If a mapped enumerable returns copies of items
                   then the union will be deep. If the enumerable returns just the items the union
                   will be shallow.

                   ~~~~ Need a copy virtual method.

                   ****How can i say the same element belongs to more than one sets?****
                   ****How can i say a set has elements and belongs to multiple sets?****
                   ****Do i need to decouple the mesh node, from the value that it represents?****
                   ****I.e. two mesh nodes have the same value if their z vector is equal?****
                   ****I.e. two mesh nodes are identical if they point to the same z vector?****

                   ****

                   s ........ s       (The first set (s) an element (e) joins, owns the element...)
                   | \       / \      (...subsequent entries create image mesh nodes (m), of type multitree...)
                   e  m     m   m     (...whose y points to the element (e)...)
                   |                  (...e's nx creates a list of all its images...)
                   c                  (...this implementation is hidden in the set, and its enumerators...)
                                      (...if self.y and self.y.nx and self != self.y.parent then i am an image...)
                                      (...this describes a multitree, where each image (m) can have its on (z) to override attributes of (e)...)
                                      (How will this work with copy is this, all nodes will know their copies through (nz)...)
                                      (...as we traverse the tree we first traverse it through the shared pointers, which create copies,...)
                                      (...and then through the weak, where the copy method returns the copy from (nz) just to link them, hence if we reach (e)...)
                                      (...1. we reach (e) from (s) for the first time, hence we make a copy...)
                                      (...2. we reach (e) from (m) for the first time, hence we make a copy...)
                                      (...3. we reach (e) from (m)    additional time, hence we find the copy...)
                                      (...a second traversal will be needed of the source to remove the links to the copies from (nz)...)
                                      
                                      copy()
                                      {
                                          if(!self.nx)
                                              self.nx = make_shared()
                                          self.nx.z = self.z->copy()
                                          // since we recurse over shared pointers first
                                          // all copies must exist by now
                                          self.nx.nz = self.nz.nx 
                                      }


                   ****

                   Three operations
                   - copy element:
                   - move element:
                   - reference element:

     * complement/line graph - since they were using a map, they had to understand the types of the keys,
                               and their structure
                               On the mesh a vertices and edges would be small trees. A vertex in a line
                               graph would be the two edge trees, and the edges will be the vertex trees.
                             - since the keys infected the type of the container, they had to introduce new
                               types.
                               In the new representation there is no compile time defined type for any
                               aggregation of values. An index will be a tree, hence they will be easy to
                               manipulate.

     * python - Each module had all its types defined, no reusable types
              - Each module had to define iterators/pairs, etc...
              - Not a pretty way of exposing templates to python.
              - Set constructors from python lists.
              - Python lists formed from enumerables.
              - C++ APIs of T& vs Python APIs of ptr_t<T>
                No C++ function will allocate objects. All C++ APIs will be using T&, and they will accept
                the tree they will be working under, ie. the user will allocate the new tree, and pass
                it to a C++ function. Hence Python will work with ptr_t<T>, and in C++ we will see ptr_t<T>
                only where we create new nodes to place into the mesh.

     * Higher order meshes: On the z vector i could have bridge nodes, that connect the same node
       over different parallel meshes. The multitree allows me to add the same node under different
       sets, but the node can only have one subtree. Higher order meshes, through the bridge node,
       allows us the same node to have different subtrees on parallel meshes. Or i could introduce
       another vector.

       - every aggregation of items can change, the only fixed compile type is the mesh
       which is type agnostic, and all types are expressed as a mesh, ex a double is a 
       node of a mesh, an edge could a small tree/mess, a set is a bigger mesh, a graph
       even bigger
       - in traditional templated types, the type of the container is infected by the types
       of each elements, and it is fixed at compile time, ex, std::set<Vertex<int>>,
       hence i need a data type per interpretation, while now a set would be collection<mesh*>
       - there is a single data type, which is recursive, ie mesh, so you can
           - chain functions
           - functions can receive and return the same type, ie mesh
           - the same mesh can be filtered and be given different meaning by different methods
           - the same mesh can be reorganised, or transformed from one base type to another, ie from int to float
           - the same mesh can be used to express different shapes, concepts etc
           - there isn't an std::set<float> that cannot be used with int, the mesh allows for any transformation
           or that we need to compile of all the std::set<T> for every type that we forcee, plus making all
           functions aware of all these types. 

       - Transformation: transforms from one interpretation to another, most probably they are interpretation
       specific as they hold the logic that integrates two interpretations, receives a mesh and the
       two interpretations, and does the transformation, ex set to powerset, graph to linegraph, double to float.
       Having dedicated transformations allows us to host transformation logic outside the interpreters, and to
       avoid poluting the interpreters with all the types we need to support transformations with. Basically
       transformations factories from source to destination interpretations. Is this a functor, does it inherit
       from a common base?
       It creates a new mesh, hence it needs to copy parts of the source mesh, the copy is done in the source
       interpretation. However, this raises the issue of managing the copies. Lets say we have the mesh

       a -- b -- c

       and we transform b->d, this means we will have 

       a -- b -- c and d

       now in order to use d, we have to replace b with d

       a -- d -- c

       or go from b to a, create a copy of the original structure and replace b with d.

       or we can use the z axis to keep track of transformations

       a -- b -- c
            |
            d

       hence, the z axis holds a stack of intepretations. However, algorithms only work in 2d space, hence 
       only the intepretation at the front is used. We can move any intepretation at the front as it is a 
       simple link list. Edit: Moving the intepretation to the front wont work, lets think of the example
       of somebody holding a pointer to node b, and that is transformed to node d. If d->z = b, the person
       who is holding a pointer to b cannot access d. Instead new interpretations need to be appended to 
       z and the algorithms that traverse the tree look for the most recent z.

       Maybe i will need an identifier to be able to recognise translations, or a printable representation
       of the intepretations. Save the address of the method that created the intepretation?

       - Interpretation: inherits from mesh pattern, gives a specific meaning to the pattern,
       ex. set interpetation, graph interpretation, path interpretation, tuple interpetation, nodes capturing a
       double type.
       - Interpretation can use multiple inheritance so that they are reusable into different
       interpretations, a set interpretation can have subset, and membership traits. They will be formed by
       multiple interpretations, ex graph interpretation consists of set, vertex, and edge interpretations.
       FIXME: needs further analysis. For example lets think of an element. The base type can be double
       and we can have an indexed element, a coloured element, and an element of both index and color. Since
       we do not have explicit aggregate types the nodes would be

       double - index
              \ color

       set - multiset
           \ ordering
           \ color
           \ index

       (a) Lets assume we achieve this through multiple inheritance
           - constructors each add their own child - so all traits need to derive from Tree?
       (b) Lets assume we achieve this through fat objects, ie subclass from double and add the extra
           APIs.
       (c) Traits do not subclass from tree, but they provide methods that receive a mesh. They could also
           check the type of mesh given, and become noops when they do not apply.
       (d) Traits could receive the target type through CRTP, cast this to the received type, and hence 
           provide methods that operate on this, without inheriting from Tree.
       (e) A functor tree node??
           - It would allow me to modify the behaviour of the parent interpretation at run time.
           - How will i use it?
           - Mesh defines Mesh::obtain<X>(node), traverses the node, gets the functor of type X.
           - Calls the functor.
           - Everything is a mesh, adding color to a node, call the color method, which adds a node for color.
           - Run time modifications.
           - Static method  void color(mesh_ptr, color)
           - Static method color color(mesh_ptr, ...)
           - Do i need a specific vector for attributes? z? m for metadata? a for attributes? p for property?
           - Color could inherit from tree, constructor attaches itself to the given mesh, ie instantiates
             the object to the correct location in the mesh that is given.

       Hence: The set node, will have into its z vector an index node. Index nodes of the same type
       will be linked to its elements z vectors too. Hence forming an indexed set, with customizable 
       index objects. Algorithms can query the set node to get the index type, and then work with the
       indexes of the set's elements. So all index nodes should derive from the same base class, and
       algorithms should query the set's index property using the base class, then we can cast/query 
       using specific types. Base classes will be defined under set/properties, element/properties.

       Two APIs, Virtual methods, or Visitor pattern? First implement everything as virtual methods.
       When we find the same algorithm needs to be generalised and overloaded, implement it as visitor.
       So if you find yourself having to subclass an interpretation in order to provide generalisations of
       the same interpretation, then implement the overloaded methods as visitors instead, ex., subclassing
       the set interpretation to provide alternative set APIs, then these APIs can be implemented as visitors,
       and keep a single set class. The visitor can be a functor, the visitor's arguments are given to the
       constructor of the visitor. The visitor is applied on the tree through the map method.       

       Iterators/enumerables have two responsibilities
       - Iteration strategy, ie traversal, i want this to be reusable
       - Type conversion, from mesh_ptr, to base type such as double

       An algorithm such as the mean will need to work with base types, ie, needs a conversion from
       mesh_ptr to double. Hence it can receive an MapEnumerable<double(mesh_ptr)>. They receive a
       range of mesh traversal iterators (begin,end) and map the function on the range. I may need
       a factory for creating the enumerables, so that it is overloaded based on the return type
       of the functor.

       ~~~~ The problem with this approach is exposing all these conversion types to python!
       ~~~~ Instead we already expose in python the API Mesh<T>::of, hence the algorithm can do
       mean(mesh_enumerable, std::function<double(mesh_ptr)>), and python can pass Mesh<T>::of.
       This interface allows the same mean to be used with any mesh_enumerable (virtual, see bellow),
       and any mesh_ptr that can be converted to double. Most probably will be 
       mean(enumerable, [](mesh_ptr)->double{ return (double) Mesh<X>::of(mesh_ptr); }
       mean(enumerable, lambda mesh_ptr : float(Z().of(mesh_ptr)))

       ~~~~ All this is wrong, since i am having a virtual hierarchy i need to take advantage of it. I need
       to have a Numeric base class, from which through a template i get Double, Int, etc. I need to have
       a collection base class from which all collections derive. Hence when an algorithm needs to say i want
       to use an object that meets this interface has a type to cast to. Alternatively, i could have the
       algorithms being templated to the return type of the std::function<ptr_<T>(mesh_ptr)>, so that the
       same algorithm can be used with different mesh_ptr types. All these will be exposed through overloading.

       Two options, (a) use anchor types within the virtual hierarchy, for which we can query the types
       in the algorithms, or (b) use templates. (a) requires that all types are derived from the anchors,
       (b) requires that we expose all the needed interfaces to python through specialisation, at least
       if we end up using mapped enumerables they can provide this conversion layer.

       ~~~~ I need a virtual recursive data structure, and templated algorithms.

       The strategy enumerables need to work over mesh_ptr, and they just do traversals of the mesh, ex
       tree traversal, or set traversal. They are initialised with a mesh_ptr. They are virtual.

       - How can python take all elements of a set?
         If we want a list of mesh_ptr from a set, then we can use the traversal iterator. If we want
         to work with the actual types instead in python will be easier as the methods are bound to
         the object through the boost def, and the interface will be available regardless of the 
         c++ pointer type.

       - constructor receiving a list?
         This interface needs to change. In the old interface the type of the set was the same with the
         typ of the list, hence we could do STLEnumerable<double>. Now we have no such information.
         We also want to be able to create variadic type sets from variadic type lists.
         In CPP the constructor can assume a list of mesh_ptr. In python a simple list comprehension
         can translate a list of doubles to a list of mesh_ptr for doubbles.

       .. Iterators/Iterables. We won't have either. The API responsible for iterations is the map method.
       .. It should receive the function to map, and the function defining the iteration strategy. Hence
       .. algorithms like the mean, which would iterate through the iterator returned by the enumerable, now
       .. they will be mapped onto the mesh, each time receiving a mesh_ptr, and in their global state they
       .. calculate the mean. Algorithms that need to do several passes, with different traversals, they can
       .. call map with lambdas, or other functions, and calculate temporary states. So all loops will happen
       .. through map and lambdas. NOTE: The map interface needs to move under Traversals, algorithms can
       .. employ a traversal that is reasonable for the algorithm. Otherwise the traversal will be related
       .. to interpretations, and if we want a different traversal we need to make a different interpretation.
      
       .. Problem - iterators/enumerables introduce new types, i cannot just use mesh_ptr - i cannot have
       .. virtual enumerable as what type will the begin/end return?

       .. - How can python take all elements of a set?
       .. - Does the map interface needs to be parameterised?
       ..   - How can i implement break and continue?
       ..     - This could be implemented by the functor becoming a noop.
       .. - boost any_range?
       .. - Virtual enumerable/iterator? https://www.codeproject.com/Articles/92671/How-to-write-abstract-iterators-in-Cplusplus
       ..   - The problem here is the return type of the dereference of iterators.
       ..   - This can be resolved if all iterators/iterables work over mesh_ptr only.
       .. - See
       ..   /UnrealEngine/Engine/Plugins/Experimental/GeometryProcessing/Source/GeometricObjects/Public/Util/RefCountVector.h
       ..   /UnrealEngine/Engine/Plugins/Experimental/GeometryProcessing/Source/GeometricObjects/Public/Util/IteratorUtil.h
       .. - constructor receiving a list?
       ..   - Templated factory?

       - Pattern: mesh patterns, planar tree, polygon, etc, these define how the mesh is put together,
       for instance a tree pattern might be used to represent a set. Inherit from mesh.

       - An algorithm receives a mesh, and a translation/interpretation and returns a new mesh

       - Tree interpretation for the mesh? and set, tuple etc inherit from tree interpretation?

       - Partial order, a mesh/tree type connecting the nodes in the ordering.
          - Needs an enumerable that traverses based on ordering.
          - Needs to be able to attach and distinguish between multiple orderings.

          What is an ordering? Probably is a tree mesh type, which allows us to represent total orders
          when each node has one child, and partial orders when nodes can have more than one children.

          How can i say iterate using this or that ordering? I need a fanctor mesh type?

          - Implementing an enumerable that accepts an ordering functor is well known.
          - An ordering attaches to the set type, and creates the ordering by placing copies of itself in 
          the members of the set.
          - Hence the reference that the user has can be used to identify and use a specific ordering.
          - Probably the ordering needs to receive a comparison function as well, so that it is type agnostic.
          - Is the ordering also an iterator, or is there an iterator for accepting an ordering function?
          - So the ordering has a (a) functor for applying a comparison method on a set, and (b) a fanctor
          for returning the next element?

          ..
          .. for i in filtered_enumerable:
          ..    ordered_enumerable(comp, mesh_ptr)  // templated to child_enumerable??
          ..                                        // or should it move under patterns and
          ..                                        // be given full access to the tree?
          ..                                        // Both? It can do a single iteration
          ..                                        // of the elements of set, use insert short
          ..                                        // to build the ordering, and be placed under
          ..                                        // patterns.
          ..    for m in mapped_enumerable(ordered_enumerable, func):
          ..      ...
          ..
          .. So the ordering is an iterator. In its constructor it it populates the z vector of the set
          .. with the ordering data. The ordering data holds the comp function, which uniquely identifies an
          .. ordering, as there is only one ordering that can be generated from a comp function.. 
          ..
          .. When the user constructs an enumerable using a specific comp function, the code looks at the tree
          .. for an ordering with the same comp function, and uses the existing ordering or creates one 
          .. otherwise.
          ..
          .. Ordering: Tree
          .. OrderingIterator
          .. OrderingEnumerable
          .. ComparisonFunction(mesh_ptr, mesh_ptr) -> -1,0,1  // responsible also for mapping mesh_ptr
          ..                                                   // to a comparable type
          ..                                                   // -1 less than
          ..                                                   // 0 equal
          ..                                                   // 1 independent
          ..

       - Set index, each node can have multiple indexes, of different types. It could have multiple indexes
       of the same type but not sure how these can be used. So the set type can be
       Set<IndexFunction, OrderingType>. If i want to convert a set to a different IndexFunction, i can
       simply create the new set type, populate it with the mesh_ptr of the old set, and add the new index
       type to the elements. Hence the same element can appear in more than one sets with different index
       types. Or we could make deep copies of the elements.
       - The index with the ordering might be combined, so that elements are ordered based on their index
       instead.

       ~~~~ This will be implemented as the approach for the partial order above. The index will attach
       on the z vector, it will form a linked list, and each node will have an index type, or subtree,
       whatever. It can be combined with an ordering, so that the index imposes a difference ordering
       as well.
     */
    
    class Mesh
        : public std::enable_shared_from_this<Mesh>
    {

    public:  // typedefs
        using mesh_ptr = ptr_t<Mesh>;
        using mesh_wptr = ptr_w<Mesh>;
        using mesh_ref = Mesh&;

        struct dim
        {
        public:  // typedefs

            /**
             * A new dimension is needed when the shape with the new relationship
             * needs to preserve the relationships in the existing shape.
             */
            enum e
            {
                p = 0,
                x = 1,
                y = 2,
                z = 3,
                h = 4
            };
        public:  // copy/move semantics
            dim()
                : next_{}
                , positive_{nullptr}
                , negative_{}
            {}
            dim(dim const& other) = delete;
            dim(dim&& other) = delete;
            dim& operator=(dim const& other) = delete;
            dim& operator=(dim&& other) = delete;
        public:  // members
            ptr_u<struct dim> next_;
            mesh_ptr  positive_;
            mesh_wptr negative_;
        };

    public:  // introspection
        template <typename T>
        static inline ptr_t<T> of(mesh_wptr p)
        {
            return !p.expired() ? Mesh::of<T>(p.lock()->shared_from_this()) : nullptr;
        }
        
        template <typename T>
        static inline ptr_t<T> of(mesh_ptr p)
        {
            return std::dynamic_pointer_cast<T>(p);
        }

        template <typename T>
        static inline T& of(mesh_ref r)
        {
            return dynamic_cast<T&>(r);
        }

        template <typename T>
        static inline bool is(mesh_wptr p)
        {
            return !p.expired() ? Mesh::is<T>(p.lock()->shared_from_this()) : false;
        }
        
        template <typename T>
        static inline bool is(mesh_ptr p)
        {
            return (p ? Mesh::is<T>(*p) : false);
        }

        template <typename T>
        static inline bool is(mesh_ref r)
        {
            return typeid(T).hash_code() == typeid(r).hash_code();
        }

    public:  // virtual methods
        virtual ~Mesh()
        {                
            dimension_.next_.reset();
            dimension_.positive_ = nullptr;
            dimension_.negative_.reset();
        }

        virtual mesh_ptr p(uint8_t dimension, mesh_ptr p)
        {
            assertm(dimension, "Cannot assign to 0th dimension!");
            return (*this)[dimension].positive_ = p;
        }
        virtual mesh_ptr p(uint8_t dimension)
        {
            return !dimension ? shared_from_this() : (*this)[dimension].positive_;
        }
        virtual mesh_ptr n(uint8_t dimension, mesh_ptr p)
        {
            assertm(dimension, "Cannot assign to 0th dimension!");
            if(!p)
                (*this)[dimension].negative_.reset();
            else
                (*this)[dimension].negative_ = p->shared_from_this();
            return Mesh::n(dimension);
        }
        virtual mesh_ptr n(uint8_t dimension)
        {
            auto ret = (!(*this)[dimension].negative_.expired()
                        ? (*this)[dimension].negative_.lock()->shared_from_this()
                        : nullptr);
            ret = (dimension ? ret : nullptr);
            return ret;
        }
        virtual std::string str()
        {
            size_t count = 0;
            for(auto i = &dimension_; i; i = i->next_.get())
                count++;
            std::stringstream ret;
            ret << demangle(typeid(*this).name());
            ret << " (";
            ret << count;
            ret << " dimensional mesh)";
            return ret.str();
        }

    public:  // copy/move semantics
        Mesh()
            : dimension_{}
        {}
        Mesh(Mesh const& other) = delete;
        Mesh(Mesh&& other) = delete;
        Mesh& operator=(Mesh const& other) = delete;
        Mesh& operator=(Mesh&& other) = delete;

    private:  //dimension
        dim dimension_;

        dim& operator[](uint8_t dimension)
        {
            auto tmp = &dimension_;
            while(dim::x < dimension--)
            {
                if(!tmp->next_)
                    tmp->next_.reset(new struct dim);
                tmp = tmp->next_.get();
            }
            return *tmp;
        }

#ifdef PYDEF
    public:
        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;

            c.def(py::init<>());
            c.def("__repr__", &T::str);
            c.def("__str__", &T::str);
            c.def("__of__", static_cast<ptr_t<T>(*)(mesh_ptr)>(&Mesh::of<T>));
            c.def("__is__", static_cast<bool(*)(mesh_ptr)>(&Mesh::is<T>));
            c.def("__eq__", +[](mesh_ptr rhs, mesh_ptr lhs){ return rhs == lhs; });
            c.def("p", static_cast<mesh_ptr(T::*)(uint8_t)>(&T::p));
            c.def("p", static_cast<mesh_ptr(T::*)(uint8_t,mesh_ptr)>(&T::p));
            c.def("n", static_cast<mesh_ptr(T::*)(uint8_t)>(&T::n));
            c.def("n", static_cast<mesh_ptr(T::*)(uint8_t,mesh_ptr)>(&T::n));
            return c;
        }
#endif  // PYDEF
    };
    
}}  // namespace extensions::mesh

#endif  // EXTENSIONS_MESH_MESH_H
