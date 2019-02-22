#pragma once
#include <string_view>
#include <iostream>
#include <algorithm>
#include <optional>
#include <cstring>
#include <vector>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

namespace intercept::types {
    
#pragma region Allocator


    namespace __internal {
        void* rv_allocator_allocate_generic(size_t _size);
        void rv_allocator_deallocate_generic(void* _Ptr);
        void* rv_allocator_reallocate_generic(void* _Ptr, size_t _size);
    }

    template<class Type>
    class rv_allocator {
    public:

        using value_type = Type;

        using pointer = value_type *;
        using const_pointer = const value_type *;

        using reference = value_type&;
        using const_reference = const value_type&;

        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::true_type;


        template<class Other>
        using rebind = rv_allocator<Other>;

        template<class Other>
        using rebind_alloc = rv_allocator<Other>;

        template<class Other>
        using rebind_traits = std::allocator_traits<rv_allocator<Other>>;

        rv_allocator() = default;
        template<class Other>
        rv_allocator(const rv_allocator<Other>&) {}
        template<class Other>
        rv_allocator(rv_allocator<Other>&&) {}

       

        static void deallocate(Type* _Ptr, size_t = 0) {
            return __internal::rv_allocator_deallocate_generic(_Ptr);
        }

        static void destroy(Type* _Ptr) {
            _Ptr->~Type();
        }

        static void destroy(Type* _Ptr, size_t size_) {
            for (size_t i = 0; i < size_; ++i) {
            #pragma warning(suppress: 26409)
                (_Ptr + i)->~Type();
            }
        }

        static void destroy_deallocate(Type* _Ptr) {
            destroy(_Ptr);
            return __internal::rv_allocator_deallocate_generic(_Ptr);
        }

        static void destroy_deallocate(Type* _Ptr, size_t size_) {
            destroy(_Ptr, size_);
            return __internal::rv_allocator_deallocate_generic(_Ptr);
        }

        //This only allocates the memory! This will not be initialized to 0 and the allocated object will not have it's constructor called!
        //use the create* Methods instead
        static Type* allocate(const size_t count_) {
            return reinterpret_cast<Type*>(__internal::rv_allocator_allocate_generic(sizeof(Type)*count_));
        }

        //Allocates and Initializes one Object
        template<class... _Types>
        static Type* create_single(_Types&&... _Args) {
            auto ptr = allocate(1);
            ::new (ptr) Type(std::forward<_Types>(_Args)...);
            //Have to do a little more unfancy stuff for people that want to overload the new operator
            return ptr;
        }

        //Allocates and initializes array of Elements using the default constructor.
        static Type* create_array(const size_t count_) {
            if (count_ == 1) return create_single();

            auto ptr = allocate(count_);

            for (size_t i = 0; i < count_; ++i) {
            #pragma warning(suppress: 26409)
                ::new (ptr + i) Type();
            }

            return ptr;
        }

        static Type* create_uninitialized_array(const size_t count_) {
            if (count_ == 1) return create_single();

            auto ptr = allocate(count_);

            return ptr;
        }

        static Type* reallocate(Type* ptr_, const size_t count_) {
            return reinterpret_cast<Type*>(__internal::rv_allocator_reallocate_generic(ptr_, sizeof(Type)*count_));
        }
    };

    class rv_pool_allocator {
        [[maybe_unused]] char pad_0x0000[0x24]; //0x0000
    public:
        const char* _allocName;

        int _1;
        int _2;
        int _3;
        int _4;
        int allocated_count;

        void* allocate(size_t count);
        void deallocate(void* data);

    };


    template<class Type, int Count, class Fallback = rv_allocator<Type>>
    class rv_allocator_local : private Fallback {
        //buffer will be aligned by this
        using buffer_type = int;

        //Raw size of buffer in bytes
        static const size_t buffersize_bytes = (Count* sizeof(Type));
        //Raw size in count of buffer_type elements
        static const size_t buffersize_count = (buffersize_bytes + sizeof(buffer_type) - 1) / sizeof(buffer_type);
        int dummy{0};
        buffer_type buf[buffersize_count]{ 0 };
        bool has_data{ false };
    public:

        Type* allocate(size_t count_) {
            if (count_ <= Count && !has_data) {
                has_data = true;
                return reinterpret_cast<Type*>(buf);
            }
            return Fallback::allocate(count_);
        }
        Type* reallocate(Type* ptr_, size_t count_) {
            if (ptr_ == reinterpret_cast<Type*>(buf)) {
                if (count_ <= Count) return reinterpret_cast<Type*>(buf);
                return nullptr;
            }
            return Fallback::reallocate(ptr_, count_);
        }
        void deallocate(Type* ptr_, size_t count_ = 1) {
            if (ptr_ != reinterpret_cast<Type*>(buf)) Fallback::deallocate(ptr_, count_);
            else has_data = false;
        }

        //Allocates and Initializes one Object
        template<class... _Types>
        Type* create_single(_Types&&... _Args) {
            auto ptr = allocate(1);
            ::new (ptr) Type(std::forward<_Types>(_Args)...);
            //Have to do a little more unfancy stuff for people that want to overload the new operator
            return ptr;
        }

        Type* create_uninitialized_array(const size_t count_) {
            if (count_ == 1) return create_single();

            auto ptr = allocate(count_);

            return ptr;
        }
    };

#pragma endregion


#pragma region Refcounting

    class refcount_base {
    public:
        constexpr refcount_base() noexcept : _refcount(0) {}
        constexpr refcount_base(const refcount_base &) noexcept : _refcount(0) {}
        constexpr void operator = (const refcount_base &) const noexcept {}

        constexpr int add_ref() const noexcept {
            return ++_refcount;
        }
        constexpr int dec_ref() const noexcept {
            return --_refcount;
        }
        constexpr int ref_count() const noexcept {
            return _refcount;
        }
        mutable int _refcount;
    };

    //refcount has to be the first element in a class. Use refcount_vtable instead if refcount is preceded by a vtable
    class refcount : public refcount_base {
    public:
        virtual ~refcount() = default;
        int release() const {
            const auto rcount = dec_ref();
            if (rcount == 0) {
                //this->~refcount();
                lastRefDeleted();
                //rv_allocator<refcount>::deallocate(const_cast<refcount *>(this), 0);
            }
            return rcount;
        }
        void destruct() const noexcept {
            delete const_cast<refcount *>(this);
        }
        virtual void lastRefDeleted() const { destruct(); }
    private:
        virtual int __dummy_refcount_func() const noexcept { return 0; }
    };
    class game_value_static;
    template<class Type>
    class ref {
        friend class game_value_static; //Overrides _ref to nullptr in destructor when Arma is exiting
        static_assert(std::is_base_of<refcount_base, Type>::value, "Type must inherit refcount_base");
        Type* _ref{ nullptr };
    public:
        constexpr ref() noexcept = default;
        ~ref() { free(); }

        //Construct from Pointer
        constexpr ref(Type* other_) noexcept {
            if (other_) other_->add_ref();
            _ref = other_;
        }
        //Copy from pointer
        ref &operator = (Type *source_) noexcept {
            Type *old = _ref;
            if (source_) source_->add_ref();
            _ref = source_;
            if (old) old->release();//decrement reference and delete object if refcount == 0
            return *this;
        }

        //Construct from reference
        constexpr ref(const ref &source_ref_) noexcept {
            Type *source = source_ref_._ref;
            if (source) source->add_ref();
            _ref = source;
        }
        //Copy from reference.
        ref &operator = (const ref &other_) {
            Type *source = other_._ref;
            Type *old = _ref;
            if (source) source->add_ref();
            _ref = source;
            if (old) old->release();//decrement reference and delete object if refcount == 0
            return *this;
        }
        void swap(ref &other_) noexcept {
            auto temp = other_._ref;
            other_._ref = _ref;
            _ref = temp;
        }
        constexpr bool is_null() const noexcept { return _ref == nullptr; }
        void free() noexcept {
            if (!_ref) return;
            _ref->release();
            _ref = nullptr;
        }
        //This returns a pointer to the underlying object. Use with caution!

        [[deprecated]] constexpr Type *getRef() const noexcept { return _ref; }
        ///This returns a pointer to the underlying object. Use with caution!
        constexpr Type *get() const noexcept { return _ref; }
        constexpr Type *operator -> () const noexcept { return _ref; }
        operator Type *() const noexcept { return _ref; }
        bool operator != (const ref<Type>& other_) const noexcept { return _ref != other_._ref; }
        size_t ref_count() const noexcept { return _ref ? _ref->ref_count() : 0; }
    };


    ///When this goes out of scope. The pointer is deleted. This takes ownership of the pointer
    template<class Type>
    class unique_ref {
    protected:
        Type* _ref;
    public:
        unique_ref() { _ref = NULL; }
        unique_ref(Type* source) {
            _ref = source;
        }
        unique_ref(const unique_ref& other) {
            _ref = other._ref;
            other._ref = nullptr;//We take ownership
        }
        ~unique_ref() { clear(); }

        unique_ref& operator = (Type* other) {
            if (_ref == other) return *this;
            clear();
            _ref = other;
            return *this;
        }

        unique_ref& operator = (const unique_ref& other) {
            if (other._ref == _ref) return *this;
            clear();
            _ref = other._ref;
            other._ref = nullptr;//We take ownership
            return *this;
        }

        bool is_null() const { return _ref == nullptr; }
        void clear() { if (_ref) delete _ref; _ref = nullptr; }
        Type* operator -> () const { return _ref; }
        operator Type* () const { return _ref; }
        Type *get() const { return _ref; }
    };



#pragma endregion

    /*
    This is a placeholder so i can use refcount but still have an accessible vtable pointer
    */
    class __vtable {
    public:
        uintptr_t _vtable{ 0 };
    };

    class _refcount_vtable_dummy : public __vtable, public refcount_base {};

#pragma region Containers
    template<class Type, class Allocator = rv_allocator<char>> //Has to be allocator of type char
    class compact_array : public refcount_base {
        //static_assert(std::is_literal_type<Type>::value, "Type must be a literal type");
    public:

        class const_iterator;

        class iterator {
            friend class const_iterator;
            Type* p_;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = Type;
            using difference_type = ptrdiff_t;
            using pointer = Type * ;
            using reference = Type & ;
            using _Unchecked_type = pointer;

            iterator() : p_(nullptr) {}
            explicit iterator(Type* p) : p_(p) {}
            iterator(const iterator& other) : p_(other.p_) {}
            iterator& operator=(const iterator& other) { p_ = other.p_; return *this; }

            iterator& operator++() { ++p_; return *this; } // prefix++
            iterator  operator++(int) { iterator tmp(*this); ++(*this); return tmp; } // postfix++
            iterator& operator--() { --p_; return *this; } // prefix--
            iterator  operator--(int) { iterator tmp(*this); --(*this); return tmp; } // postfix--

            void     operator+=(const std::size_t& n) { p_ += n; }
            void     operator+=(const iterator& other) { p_ += other.p_; }
            iterator operator+ (const std::size_t& n) const { iterator tmp(*this); tmp += n; return tmp; }
            iterator operator+ (const iterator& other) const { iterator tmp(*this); tmp += other; return tmp; }

            void        operator-=(const std::size_t& n) { p_ -= n; }
            void        operator-=(const iterator& other) { p_ -= other.p_; }
            iterator    operator- (const std::size_t& n) const { iterator tmp(*this); tmp -= n; return tmp; }
            std::size_t operator- (const iterator& other) const { return p_ - other.p_; }

            bool operator< (const iterator& other) const { return (p_ - other.p_) < 0; }
            bool operator<=(const iterator& other) const { return (p_ - other.p_) <= 0; }
            bool operator> (const iterator& other) const { return (p_ - other.p_) > 0; }
            bool operator>=(const iterator& other) const { return (p_ - other.p_) >= 0; }
            bool operator==(const iterator& other) const { return  p_ == other.p_; }
            bool operator!=(const iterator& other) const { return  p_ != other.p_; }

            Type& operator*() const { return *p_; }
            Type* operator->() { return  p_; }
            explicit operator Type*() { return p_; }
        };
        class const_iterator {
            const Type* p_;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = Type;
            using difference_type = ptrdiff_t;
            using pointer = Type * ;
            using reference = Type & ;
            using _Unchecked_type = pointer;

            const_iterator() : p_(nullptr) {}
            explicit const_iterator(const Type* p) noexcept : p_(p) {}
            const_iterator(const typename compact_array<Type>::iterator& other) : p_(other.p_) {}
            const_iterator(const const_iterator& other) noexcept : p_(other.p_) {}
            const_iterator& operator=(const const_iterator& other) { p_ = other.p_; return *this; }
            const_iterator& operator=(const typename compact_array<Type>::iterator& other) { p_ = other.p_; return *this; }

            const_iterator& operator++() noexcept { ++p_; return *this; } // prefix++
            const_iterator  operator++(int) { const_iterator tmp(*this); ++(*this); return tmp; } // postfix++
            const_iterator& operator--() noexcept { --p_; return *this; } // prefix--
            const_iterator  operator--(int) { const_iterator tmp(*this); --(*this); return tmp; } // postfix--

            void           operator+=(const std::size_t& n) { p_ += n; }
            void           operator+=(const const_iterator& other) { p_ += other.p_; }
            const_iterator operator+ (const std::size_t& n)        const { const_iterator tmp(*this); tmp += n; return tmp; }
            const_iterator operator+ (const const_iterator& other) const { const_iterator tmp(*this); tmp += other; return tmp; }

            void           operator-=(const std::size_t& n) noexcept { p_ -= n; }
            void           operator-=(const const_iterator& other) noexcept { p_ -= other.p_; }
            const_iterator operator- (const std::size_t& n)        const { const_iterator tmp(*this); tmp -= n; return tmp; }
            std::size_t    operator- (const const_iterator& other) const noexcept { return p_ - other.p_; }

            bool operator< (const const_iterator& other) const noexcept { return (p_ - other.p_) < 0; }
            bool operator<=(const const_iterator& other) const noexcept { return (p_ - other.p_) <= 0; }
            bool operator> (const const_iterator& other) const noexcept { return (p_ - other.p_) > 0; }
            bool operator>=(const const_iterator& other) const noexcept { return (p_ - other.p_) >= 0; }
            bool operator==(const const_iterator& other) const noexcept { return  p_ == other.p_; }
            bool operator!=(const const_iterator& other) const noexcept { return  p_ != other.p_; }

            const Type& operator*()  const noexcept { return *p_; }
            const Type* operator->() const noexcept { return  p_; }
            explicit operator const Type*() const noexcept { return p_; }
        };


        size_t size() const noexcept { return _size; }
        Type* data() noexcept { return &_data; }
        const Type* data() const noexcept { return &_data; }
        Type& front() noexcept { return _data; }
        const Type& front() const noexcept { return _data; }
        iterator begin() noexcept { return iterator(&_data); }
        iterator end() noexcept { return iterator((&_data) + _size - 1); }
        const_iterator cbegin() const noexcept { return const_iterator(&_data); }
        const_iterator cend() const noexcept { return const_iterator((&_data) + _size - 1); }

        const Type& operator[](const size_t index_) {
            return *(begin() + index_);
        }
        const Type& get(const size_t index_) {
            return *(begin() + index_);
        }

        //We delete ourselves! After release no one should have a pointer to us anymore!
        int release() const {
            const auto ret = dec_ref();
            if (!ret) del();
            return ret;
        }

        //Doesn't clear memory. use create_zero if you want that
        static compact_array* create(size_t number_of_elements_) {
            const size_t size = sizeof(compact_array) + sizeof(Type)*(number_of_elements_ - 1);//-1 because we already have one element in compact_array
            auto* buffer = reinterpret_cast<compact_array*>(Allocator::allocate(size));
        #pragma warning(suppress: 26409) //don't use new/delete
            new (buffer) compact_array(number_of_elements_);
            return buffer;
        }
        static compact_array* create_zero(size_t number_of_elements_) {
            const size_t size = sizeof(compact_array) + sizeof(Type)*(number_of_elements_ - 1);//-1 because we already have one element in compact_array
            auto* buffer = reinterpret_cast<compact_array*>(Allocator::allocate(size));
        #pragma warning(suppress: 26409) //don't use new/delete
            std::memset(buffer, 0, size);
            new (buffer) compact_array(number_of_elements_);
            return buffer;
        }
        static compact_array* create(const compact_array &other) {
            const size_t size = other.size();
            auto* buffer = reinterpret_cast<compact_array*>(Allocator::allocate(size));
            new (buffer) compact_array(size);
            std::copy(other.data(), other.data() + other.size(), buffer->data());
            return buffer;
        }

        template<class _InIt>
        static compact_array* create(_InIt beg, _InIt end) {
            const size_t size = sizeof(compact_array) + sizeof(Type)*(std::distance(beg,end)-1);
            auto* buffer = reinterpret_cast<compact_array*>(Allocator::allocate(size));
            std::memset(buffer, 0, size);
        #pragma warning(suppress: 26409) //don't use new/delete
            new (buffer) compact_array(size);

            size_t index = 0;
            for (auto& i = beg; i < end; ++i) {
                buffer->data()[index++] = *i;
            }

            return buffer;
        }

        //Specialty function to copy less elements or to allocate more space
        static compact_array* create(const compact_array &other, size_t element_count) {
            const size_t size = other.size();
            auto buffer = reinterpret_cast<compact_array*>(Allocator::allocate(sizeof(compact_array) + sizeof(Type)*(element_count - 1)));
            new (buffer) compact_array(element_count);
            std::memset(buffer->data(), 0, element_count * sizeof(Type));
            const auto elements_to_copy = std::min(size, element_count);

            std::copy(other.cbegin(), other.cbegin() + elements_to_copy, buffer->begin());

            return buffer;
        }
        void del() const {
            this->~compact_array();
            const void* _thisptr = this;
        #pragma warning(suppress: 26492) //don't cast away const
            const auto _thisptr2 = const_cast<void*>(_thisptr);
            Allocator::deallocate(static_cast<char*>(_thisptr2), _size - 1 + sizeof(compact_array));
        }

        size_t _size;
        Type _data{};
    private:
        explicit compact_array(const size_t size_) noexcept {
            _size = size_;
        }
    };


    //template<std::size_t N>
    //class r_string_const : public refcount_base { // constexpr string
    //private:
    //    const size_t _size;
    //    const char _data[];
    //public:
    //    constexpr r_string_const(const char a[N]) : _size(N - 1), _data({a[0] ... a[N]}) { add_ref(); }
    //};

    class r_string {
    public:
        constexpr r_string() noexcept {}
        r_string(std::string_view str_) {
            if (str_.length()) _ref = create(str_.data(), str_.length());
        }
        //template <size_t N>
        //constexpr r_string(const r_string_const<N>& c) {
        //    _ref = reinterpret_cast<compact_array<char>*>(&c);
        //}
        explicit r_string(compact_array<char>* dat_) noexcept : _ref(dat_) {}
        r_string(r_string&& move_) noexcept {
            _ref.swap(move_._ref);
        }
        r_string(const r_string& copy_) {
            _ref = copy_._ref;
        }

        r_string& operator = (r_string&& move_) noexcept {
            if (this == &move_)
                return *this;
            _ref.swap(move_._ref);
            return *this;
        }

        r_string& operator = (const r_string& copy_) = default;

        r_string& operator = (std::string_view copy_) {
            _ref = create(copy_.data(), copy_.length());
            return *this;
        }
        const char* data() const noexcept {
            if (_ref) return _ref->data();
            return "";
        }
        const char* c_str() const noexcept { return data(); }
        //This will copy the underlying container if we cannot safely modify this r_string
        void make_mutable() {
            if (_ref && _ref->ref_count() > 1) {//If there is only one reference we can safely modify the string
                _ref = create(_ref->data());
            }
        }
        explicit operator const char *() const noexcept { return data(); }
        operator std::string_view() const noexcept { return std::string_view(data()); }
        //explicit operator std::string() const { return std::string(data()); } //non explicit will break string_view operator because std::string operator because it becomes ambiguous
        ///This calls strlen so O(N)
        size_t length() const noexcept {
            if (!_ref) return 0;
            return strlen(_ref->data());
        }

        ///This calls strlen so O(N)
        size_t size() const noexcept {
            return length();
        }

        bool empty() const noexcept {
            if (!_ref) return true;
            return *_ref->data() == '\0';
        }

        size_t capacity() const noexcept {
            if (!_ref) return 0;
            return _ref->size();
        }

        ///== is case insensitive just like scripting
        bool operator == (const char *other_) const {
            if (!data())  return !other_ || !*other_; //empty?

            return std::equal(_ref->cbegin(), _ref->cend(),
                compact_array<char>::const_iterator(other_), [](unsigned char l, unsigned char r) {return l == r || tolower(l) == tolower(r); });
        }

        ///== is case insensitive just like scripting
        bool operator == (std::string_view other_) const {
            if (!data()) return other_.empty(); //empty?
            if (other_.length() > _ref->size()) return false; //There is more data than we can even have

            return std::equal(other_.cbegin(), other_.cend(),
                _ref->cbegin(), [](unsigned char l, unsigned char r) {return l == r || tolower(l) == tolower(r); });
        }

        ///== is case insensitive just like scripting
        bool operator == (const r_string& other_) const {
            if (!data()) return !other_.data() || !*other_.data(); //empty?
            if (data() == other_.data()) return true;

            return operator==(std::string_view(other_));
        }

        ///== is case insensitive just like scripting
        bool operator == (const std::string& other_) const {
            return operator==(std::string_view(other_));
        }

        ///!= is case insensitive just like scripting
        bool operator != (const r_string& other_) const {
            return !(*this == other_);
        }

        ///!= is case insensitive just like scripting
        bool operator != (const std::string& other_) const {
            return !(*this == other_);
        }

        ///!= is case insensitive just like scripting
        bool operator != (const std::string_view other_) const {//#TODO templates would be nice here. But we don't want string_view by reference
            return !(*this == other_);
        }

        bool operator < (const r_string& other_) const {
            if (!data()) return false; //empty?
            return strcmp(data(), other_.data()) < 0;
        }

        bool operator > (const r_string& other_) const {
            if (!data()) return false; //empty?
            return strcmp(data(), other_.data()) > 0;
        }

        friend std::ostream& operator << (std::ostream& _os, const r_string& _s) {
            _os << _s.data();
            return _os;
        }

        friend std::istream& operator >> (std::istream& _in, r_string& _t) {
            char inp;
            std::string tmp;
            while (_in.get(inp)) {
                if (inp == 0) break;
                tmp.push_back(inp);
            }
            _t._ref = create(tmp.data(), tmp.length());
            return _in;
        }

        bool compare_case_sensitive(const char *other_) const {
            return !std::equal(_ref->cbegin(), _ref->cend(),
                compact_array<char>::const_iterator(other_), [](unsigned char l, unsigned char r) {return l == r; });
        }

        bool compare_case_insensitive(const char *other_) const {//#TODO string_view variant with length checking
            return !std::equal(_ref->cbegin(), _ref->cend(),
                compact_array<char>::const_iterator(other_), [](unsigned char l, unsigned char r) {return ::tolower(l) == ::tolower(r); });
        }

        size_t find(const char ch_, const size_t start_ = 0) const {
            if (length() == 0) return -1;
            const char *pos = strchr(_ref->data() + start_, ch_);
            if (pos == nullptr) return -1;
            return pos - _ref->data();
        }

        size_t find(const char *substring_, const size_t start_ = 0) const {
            if (_ref == nullptr || length() == 0) return -1;
            const char *pos = strstr(_ref->data() + start_, substring_);
            if (pos == nullptr) return -1;
            return pos - _ref->data();
        }
        void clear() {
            _ref = nullptr;
        }
        size_t hash() const noexcept {
            return std::hash<std::string_view>()(std::string_view(data(), _ref ? _ref->size() : 0u));
        }

        r_string append(std::string_view right_) const {
            const auto my_length = length();
            auto new_data = create(my_length + right_.length() + 1);//Space for terminating nullchar

            std::copy_n(begin(), my_length, new_data->begin());
            std::copy_n(right_.data(), right_.length(), new_data->begin() + my_length);

            new_data->data()[my_length + right_.length()] = 0;
            return r_string(new_data);
        }
        r_string& append_modify(std::string_view right_) {
            const auto my_length = length();
            auto newData = create(my_length + right_.length() + 1);//Space for terminating nullchar
            
            std::copy_n(data(), my_length, newData->begin());
            std::copy_n(right_.data(), right_.length(), newData->begin() + my_length);

            newData->data()[my_length + right_.length()] = 0;
            _ref = newData;
            return *this;
        }
        r_string operator+(const std::string_view right_) const {
            return append(right_);
        }
        r_string& operator+=(const std::string_view right_) {
            return append_modify(right_);
        }
        r_string& to_lower() {
            if (!_ref) return *this;
            make_mutable();

            std::transform(_ref->begin(), _ref->end(), _ref->begin(), ::tolower);

            return *this;
        }
        ///Be careful! This returns nullptr on empty string
        compact_array<char>::const_iterator begin() const noexcept {
            if (_ref)
                return _ref->begin();
            return {};
        }
        ///Be careful! This returns nullptr on empty string
        compact_array<char>::const_iterator end() const noexcept {
            if (_ref)
                return _ref->end();
            return {};
        }
        char front() const noexcept {
            if (_ref)
                return *_ref->begin();
            return '\0';
        }



    private:
        ref<compact_array<char>> _ref;

        static compact_array<char> *create(const char *str, const size_t len_) {
            if (len_ == 0 || *str == 0) return nullptr;
            compact_array<char> *string = compact_array<char>::create(len_ + 1);
        #if __GNUC__
            std::copy_n(str, len_, string->data());
        #else
            strncpy_s(string->data(), string->size(), str, len_);//#TODO better use memcpy? does strncpy_s check for null chars? we don't want that
        #endif
            string->data()[len_] = 0;
            return string;
        }

        static compact_array<char> *create(const size_t len_) {
            if (len_ == 0) return nullptr;
            compact_array<char> *string = compact_array<char>::create(len_ + 1);
            string->data()[0] = 0;
            return string;
        }

        static compact_array<char> *create(const char *str_) {
            if (*str_ == 0) return nullptr;
            return create(str_, strlen(str_));
        }
    };

    //constexpr r_string tests
    //#define r_string_literal(str) \
            //    ([]() -> const r_string_const<sizeof(str)- 1>& { \
        //        static const r_string_const<sizeof(str)- 1> literal{ str }; \
        //        return literal; \
        //    }())

//constexpr const r_string& operator ""_rs(const char* __str, size_t len) {
//    return r_string_literal(__str);
//};

//Contributed by ArmaDebugEngine
    class rv_arraytype {};

    template<class Type>
    class rv_array : public rv_arraytype {
    protected:
        Type *_data;
        int _n;
    public:
        class const_iterator;

        class iterator {
            friend class const_iterator;
            Type* p_;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = Type;
            using difference_type = ptrdiff_t;
            using pointer = Type*;
            using reference = Type&;

            iterator() : p_(nullptr) {}
            explicit iterator(Type* p) : p_(p) {}
            iterator(const iterator& other) : p_(other.p_) {}
            iterator& operator=(const iterator& other) { p_ = other.p_; return *this; }

            iterator& operator++() { ++p_; return *this; } // prefix++
            iterator  operator++(int) { iterator tmp(*this); ++(*this); return tmp; } // postfix++
            iterator& operator--() { --p_; return *this; } // prefix--
            iterator  operator--(int) { iterator tmp(*this); --(*this); return tmp; } // postfix--

            void     operator+=(const std::size_t& n) { p_ += n; }
            void     operator+=(const iterator& other) { p_ += other.p_; }
            iterator operator+ (const std::size_t& n) const { iterator tmp(*this); tmp += n; return tmp; }
            iterator operator+ (const iterator& other) const { iterator tmp(*this); tmp += other; return tmp; }

            void        operator-=(const std::size_t& n) { p_ -= n; }
            void        operator-=(const iterator& other) { p_ -= other.p_; }
            iterator    operator- (const std::size_t& n) const { iterator tmp(*this); tmp -= n; return tmp; }
            std::size_t operator- (const iterator& other) const { return p_ - other.p_; }

            bool operator< (const iterator& other) const { return (p_ - other.p_) < 0; }
            bool operator<=(const iterator& other) const { return (p_ - other.p_) <= 0; }
            bool operator> (const iterator& other) const { return (p_ - other.p_) > 0; }
            bool operator>=(const iterator& other) const { return (p_ - other.p_) >= 0; }
            bool operator==(const iterator& other) const { return  p_ == other.p_; }
            bool operator!=(const iterator& other) const { return  p_ != other.p_; }

            Type& operator*() const { return *p_; }
            Type* operator->() { return  p_; }
            explicit operator Type*() { return p_; }
        };
        class const_iterator {
            const Type* p_;
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = Type;
            using difference_type = ptrdiff_t;
            using pointer = Type *;
            using reference = Type &;

            const_iterator() : p_(nullptr) {}
            explicit const_iterator(const Type* p) noexcept : p_(p) {}
            const_iterator(const typename rv_array<Type>::iterator& other) : p_(other.p_) {}
            const_iterator(const const_iterator& other) noexcept : p_(other.p_) {}
            const_iterator& operator=(const const_iterator& other) { p_ = other.p_; return other; }
            const_iterator& operator=(const typename rv_array<Type>::iterator& other) { p_ = other.p_; return other; }

            const_iterator& operator++() noexcept { ++p_; return *this; } // prefix++
            const_iterator  operator++(int) { const_iterator tmp(*this); ++(*this); return tmp; } // postfix++
            const_iterator& operator--() noexcept { --p_; return *this; } // prefix--
            const_iterator  operator--(int) { const_iterator tmp(*this); --(*this); return tmp; } // postfix--

            void           operator+=(const std::size_t& n) { p_ += n; }
            void           operator+=(const const_iterator& other) { p_ += other.p_; }
            const_iterator operator+ (const std::size_t& n)        const { const_iterator tmp(*this); tmp += n; return tmp; }
            const_iterator operator+ (const const_iterator& other) const { const_iterator tmp(*this); tmp += other; return tmp; }

            void           operator-=(const std::size_t& n) noexcept { p_ -= n; }
            void           operator-=(const const_iterator& other) noexcept { p_ -= other.p_; }
            const_iterator operator- (const std::size_t& n)        const { const_iterator tmp(*this); tmp -= n; return tmp; }
            std::size_t    operator- (const const_iterator& other) const noexcept { return p_ - other.p_; }

            bool operator< (const const_iterator& other) const noexcept { return (p_ - other.p_) < 0; }
            bool operator<=(const const_iterator& other) const noexcept { return (p_ - other.p_) <= 0; }
            bool operator> (const const_iterator& other) const noexcept { return (p_ - other.p_) > 0; }
            bool operator>=(const const_iterator& other) const noexcept { return (p_ - other.p_) >= 0; }
            bool operator==(const const_iterator& other) const noexcept { return  p_ == other.p_; }
            bool operator!=(const const_iterator& other) const noexcept { return  p_ != other.p_; }

            const Type& operator*()  const noexcept { return *p_; }
            const Type* operator->() const noexcept { return  p_; }
            explicit operator const Type*() const noexcept { return p_; }
        };


        constexpr rv_array() noexcept :_data(nullptr), _n(0) {}
        rv_array(rv_array<Type>&& move_) noexcept :_data(move_._data), _n(move_._n) { move_._data = nullptr; move_._n = 0; }
        Type &get(const size_t i_) {
            if (i_ > count()) throw std::out_of_range("rv_array access out of range");
            return _data[i_];
        }
        const Type &get(const size_t i_) const {
            if (i_ > count()) throw std::out_of_range("rv_array access out of range");
            return _data[i_];
        }
        Type &operator [] (const size_t i_) { return get(i_); }
        const Type &operator [] (const size_t i_) const { return get(i_); }
        Type *data() { return _data; }
        constexpr size_t count() const noexcept { return static_cast<size_t>(_n); }
        constexpr size_t size() const noexcept { return static_cast<size_t>(_n); }

        iterator begin() { if (!_data) return iterator(); return iterator(&get(0)); }
        iterator end() { if (!_data) return iterator(); return iterator(&get(_n)); }

        const_iterator cbegin() const { return const_iterator(&get(0)); }
        const_iterator cend() const { return const_iterator(&get(_n)); }

        const_iterator begin() const { return const_iterator(&get(0)); }
        const_iterator end() const { return const_iterator(&get(_n)); }

        const Type &front() const { return get(0); }
        const Type &back() const { return get(_n - 1); }

        Type &front() { return get(0); }
        Type &back() { return get(_n - 1); }

        bool is_empty() const { return _n == 0; }

        template <class Func>
        void for_each(const Func &f_) const {
            for (size_t i = 0; i < count(); i++) {
                f_(get(i));
            }
        }

        template <class Func>
        void for_each_backwards(const Func &f_) const { //This returns if Func returns true
            for (size_t i = count() - 1; i >= 0; i--) {
                if (f_(get(i))) return;
            }
        }

        template <class Func>
        void for_each(const Func &f_) {
            for (size_t i = 0; i < count(); i++) {
                f_(get(i));
            }
        }
        size_t hash() const {
            size_t _hash{ 0 };
            for (const auto& it : *this) {
                _hash ^= std::hash<Type>()(it) + 0x9e3779b9 + (_hash << 6) + (_hash >> 2);
            }
            return _hash;
        }
        template<typename FindType>
        iterator find(const FindType& find_) {
            return std::find(begin(), end(), find_);
        }
    };

    template<class Type, class Allocator = rv_allocator<Type>, size_t growthFactor = 32>
    class
    #ifdef _MSC_VER
        __declspec(empty_bases)
    #endif
        //#TODO try GCC maybe __attribute__((__packed__)) or #pragma pack. But be careful of the bool in local alloc
        auto_array : public rv_array<Type>, private Allocator {
        typedef rv_array<Type> base;
    protected:
        int _maxItems;
        Type *try_realloc(Type *old_, size_t n_) {
            Type *ret = Allocator::reallocate(old_, n_);
            return ret;
        }

        void reallocate(size_t size_) {

            if (_maxItems == size_) return;

            if (base::_n > static_cast<int>(size_)) {
                resize(size_);
                return;//resize calls reallocate and reallocates... Ugly.. I know
            }
            Type *newData = nullptr;
            if (base::_data && size_ > 0 && ((newData = try_realloc(base::_data, size_)))) {
                //if (size > _maxItems)//Don't null out new stuff if there is no new stuff
                //    std::fill(reinterpret_cast<uint32_t*>(&newData[_maxItems]), reinterpret_cast<uint32_t*>(&newData[size]), 0);
                _maxItems = static_cast<int>(size_);
                base::_data = newData;
                return;
            }
            newData = Allocator::create_uninitialized_array(size_);
            //memset(newData, 0, size * sizeof(Type));
            if (base::_data) {
            #ifdef __GNUC__
                memmove(newData, base::_data, base::_n * sizeof(Type));
            #else
                //std::uninitialized_move(begin(), end(), newData); //This might be cleaner. But still causes a move construct call where memmove just moves bytes.
                memmove_s(newData, size_ * sizeof(Type), base::_data, base::_n * sizeof(Type));
            #endif
                Allocator::deallocate(base::_data);
            }
            base::_data = newData;
            _maxItems = static_cast<int>(size_);

        }

        void grow(const size_t n_) {
        #undef max
            reallocate(_maxItems + std::max(n_, growthFactor));
        }
    public:
        using base::end;
        using base::begin;
        using base::cend;
        using base::cbegin;
        using iterator = typename base::iterator;
        using const_iterator = typename base::const_iterator;
        constexpr auto_array() noexcept : rv_array<Type>(), _maxItems(0) {}
        auto_array(const std::initializer_list<Type> &init_) : rv_array<Type>(), _maxItems(0) {
            insert(end(), init_.begin(), init_.end());
        }
        template<class _InIt>
        auto_array(_InIt first_, _InIt last_) : rv_array<Type>(), _maxItems(0) {
            insert(end(), first_, last_);
        }
        auto_array(const auto_array<Type> &copy_) : rv_array<Type>(), _maxItems(0) {
            insert(end(), copy_.begin(), copy_.end());
        }
        auto_array(auto_array<Type> &&move_) noexcept : rv_array<Type>(std::move(move_)), _maxItems(move_._maxItems) {
            move_._maxItems = 0;
        }
        ~auto_array() {
            if (base::_data == nullptr) return;
            resize(0);
        }
        void shrink_to_fit() {
            resize(base::_n);
        }
        auto_array& operator = (auto_array &&move_) noexcept {
            base::_n = move_._n;
            _maxItems = move_._maxItems;
            base::_data = move_._data;
            move_._data = nullptr;
            move_._n = 0;
            move_._maxItems = 0;
            return *this;
        }

        auto_array& operator = (const auto_array &copy_) {
            if (copy_._n) {
                insert(base::end(), copy_.begin(), copy_.end());
            }
            return *this;
        }

        void resize(const size_t n_) {
            if (static_cast<int>(n_) < base::_n) {
                for (int i = static_cast<int>(n_); i < base::_n; i++) {
                    (*this)[i].~Type();
                }
                base::_n = static_cast<int>(n_);
            }
            if (n_ == 0 && base::_data) {
                Allocator::deallocate(rv_array<Type>::_data);
                rv_array<Type>::_data = nullptr;
                return;
            }
            reallocate(n_);
        }
        void reserve(const size_t res_) {
            if (_maxItems < static_cast<int>(res_)) {
                grow(res_ - _maxItems);
            }
        }
        template<class... _Valty>
        iterator emplace(iterator where_, _Valty&&... val_) {
            if (where_ < base::begin() || where_ > base::end()) throw std::runtime_error("Invalid Iterator"); //WTF?!
            const size_t insertOffset = where_ - base::begin();
            auto previousEnd = base::_n;
            if (_maxItems < base::_n + 1) {
                grow(1);
            }
            auto& item = base::_data[base::_n];
            ::new (&item) Type(std::forward<_Valty>(val_)...);
            ++base::_n;

            std::rotate(base::begin() + insertOffset, base::begin() + previousEnd, base::end());

            return base::begin() + insertOffset;
        }
        template<class... _Valty>
        iterator emplace_back(_Valty&&... val_) {
            if (_maxItems < base::_n + 1) {
                grow(1);
            }
            auto& item = base::_data[base::_n];
            ::new (&item) Type(std::forward<_Valty>(val_)...);
            ++base::_n;
            return iterator(&item);
        }
        iterator push_back(const Type& val_) {
            return emplace_back(val_);
        }
        iterator push_back(Type&& val_) {
            return emplace_back(std::move(val_));
        }

        //void erase(int index) {
        //    if (index > base::_n) return;
        //    auto item = (*this)[index];
        //    item.~Type();
        //    memmove_s(&(*this)[index], (base::_n - index) * sizeof(Type), &(*this)[index + 1], (base::_n - index - 1) * sizeof(Type));
        //}
        void erase(const_iterator element_) {
            if (element_ < base::begin() || element_ > base::end()) throw std::runtime_error("Invalid Iterator");
            const size_t index = std::distance(base::cbegin(), element_);
            if (static_cast<int>(index) > base::_n) return;
            auto item = (*this)[index];
            item.~Type();
        #ifdef __GNUC__
            memmove(&(*this)[index], &(*this)[index + 1], (base::_n - index - 1) * sizeof(Type));
        #else
            memmove_s(&(*this)[index], (base::_n - index) * sizeof(Type), &(*this)[index + 1], (base::_n - index - 1) * sizeof(Type));
        #endif
            --base::_n;
        }

        void erase(const_iterator first_, const_iterator last_) {
            if (first_ > last_ || first_ < base::begin() || last_ > base::end()) throw std::runtime_error("Invalid Iterator");
            const size_t firstIndex = std::distance(base::cbegin(), first_);
            const size_t lastIndex = std::distance(base::cbegin(), last_);
            const size_t range = std::distance(first_, last_);

            for (size_t index = firstIndex; index < lastIndex; ++index) {
                auto item = (*this)[index];
                item.~Type();
            }
            if (last_ != end()) {
            #ifdef __GNUC__
                memmove(&(*this)[firstIndex], &(*this)[lastIndex + 1], (base::_n - lastIndex - 1) * sizeof(Type));
            #else
                memmove_s(&(*this)[firstIndex], (base::_n - firstIndex) * sizeof(Type), &(*this)[lastIndex + 1], (base::_n - lastIndex - 1) * sizeof(Type));
            #endif
            }
            base::_n -= range;
        }

        void erase(const uint32_t index_, const uint32_t count_ = 1) {
            if (count_ == 1)
                erase(begin() + index_);
            else if (count_ > 1)
                erase(begin() + index_, begin() + index_ + (count_ - 1));
        }

        //This is sooo not threadsafe!
        template<class _InIt>
        iterator insert(iterator _where, _InIt _first, _InIt _last) {
            if (_first == _last) return _where; //Boogie!
            if (_where < base::begin() || _where > base::end()) throw std::runtime_error("Invalid Iterator"); //WTF?!
            const size_t insertOffset = std::distance(base::begin(), _where);
            const size_t previousEnd = static_cast<size_t>(base::_n);
            const size_t oldSize = base::count();
            const size_t insertedSize = std::distance(_first, _last);
            reserve(oldSize + insertedSize);

            auto index = base::_n;
            for (; _first != _last; ++_first) {
                //emplace_back(*_first);
                //custom inlined version of emplace_back. No capacity checks and only incrementing _n once.
                auto& item = base::_data[index];
                ::new (&item) Type(std::forward<decltype(*_first)>(*_first));
                ++index;
            }
            base::_n = index;

            std::rotate(base::begin() + insertOffset, base::begin() + previousEnd, base::end());
            return base::begin() + insertOffset;
        }
        void clear() {
            if (base::_data)
                Allocator::deallocate(rv_array<Type>::_data);
            base::_n = 0;
            _maxItems = 0;
        }

        bool operator==(rv_array<Type> other_) {
            if (other_._n != base::_n || ((base::_data == nullptr || other_._data == nullptr) && base::_data != other_._data))
                return false;
            auto index = 0;
            for (auto& it : other_) {
                if ((*this[index]) != it)
                    return false;
                ++index;
            }
            return true;
        }
    };

    template <class Type>
    struct find_key_array_traits {
        using key_type = const Type&;
        static bool equals(key_type a_, key_type b_) { return a_ == b_; }
        static key_type get_key(const Type &a_) { return a_; }
    };

    template<class Type, class Traits = find_key_array_traits<Type>>
    class find_key_array : public auto_array<Type> {
        using base = auto_array<Type>;
        using key_type = typename Traits::key_type;
    public:
        void delete_at(uint32_t index_) { base::erase(index_, 1); }
        void delete_at(uint32_t index_, uint32_t count_) { base::erase(index_, count_); }
        bool delete_by_key(key_type key_) {
            const int index = find_by_key(key_);
            if (index < 0) return false;
            delete_at(index);
            return true;
        }

        void delete_all_by_key(key_type key_) {
            base::erase(std::remove_if(base::begin(), base::end(), [key_](const Type& el) {
                return Traits::equals(Traits::get_key(el), key_);
            }), base::end());
        }

        std::optional<uint32_t> find(const Type& elem_) const {
            return find_by_key(Traits::get_key(elem_));
        }
        std::optional<uint32_t> find_by_key(key_type key_) const {
            for (int i = 0; i < base::Size(); i++)
                if (Traits::equals(Traits::get_key(base::get(i)), key_)) return i;
            return {};
        }

        std::optional<uint32_t> push_back_unique(const Type& src_) {
            if (find(src_)) return -1;
            return this->push_back(src_);
        }

        uint32_t find_or_push_back(const Type& src_) {
            auto index = find(src_);
            if (index) return index;
            return this->push_back(src_);
        }
    };

    template <class Type>
    struct reference_array_find_key_array_traits {
        using key_type = const Type&;
        static bool equals(key_type a_, key_type b_) { return a_ == b_; }
        static key_type get_key(const Type &a_) { return a_.get(); }
    };

    template <class Type>
    class reference_array : public find_key_array<ref<Type>, reference_array_find_key_array_traits<Type>> {
        using base = find_key_array <ref<Type>, reference_array_find_key_array_traits<Type>>;
        using traits = reference_array_find_key_array_traits<Type>;
    public:
        using base::delete_at;

        bool delete_at(const Type* el_) { return base::delete_by_key(el_); }
        bool delete_at(const ref<Type>& src_) const { return base::delete_by_key(traits::get_key(src_)); }


        bool find(const Type* el_) { return base::find_by_key(el_); }
        bool find(const ref<Type>& src_) const { return base::find_by_key(traits::get_key(src_)); }
    };



    static unsigned int rv_map_hash_string_case_sensitive(const char *key, int hashValue = 0) noexcept {
        while (*key) {
            hashValue = hashValue * 33 + static_cast<unsigned char>(*key++);
        }
        return hashValue;
    }
    static unsigned int rv_map_hash_string_case_insensitive(const char *key, int hashValue = 0) noexcept {
        while (*key) {
            hashValue = hashValue * 33 + static_cast<unsigned char>(tolower(*key++));
        }
        return hashValue;
    }

    struct map_string_to_class_trait {
        static unsigned int hash_key(const char * key) noexcept {
            return rv_map_hash_string_case_sensitive(key);
        }
        static bool compare_keys(const char * k1, const char * k2) noexcept {
            return strcmp(k1, k2) == 0;
        }
    };

    struct map_string_to_class_trait_caseinsensitive : map_string_to_class_trait {
        static unsigned int hash_key(const char * key) noexcept {
            return rv_map_hash_string_case_insensitive(key);
        }

        static bool compare_keys(const char * k1, const char * k2) noexcept {
        #ifdef __GNUC__
            return std::equal(k1, k1 + strlen(k1),
                k2, [](unsigned char l, unsigned char r) {return l == r || tolower(l) == tolower(r); });
        #else
            return _strcmpi(k1, k2) == 0;
        #endif
        }
    };

    template <class Type, class Container, class Traits = map_string_to_class_trait>
    class map_string_to_class {
    protected:
        Container* _table;
        int _tableCount{ 0 };
        int _count{ 0 };
        static Type _null_entry;
    public:

        class iterator {
            size_t _table;
            size_t _item;
            const map_string_to_class<Type, Container, Traits> *_map;
            size_t number_of_tables() {
                return _map->_table ? static_cast<size_t>(_map->_tableCount) : 0u;
            }
            void get_next() {
                while (_table < number_of_tables() && _item >= _map->_table[_table].count()) {
                    _table++;
                    _item = 0;
                }
            }
        public:
            iterator(const map_string_to_class<Type, Container, Traits> &base_) {
                _table = 0; _item = 0; _map = &base_;
                get_next();
            }
            iterator(const map_string_to_class<Type, Container, Traits> &base_, bool) { //Creates end Iterator
                _item = 0; _map = &base_;
                _table = number_of_tables();
            }
            iterator& operator++ () {
                if (_table >= number_of_tables()) return *this;
                ++_item;
                get_next();
                return *this;
            }
            iterator operator++(int) {
                iterator _tmp = *this;
                ++*this;
                return (_tmp);
            }
            bool operator==(const iterator& _other) {
                return _table == _other._table && _item == _other._item;
            }
            bool operator!=(const iterator& _other) {
                return _table != _other._table || _item != _other._item;
            }
            const Type &operator * () const {
                return _map->_table[_table][_item];
            }
            const Type *operator-> () const {
                return &_map->_table[_table][_item];
            }
            Type &operator * () {
                return _map->_table[_table][_item];
            }
            Type *operator-> () {
                return &_map->_table[_table][_item];
            }
        };
        iterator begin() {
            return iterator(*this);
        }
        iterator end() {
            return iterator(*this, true);
        }
        const iterator begin() const {
            return iterator(*this);
        }
        const iterator end() const {
            return iterator(*this, true);
        }

        map_string_to_class() {}

        template <class Func>
        void for_each(Func func_) const {
            if (!_table || !_count) return;
            for (int i = 0; i < _tableCount; i++) {
                const Container &container = _table[i];
                for (int j = 0; j < container.count(); j++) {
                    func_(container[j]);
                }
            }
        }

        template <class Func>
        void for_each_backwards(Func func_) const {
            if (!_table || !_count) return;
            for (int i = _tableCount - 1; i >= 0; i--) {
                const Container &container = _table[i];
                for (int j = container.count() - 1; j >= 0; j--) {
                    func_(container[j]);
                }
            }
        }

        const Type &get(const char* key_) const {
            if (!_table || !_count) return _null_entry;
            const int hashed_key = hash_key(key_);
            for (int i = 0; i < _table[hashed_key].count(); i++) {
                const Type &item = _table[hashed_key][i];
                if (Traits::compare_keys(item.get_map_key(), key_))
                    return item;
            }
            return _null_entry;
        }

        Container* get_table_for_key(const char* key_) {
            if (!_table || !_count) return nullptr;
            const int hashed_key = hash_key(key_);
            return &_table[hashed_key];
        }

        Type &get(const char* key_) {
            if (!_table || !_count) return _null_entry;
            const int hashed_key = hash_key(key_);
            for (size_t i = 0; i < _table[hashed_key].count(); i++) {
                Type &item = _table[hashed_key][i];
                if (Traits::compare_keys(item.get_map_key(), key_))
                    return item;
            }
            return _null_entry;
        }

        static bool is_null(const Type &value_) { return &value_ == &_null_entry; }

        bool has_key(const char* key_) const {
            return !is_null(get(key_));
        }

        int count() const { return _count; }


        //ArmaDebugEngine
        void rebuild(int tableSize) {
            auto oldTableCount = _tableCount;
            _tableCount = tableSize;
            auto newTable = rv_allocator<Container>::create_array(tableSize);
            for (auto i = 0; i < _tableCount; i++) {
                auto& container = newTable[i];
                for (Type& item : container) {
                    auto hashedKey = hash_key(item.get_map_key());
                    auto it = newTable[hashedKey].emplace(newTable[hashedKey].end(), Type());
                    *it = std::move(item);
                }
            }
            std::swap(_table, newTable);
            rv_allocator<Container>::destroy_deallocate(newTable, oldTableCount);
        }

        Type& insert(const Type &value) {
            //Check if key already exists
            auto key = value.get_map_key();
            Type &old = get(key);
            if (!is_null(old)) {
                return old;
            }

            //Are we full?
            if (_count + 1 > (16 * _tableCount)) {
                int tableSize = _tableCount + 1;
                do {
                    tableSize *= 2;
                } while (_count + 1 > (16 * (tableSize - 1)));
                rebuild(tableSize - 1);
            }
            auto hashedkey = hash_key(key);
            auto &x = *(_table[hashedkey].emplace_back(value));
            _count++;
            return x;
        }

        Type& insert(Type &&value) {
            //Check if key already exists
            auto key = value.get_map_key();
            Type &old = get(key);
            if (!is_null(old)) {
                return old;
            }

            //Are we full?
            if (_count + 1 > (16 * _tableCount)) {
                int tableSize = _tableCount + 1;
                do {
                    tableSize *= 2;
                } while (_count + 1 > (16 * (tableSize - 1)));
                rebuild(tableSize - 1);
            }

            if (!_table) {
                _table = rv_allocator<Container>::create_array(_tableCount);
            }

            auto hashedkey = hash_key(key);
            auto &x = *(_table[hashedkey].emplace_back(std::move(value)));
            _count++;
            return x;
        }



    protected:
        int hash_key(const char* key_) const {
            return Traits::hash_key(key_) % _tableCount;
        }
    };

    template<class Type, class Container, class Traits>
    Type map_string_to_class<Type, Container, Traits>::_null_entry;


#pragma endregion

}

#pragma pop_macro("min")
#pragma pop_macro("max")
