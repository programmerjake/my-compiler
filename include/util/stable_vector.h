/* Copyright (c) 2015 Jacob R. Lifshay
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
#ifndef STABLE_VECTOR_H_INCLUDED
#define STABLE_VECTOR_H_INCLUDED

#include <iterator>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <cassert>

template <typename T>
class stable_vector final
{
public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
private:
    struct base_node_type;
    struct cell_type final
    {
        base_node_type *node;
        explicit constexpr cell_type(base_node_type *node = nullptr)
            : node(node)
        {
        }
    };
    struct base_node_type
    {
        cell_type *cell;
        explicit constexpr base_node_type(cell_type *cell = nullptr)
            : cell(cell)
        {
        }
    };
    struct node_type final : public base_node_type
    {
        T value;
        template <typename ...Args>
        node_type(Args &&...args)
            : value(std::forward<Args>(args)...)
        {
        }
    };
    struct end_node_type final : public base_node_type
    {
        using base_node_type::base_node_type;
    };
    cell_type *cells;
    size_type allocated, used;
    end_node_type end_node;
    cell_type empty_cell;
    size_type get_new_size(size_type needed_size)
    {
        size_type retval = allocated + allocated / 2;
        if(retval <= needed_size)
            return needed_size;
        return retval;
    }
    void ensure_size(size_type needed_size)
    {
        if(needed_size >= allocated && needed_size > 0)
        {
            size_type new_allocated = get_new_size(needed_size + 1);
            cell_type *new_cells = new cell_type[new_allocated];
            for(size_type i = 0; i < used; i++)
            {
                new_cells[i].node = cells[i].node;
                new_cells[i].node->cell = &new_cells[i];
            }
            new_cells[used].node = &end_node;
            end_node.cell = &new_cells[used];
            if(cells != &empty_cell)
                delete []cells;
            cells = new_cells;
            allocated = new_allocated;
        }
    }
    void make_space_for_insert(size_type space_start, size_type space_size)
    {
        if(space_size == 0)
            return;
        if(used + space_size >= allocated)
        {
            size_type new_allocated = get_new_size(used + space_size + 1);
            cell_type *new_cells = new cell_type[new_allocated];
            for(size_type i = 0; i < space_start; i++)
            {
                new_cells[i].node = cells[i].node;
                new_cells[i].node->cell = &new_cells[i];
            }
            for(size_type i = space_start; i < used; i++)
            {
                size_type j = i + space_size;
                new_cells[j].node = cells[i].node;
                new_cells[j].node->cell = &new_cells[j];
            }
            if(cells != &empty_cell)
                delete []cells;
            used += space_size;
            new_cells[used].node = &end_node;
            end_node.cell = &new_cells[used];
            cells = new_cells;
            allocated = new_allocated;
        }
        else if(used == 0)
        {
            used = space_size;
            cells[used].node = &end_node;
            end_node.cell = &cells[used];
        }
        else
        {
            for(size_type j = used - 1 + space_size; j >= space_start + space_size; j--)
            {
                size_type i = j - space_size;
                cells[j].node = cells[i].node;
                cells[j].node->cell = &cells[j];
            }
            used += space_size;
            cells[used].node = &end_node;
            end_node.cell = &cells[used];
        }
    }
    void remove_space_after_erase(size_type space_start, size_type space_size)
    {
        if(space_size == 0)
            return;
        for(size_type i = space_start + space_size; i < used; i++)
        {
            size_type j = i - space_size;
            cells[j].node = cells[i].node;
            cells[j].node->cell = &cells[j];
        }
        used -= space_size;
        cells[used].node = &end_node;
        end_node.cell = &cells[used];
    }
public:
    constexpr stable_vector()
        : cells(&empty_cell), allocated(1), used(0), end_node(&empty_cell), empty_cell(&end_node)
    {
    }
    explicit stable_vector(size_type count)
        : stable_vector()
    {
        resize(count);
    }
    stable_vector(size_type count, const T &value)
        : stable_vector()
    {
        resize(count, value);
    }
    stable_vector(const stable_vector &rt)
        : stable_vector()
    {
        reserve(rt.size());
        for(const T &value : rt)
            push_back(value);
    }
    stable_vector(stable_vector &&rt)
        : stable_vector()
    {
        if(rt.cells == &rt.empty_cell)
            return;
        cells = rt.cells;
        allocated = rt.allocated;
        used = rt.used;

        rt.cells = &rt.empty_cell;
        rt.empty_cell.node = &rt.end_node;
        rt.end_node.cell = &rt.cells[0];
        rt.allocated = 1;
        rt.used = 0;
        cells[used].node = &end_node;
        end_node.cell = &cells[used];
    }
    ~stable_vector()
    {
        clear();
        if(cells != &empty_cell)
            delete []cells;
    }
    stable_vector &operator =(const stable_vector &rt)
    {
        if(this == &rt)
            return *this;
        clear();
        reserve(rt.size());
        for(const T &value : rt)
            push_back(value);
        return *this;
    }
    stable_vector &operator =(stable_vector &&rt)
    {
        clear();
        swap(rt);
        return *this;
    }
    void assign(size_type count, const T &value)
    {
        clear();
        resize(count, value);
    }
    reference at(size_type index)
    {
        if(index >= used)
            throw std::out_of_range("index out of range in stable_vector::at");
        return static_cast<node_type *>(cells[index].node)->value;
    }
    const_reference at(size_type index) const
    {
        if(index >= used)
            throw std::out_of_range("index out of range in stable_vector::at");
        return static_cast<const node_type *>(cells[index].node)->value;
    }
    reference operator [](size_type index)
    {
        assert(index < used);
        return static_cast<node_type *>(cells[index].node)->value;
    }
    const_reference operator [](size_type index) const
    {
        assert(index < used);
        return static_cast<const node_type *>(cells[index].node)->value;
    }
    reference front()
    {
        return operator [](0);
    }
    const_reference front() const
    {
        return operator [](0);
    }
    reference back()
    {
        assert(used > 0);
        return operator [](used - 1);
    }
    const_reference back() const
    {
        assert(used > 0);
        return operator [](used - 1);
    }
    class const_iterator;
    class iterator final : public std::iterator<std::random_access_iterator_tag, T, difference_type, pointer, reference>
    {
        friend class stable_vector;
        friend class const_iterator;
    private:
        base_node_type *node = nullptr;
        iterator(base_node_type *node)
            : node(node)
        {
        }
    public:
        constexpr iterator() = default;
        iterator &operator ++()
        {
            node = node->cell[1].node;
            return *this;
        }
        iterator &operator --()
        {
            node = node->cell[-1].node;
            return *this;
        }
        iterator operator ++(int)
        {
            iterator retval = *this;
            operator ++();
            return retval;
        }
        iterator operator --(int)
        {
            iterator retval = *this;
            operator --();
            return retval;
        }
        bool operator ==(const iterator &rt) const
        {
            return node == rt.node;
        }
        bool operator !=(const iterator &rt) const
        {
            return node != rt.node;
        }
        reference operator *() const
        {
            return static_cast<node_type *>(node)->value;
        }
        pointer operator ->() const
        {
            return &static_cast<node_type *>(node)->value;
        }
        iterator &operator +=(difference_type v)
        {
            if(node != nullptr)
                node = node->cell[v].node;
            return *this;
        }
        iterator &operator -=(difference_type v)
        {
            if(node != nullptr)
                node = node->cell[-v].node;
            return *this;
        }
        friend iterator operator +(difference_type l, iterator r)
        {
            r += l;
            return r;
        }
        iterator operator +(difference_type r) const
        {
            if(node == nullptr)
                return iterator();
            return iterator(node->cell[r].node);
        }
        iterator operator -(difference_type r) const
        {
            if(node == nullptr)
                return iterator();
            return iterator(node->cell[-r].node);
        }
        difference_type operator -(iterator r) const
        {
            if(node == r.node) // also checks for null nodes
                return 0;
            return node->cell - r.node->cell;
        }
        reference operator [](difference_type r) const
        {
            return static_cast<node_type *>(node->cell[r].node)->value;
        }
        bool operator <(iterator r) const
        {
            if(node == r.node) // also checks for null nodes
                return false;
            return node->cell < r.node->cell;
        }
        bool operator >(iterator r) const
        {
            if(node == r.node) // also checks for null nodes
                return false;
            return node->cell > r.node->cell;
        }
        bool operator <=(iterator r) const
        {
            if(node == r.node) // also checks for null nodes
                return true;
            return node->cell <= r.node->cell;
        }
        bool operator >=(iterator r) const
        {
            if(node == r.node) // also checks for null nodes
                return true;
            return node->cell >= r.node->cell;
        }
    };
    class const_iterator final : public std::iterator<std::random_access_iterator_tag, const T, difference_type, const_pointer, const_reference>
    {
        friend class stable_vector;
    private:
        base_node_type *node = nullptr;
        const_iterator(base_node_type *node)
            : node(node)
        {
        }
    public:
        constexpr const_iterator() = default;
        constexpr const_iterator(iterator v)
            : node(v.node)
        {
        }
        const_iterator &operator ++()
        {
            node = node->cell[1].node;
            return *this;
        }
        const_iterator &operator --()
        {
            node = node->cell[-1].node;
            return *this;
        }
        const_iterator operator ++(int)
        {
            const_iterator retval = *this;
            operator ++();
            return retval;
        }
        const_iterator operator --(int)
        {
            const_iterator retval = *this;
            operator --();
            return retval;
        }
        friend bool operator ==(const const_iterator &l, const const_iterator &r)
        {
            return l.node == r.node;
        }
        friend bool operator !=(const const_iterator &l, const const_iterator &r)
        {
            return l.node != r.node;
        }
        const_reference operator *() const
        {
            return static_cast<const node_type *>(node)->value;
        }
        const_pointer operator ->() const
        {
            return &static_cast<const node_type *>(node)->value;
        }
        const_iterator &operator +=(difference_type v)
        {
            if(node != nullptr)
                node = node->cell[v].node;
            return *this;
        }
        const_iterator &operator -=(difference_type v)
        {
            if(node != nullptr)
                node = node->cell[-v].node;
            return *this;
        }
        friend const_iterator operator +(difference_type l, const_iterator r)
        {
            r += l;
            return r;
        }
        friend const_iterator operator +(const_iterator l, difference_type r)
        {
            if(l.node == nullptr)
                return const_iterator();
            return const_iterator(l.node->cell[r].node);
        }
        friend const_iterator operator -(const_iterator l, difference_type r)
        {
            if(l.node == nullptr)
                return const_iterator();
            return const_iterator(l.node->cell[-r].node);
        }
        friend difference_type operator -(const_iterator l, const_iterator r)
        {
            if(l.node == r.node) // also checks for null nodes
                return 0;
            return l.node->cell - r.node->cell;
        }
        const_reference operator [](difference_type r) const
        {
            return static_cast<const node_type *>(node->cell[r])->value;
        }
        friend bool operator <(const_iterator l, const_iterator r)
        {
            if(l.node == r.node) // also checks for null nodes
                return false;
            return l.node->cell < r.node->cell;
        }
        friend bool operator >(const_iterator l, const_iterator r)
        {
            if(l.node == r.node) // also checks for null nodes
                return false;
            return l.node->cell > r.node->cell;
        }
        friend bool operator <=(const_iterator l, const_iterator r)
        {
            if(l.node == r.node) // also checks for null nodes
                return true;
            return l.node->cell <= r.node->cell;
        }
        friend bool operator >=(const_iterator l, const_iterator r)
        {
            if(l.node == r.node) // also checks for null nodes
                return true;
            return l.node->cell >= r.node->cell;
        }
    };
    iterator begin()
    {
        return iterator(cells[0].node);
    }
    const_iterator begin() const
    {
        return const_iterator(cells[0].node);
    }
    const_iterator cbegin() const
    {
        return const_iterator(cells[0].node);
    }
    iterator end()
    {
        return iterator(&end_node);
    }
    const_iterator end() const
    {
        return const_iterator(&end_node);
    }
    const_iterator cend() const
    {
        return const_iterator(&end_node);
    }
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    reverse_iterator rbegin()
    {
        return reverse_iterator(end());
    }
    reverse_iterator rbegin() const
    {
        return reverse_iterator(cend());
    }
    reverse_iterator crbegin() const
    {
        return reverse_iterator(cend());
    }
    reverse_iterator rend()
    {
        return reverse_iterator(begin());
    }
    reverse_iterator rend() const
    {
        return reverse_iterator(cbegin());
    }
    reverse_iterator crend() const
    {
        return reverse_iterator(cbegin());
    }
    bool empty() const
    {
        return used == 0;
    }
    size_type size() const
    {
        return used;
    }
    size_type capacity() const
    {
        return allocated - 1;
    }
    void reserve(size_type n)
    {
        ensure_size(n + 1);
    }
    void clear()
    {
        erase(begin(), end());
    }
    iterator insert(const_iterator pos, const T &value)
    {
        insert(pos, 1, value);
        return iterator(pos.node) - 1;
    }
    iterator insert(const_iterator pos, T &&value)
    {
        size_type index = pos - begin();
        assert(index <= used);
        make_space_for_insert(index, 1);
        try
        {
            node_type *node = new node_type(std::move(value));
            cells[index].node = node;
            node->cell = &cells[index];
            return iterator(node);
        }
        catch(...)
        {
            remove_space_after_erase(index, 1);
            throw;
        }
    }
    iterator insert(const_iterator pos, size_type count, const T &value)
    {
        if(count == 0)
            return iterator(pos.node);
        size_type index = pos - begin();
        assert(index <= used);
        make_space_for_insert(index, count);
        for(size_type i = 0; i < count; i++)
        {
            try
            {
                node_type *node = new node_type(value);
                cells[index + i].node = node;
                node->cell = &cells[index + i];
            }
            catch(...)
            {
                remove_space_after_erase(index + i, count - i);
                throw;
            }
        }
        return iterator(cells[index].node);
    }
    template <typename ...Args>
    iterator emplace(const_iterator pos, Args &&...args)
    {
        size_type index = pos - begin();
        assert(index <= used);
        make_space_for_insert(index, 1);
        try
        {
            node_type *node = new node_type(std::forward<Args>(args)...);
            cells[index].node = node;
            node->cell = &cells[index];
            return iterator(node);
        }
        catch(...)
        {
            remove_space_after_erase(index, 1);
            throw;
        }
    }
    iterator erase(const_iterator pos)
    {
        return erase(pos, pos + 1);
    }
    iterator erase(const_iterator first, const_iterator last)
    {
        if(first == last)
            return iterator(last.node);
        size_type index = first - begin();
        size_type remove_count = last - first;
        for(size_type i = 0; i < remove_count; i++)
        {
            try
            {
                delete static_cast<node_type *>(cells[index + i].node);
            }
            catch(...)
            {
                remove_space_after_erase(index, i);
                throw;
            }
        }
        remove_space_after_erase(index, remove_count);
        return iterator(last.node);
    }
    void push_back(const T &value)
    {
        insert(end(), value);
    }
    void push_back(T &&value)
    {
        insert(end(), std::move(value));
    }
    template <typename ...Args>
    void emplace_back(Args &&...args)
    {
        emplace(end(), std::forward<Args>(args)...);
    }
    void pop_back()
    {
        erase(end() - 1, end());
    }
    void push_front(const T &value)
    {
        insert(begin(), value);
    }
    void push_front(T &&value)
    {
        insert(begin(), std::move(value));
    }
    template <typename ...Args>
    void emplace_front(Args &&...args)
    {
        emplace(begin(), std::forward<Args>(args)...);
    }
    void pop_front()
    {
        erase(begin());
    }
    void resize(size_type count)
    {
        if(count > used)
        {
            reserve(count);
            for(size_type i = used; i < count; i++)
                emplace_back();
        }
        else
            erase(begin() + count, end());
    }
    void resize(size_type count, const T &value)
    {
        if(count > used)
        {
            reserve(count);
            for(size_type i = used; i < count; i++)
                push_back(value);
        }
        else
            erase(begin() + count, end());
    }
    void swap(stable_vector &rt)
    {
        if(cells == &empty_cell)
        {
            if(rt.cells == &rt.empty_cell)
                return;
            cells = rt.cells;
            allocated = rt.allocated;
            used = rt.used;

            rt.cells = &rt.empty_cell;
            rt.empty_cell.node = &rt.end_node;
            rt.end_node.cell = &rt.cells[0];
            rt.allocated = 1;
            rt.used = 0;
            cells[used].node = &end_node;
            end_node.cell = &cells[used];
        }
        else if(rt.cells == &rt.empty_cell)
        {
            rt.cells = cells;
            rt.allocated = allocated;
            rt.used = used;

            cells = &empty_cell;
            empty_cell.node = &end_node;
            end_node.cell = &cells[0];
            allocated = 1;
            used = 0;
            rt.cells[rt.used].node = &rt.end_node;
            rt.end_node.cell = &rt.cells[rt.used];
        }
        else
        {
            cell_type *temp_cells = cells;
            cells = rt.cells;
            rt.cells = temp_cells;
            size_type temp_allocated = allocated;
            allocated = rt.allocated;
            rt.allocated = temp_allocated;
            size_type temp_used = used;
            used = rt.used;
            rt.used = temp_used;
            cells[used].node = &end_node;
            end_node.cell = &cells[used];
            rt.cells[rt.used].node = &rt.end_node;
            rt.end_node.cell = &rt.cells[rt.used];
        }
    }
    void splice(const_iterator pos, stable_vector &other)
    {
        reserve(size() + other.size());
        splice(pos, other, other.begin(), other.end());
    }
    void splice(const_iterator pos, stable_vector &&other)
    {
        reserve(size() + other.size());
        splice(pos, other, other.begin(), other.end());
    }
    void splice(const_iterator pos, stable_vector &other, const_iterator it)
    {
        if(&other == this && (it == pos || it + 1 == pos))
            return;
        size_type it_index = it - other.begin();
        assert(it_index < other.used);
        if(this == &other)
            other.remove_space_after_erase(it_index, 1);
        size_type pos_index = pos - begin();
        assert(pos_index <= used);
        make_space_for_insert(pos_index, 1);
        if(this != &other)
            other.remove_space_after_erase(it_index, 1);
        base_node_type *node = it.node;
        cells[pos_index].node = node;
        node->cell = &cells[pos_index];
    }
    void splice(const_iterator pos, stable_vector &&other, const_iterator it)
    {
        splice(pos, other, it);
    }
    void splice(const_iterator pos, stable_vector &other, const_iterator first, const_iterator last)
    {
        if(&other != this)
            reserve(size() + static_cast<size_type>(last - first));
        const_iterator iter = first;
        while(iter != last)
        {
            const_iterator next_iter = iter;
            ++next_iter;
            splice(pos, other, iter);
            iter = next_iter;
        }
    }
    void splice(const_iterator pos, stable_vector &&other, const_iterator first, const_iterator last)
    {
        splice(pos, other, first, last);
    }
};

#endif // STABLE_VECTOR_H_INCLUDED
