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
#ifndef RANDOM_ACCESS_LIST_H_INCLUDED
#define RANDOM_ACCESS_LIST_H_INCLUDED

#include <iterator>
#include <cstddef>
#include <utility>
#include <deque>

struct random_access_list_node_base
{
    random_access_list_node_base *left = this;
    random_access_list_node_base *right = this;
    random_access_list_node_base(const random_access_list_node_base &) = delete;
    random_access_list_node_base(random_access_list_node_base &&rt)
        : left(rt.left), right(rt.right)
    {
        rt.left = rt.right = &rt;
        if(left == &rt)
        {
            left = right = this;
        }
        else
        {
            left->right = this;
            right->left = this;
        }
    }
    random_access_list_node_base &operator =(const random_access_list_node_base &rt) = delete;
    random_access_list_node_base &operator =(random_access_list_node_base &&rt)
    {
        if(&rt == this)
            return *this;
        random_access_list_node_base *tleft = rt.left;
        random_access_list_node_base *tright = rt.right;
        rt.left = left;
        rt.right = right;
        left = tleft;
        right = tright;
        if(left == this)
        {
            left = right = this;
        }
        else
        {
            left->right = this;
            right->left = this;
        }
        if(rt.left == &rt)
        {
            rt.left = rt.right = &rt;
        }
        else
        {
            rt.left->right = &rt;
            rt.right->left = &rt;
        }
        return *this;
    }
    void insert_after(random_access_list_node_base *newNode)
    {
        newNode->left = this;
        newNode->right = right;
        right->left = newNode;
        right = newNode;
    }
    void remove()
    {
        left->right = right;
        right->left = left;
        left = right = this;
    }
};

struct random_access_list_node_iterator_implementation final
{
    random_access_list_node_base *node;
    explicit constexpr random_access_list_node_iterator_implementation(random_access_list_node_base *node = nullptr)
        : node(node)
    {
    }
    void incr()
    {
        node = node->next;
    }
    void decr()
    {
        node = node->prev;
    }
    bool operator ==(random_access_list_node_iterator_implementation r) const
    {
        return node == r.node;
    }
    bool operator !=(random_access_list_node_iterator_implementation r) const
    {
        return node != r.node;
    }
};

struct random_access_list_chunk;

typedef std::uint8_t random_access_list_chunk_index_type;

struct random_access_list_node_base_with_chunk : public random_access_list_node_base
{
    random_access_list_chunk *chunk = nullptr;
    random_access_list_chunk_index_type index = 0;
};

template <typename T>
struct random_access_list_node final : public random_access_list_node_base_with_chunk
{
    T value;
    template <typename ...Args>
    random_access_list_node(Args &&...args)
        : value(std::forward<Args>(args)...)
    {
    }
};

class random_access_list_base;

struct random_access_list_chunk final : public random_access_list_node_base
{
    typedef random_access_list_chunk_index_type index_type;
    static constexpr index_type array_size = 64;
    random_access_list_node_base_with_chunk *nodes[array_size] = {nullptr};
    index_type begin = array_size / 2;
    index_type end = array_size / 2;
    std::size_t chunks_index = 0;
    random_access_list_base *container;
    random_access_list_chunk(random_access_list_base *container)
        : container(container)
    {
    }
    static void move_node(random_access_list_chunk *source_chunk, index_type source_index, random_access_list_chunk *dest_chunk, index_type dest_index)
    {
        random_access_list_node_base_with_chunk *node = source_chunk->nodes[source_index];
        dest_chunk->nodes[dest_index] = node;
        if(dest_chunk != source_chunk)
            node->chunk = dest_chunk;
        node->index = dest_index;
    }
    bool empty() const
    {
        return end <= begin;
    }
    bool space_at_front() const
    {
        return empty() || begin > 0;
    }
    bool space_at_back() const
    {
        return empty() || end < array_size;
    }
    bool full() const
    {
        return !space_at_front() && !space_at_back();
    }
    std::size_t size() const
    {
        return end - begin;
    }
    bool good_position(std::ptrdiff_t position) const
    {
        return position >= (std::ptrdiff_t)begin && position < (std::ptrdiff_t)end;
    }
    std::size_t nodes_after(index_type position) const
    {
        return end - position - 1;
    }
    std::size_t nodes_before(index_type position) const
    {
        return position - begin;
    }
    bool push_front(random_access_list_node_base_with_chunk *node)
    {
        if(!space_at_front())
            return false;
        begin--;
        nodes[begin] = node;
        node->chunk = this;
        node->index = begin;
        return true;
    }
    bool push_back(random_access_list_node_base_with_chunk *node)
    {
        if(!space_at_back())
            return false;
        nodes[end] = node;
        node->chunk = this;
        node->index = end;
        end++;
        return true;
    }
    bool insert(random_access_list_node_base_with_chunk *node, index_type position)
    {
        if(position == begin && space_at_front())
        {
            return push_front(node);
        }
        if(position == end && space_at_back())
        {
            return push_back(node);
        }
        index_type front_distance = position - begin;
        index_type back_distance = end - position;
        if((front_distance < back_distance || !space_at_back()) && space_at_front())
        {
            for(index_type i = begin; i < position; i++)
                move_node(this, i, this, i - 1);
            begin--;
            nodes[position - 1] = node;
            node->chunk = this;
            node->index = position - 1;
        }
        else if(space_at_back())
        {
            end++;
            for(index_type i = end; i > position; i--)
                move_node(this, i - 2, this, i - 1);
            nodes[position] = node;
            node->chunk = this;
            node->index = position;
        }
        else
            return false;
    }
    void pop_front()
    {
        begin++;
    }
    void pop_back()
    {
        end--;
    }
    void erase(index_type position)
    {
        if(position == begin)
        {
            pop_front();
        }
        else if(position + 1 == end)
        {
            pop_back();
        }
        else
        {
            index_type front_distance = nodes_before(position);
            index_type back_distance = nodes_after(position);
            if(front_distance < back_distance)
            {
                for(index_type i = position; i > begin; i--)
                    move_node(this, i - 1, this, i);
                begin++;
            }
            else
            {
                end--;
                for(index_type i = position; i < end; i++)
                    move_node(this, i + 1, this, i);
            }
        }
    }
    void clear()
    {
        begin = end = array_size / 2;
    }
};

class random_access_list_base
{
protected:
    random_access_list_node_base chunks_end_node;
    random_access_list_node_base nodes_end_node;
    typedef std::deque<random_access_list_chunk *> chunks_type;
    chunks_type chunks;
    std::size_t node_count = 0;
public:
    random_access_list_base(random_access_list_base &&rt)
        : chunks_end_node(std::move(rt.chunks_end_node)), nodes_end_node(std::move(rt.nodes_end_node)), chunks(std::move(rt.chunks)), node_count(rt.node_count)
    {
        rt.chunks = chunks_type();
        rt.node_count = 0;
    }
protected:
    void push_back_chunk(random_access_list_chunk *node)
    {
        node->chunks_index = chunks.size();
        chunks.push_back(node);
    }
    void push_front_chunk(random_access_list_chunk *node)
    {
        chunks.push_front(node);
        std::size_t i = 0;
        for(random_access_list_chunk *c : chunks)
        {
            c->chunks_index = i++;
        }
    }
    void insert_chunk(random_access_list_chunk *node, std::size_t index)
    {
        for(auto iter = chunks.insert(chunks.begin() + index, node); iter != chunks.end(); ++iter, index++)
        {
            random_access_list_chunk *c = *iter;
            c->chunks_index = index;
        }
    }
    void erase_chunk(std::size_t index)
    {
        for(auto iter = chunks.erase(chunks.begin() + index); iter != chunks.end(); ++iter, index++)
        {
            random_access_list_chunk *c = *iter;
            c->chunks_index = index;
        }
    }
    struct iterator_implementation final
    {
        random_access_list_node_iterator_implementation node_iterator;
        constexpr iterator_implementation()
        {
        }
        constexpr iterator_implementation(random_access_list_node_base *node)
            : node_iterator(node)
        {
        }
        bool operator ==(const iterator_implementation &rt) const
        {
            return node_iterator == rt.node_iterator;
        }
        bool operator !=(const iterator_implementation &rt) const
        {
            return node_iterator != rt.node_iterator;
        }
        random_access_list_node_base_with_chunk *get_base_node() const
        {
            return static_cast<random_access_list_node_base_with_chunk *>(node_iterator.node);
        }
        void incr()
        {
            node_iterator.incr();
        }
        void decr()
        {
            node_iterator.decr();
        }
        std::ptrdiff_t operator -(iterator_implementation r) const
        {
            iterator_implementation l = *this;
            if(l == r)
                return 0;
            random_access_list_chunk *l_chunk = l.get_base_node()->chunk;
            random_access_list_chunk *r_chunk = r.get_base_node()->chunk;
            random_access_list_base *container = l_chunk->container;
            std::ptrdiff_t l_index = l.get_base_node()->index;
            std::ptrdiff_t r_index = r.get_base_node()->index;
            if(l_chunk == r_chunk)
                return l_index - r_index;
            std::ptrdiff_t l_chunk_index = l_chunk->chunks_index;
            std::ptrdiff_t r_chunk_index = r_chunk->chunks_index;
            if(l_chunk_index < r_chunk_index)
            {
                std::ptrdiff_t retval = l_chunk->nodes_after(l_index) + r_chunk->nodes_before(r_index);
                retval = -retval;
                for(auto iter = container->chunks.begin() + (l_chunk_index + 1), end_iter = iter + (r_chunk_index - l_chunk_index - 1); iter != end_iter; ++iter)
                {
                    random_access_list_chunk *c = *iter;
                    retval -= c->size();
                }
                return retval;
            }
            std::ptrdiff_t retval = r_chunk->nodes_after(r_index) + l_chunk->nodes_before(l_index);
            for(auto iter = container->chunks.begin() + (r_chunk_index + 1), end_iter = iter + (l_chunk_index - r_chunk_index - 1); iter != end_iter; ++iter)
            {
                random_access_list_chunk *c = *iter;
                retval += c->size();
            }
            return retval;
        }
        int compare(const iterator_implementation &r) const
        {
            if(*this == r)
                return 0;
            random_access_list_chunk *l_chunk = get_base_node()->chunk;
            random_access_list_chunk *r_chunk = r.get_base_node()->chunk;
            std::ptrdiff_t l_index = get_base_node()->index;
            std::ptrdiff_t r_index = r.get_base_node()->index;
            if(l_chunk == r_chunk)
                return (l_index < r_index) ? -1 : 1;
            std::ptrdiff_t l_chunk_index = l_chunk->chunks_index;
            std::ptrdiff_t r_chunk_index = r_chunk->chunks_index;
            return (l_chunk_index < r_chunk_index) ? -1 : 1;
        }
        void advance(std::ptrdiff_t amount)
        {
            random_access_list_chunk *chunk = get_base_node()->chunk;
            random_access_list_base *container = chunk->container;
            std::ptrdiff_t index = get_base_node()->index;
            const std::ptrdiff_t transition_amount = 5;
            if(index + amount >= (std::ptrdiff_t)chunk->begin && index + amount < (std::ptrdiff_t)chunk->end)
            {
                node_iterator = random_access_list_node_iterator_implementation(chunk->nodes[index + amount]);
            }
            else if(amount <= transition_amount && amount >= -transition_amount)
            {
                if(amount > 0)
                {
                    for(; amount >= 0; amount--)
                    {
                        node_iterator.incr();
                    }
                }
                else
                {
                    for(; amount <= 0; amount++)
                    {
                        node_iterator.decr();
                    }
                }
            }
            else
            {
                if(amount > 0)
                {
                    amount -= chunk->nodes_after(index);
                    chunk = static_cast<random_access_list_chunk *>(chunk->right);
                    for(;;)
                    {
                        if(chunk == &container->chunks_end_node)
                        {
                            node_iterator = random_access_list_node_iterator_implementation(&container->nodes_end_node);
                            break;
                        }
                        std::ptrdiff_t chunk_size = chunk->size();
                        if(amount < chunk_size)
                        {
                            node_iterator = random_access_list_node_iterator_implementation(chunk->nodes[amount + chunk->begin]);
                            break;
                        }
                        amount -= chunk_size;
                    }
                }
                else
                {
                    amount += chunk->nodes_before(index);
                    chunk = static_cast<random_access_list_chunk *>(chunk->prev);
                    for(;;)
                    {
                        if(chunk == &container->chunks_end_node)
                        {
                            node_iterator = random_access_list_node_iterator_implementation(&container->nodes_end_node);
                            break;
                        }
                        std::ptrdiff_t chunk_size = chunk->size();
                        if(-amount < chunk_size)
                        {
                            node_iterator = random_access_list_node_iterator_implementation(chunk->nodes[chunk->end - amount - 1]);
                            break;
                        }
                        amount += chunk_size;
                    }
                }
            }
        }
    };
    iterator_implementation begin_implementation() const
    {
        return iterator_implementation(nodes_end_node->right);
    }
    iterator_implementation last_implementation() const
    {
        return iterator_implementation(nodes_end_node->left);
    }
    iterator_implementation end_implementation() const
    {
        return iterator_implementation(const_cast<random_access_list_node_base_with_chunk *>(&nodes_end_node));
    }
    void push_back_implementation(random_access_list_node_base_with_chunk *node)
    {
        random_access_list_chunk *chunk = chunks.empty() ? nullptr : chunks.back();
        if(chunks.empty() || !chunk->space_at_back())
        {
            chunk = new random_access_list_chunk(this);
            push_back_chunk(chunk);
        }
        chunk->push_back(node);
        nodes_end_node.left->insert_after(node);
        node_count++;
    }
    void push_front_implementation(random_access_list_node_base_with_chunk *node)
    {
        random_access_list_chunk *chunk = chunks.empty() ? nullptr : chunks.front();
        if(chunks.empty() || !chunk->space_at_front())
        {
            chunk = new random_access_list_chunk(this);
            push_front_chunk(chunk);
        }
        chunk->push_front(node);
        nodes_end_node.insert_after(node);
        node_count++;
    }
    iterator_implementation insert_implementation(iterator_implementation p, random_access_list_node_base_with_chunk *node)
    {
        if(p == end_implementation())
        {
            push_back_implementation(node);
            return last_implementation();
        }
        if(p == begin_implementation())
        {
            push_front_implementation(node);
            return begin_implementation();
        }
        p.node_iterator.node->left->insert_after(node);
        random_access_list_chunk *chunk = p.get_base_node()->chunk;
        std::size_t index = p.get_base_node()->index;
        if(chunk->insert(node, index))
        {
            node_count++;
            return iterator_implementation(this, node);
        }
        std::size_t chunk_index = chunk->chunks_index;
        random_access_list_chunk *next_chunk = chunk_index >= chunks.size() ? nullptr : chunks[chunk_index];
        if(chunk_index >= chunks.size() || !next_chunk->space_at_front())
        {
            next_chunk = new random_access_list_chunk(this);
            insert_chunk(next_chunk, chunk_index);
        }
        if(chunk->nodes_after(index) > 0)
        {
            random_access_list_node_base_with_chunk *movedNode = chunk->nodes[chunk->end - 1];
            chunk->pop_back();
            next_chunk->push_front(movedNode);
            chunk->insert(node, index);
            node_count++;
            return iterator_implementation(this, node);
        }
        next_chunk->push_front(node);
        node_count++;
        return iterator_implementation(this, node);
    }
    void erase_implementation(iterator_implementation p)
    {
        random_access_list_chunk *chunk = p.get_base_node()->chunk;
        std::size_t index = p.get_base_node()->index;
        p.node_iterator.node->remove();
        chunk->erase(index);
        node_count--;
        if(chunk->empty())
        {
            erase_chunk(chunk->chunks_index);
            delete chunk;
        }
    }
    void pop_back_implementation()
    {
        erase_implementation(last_implementation());
    }
    void pop_front_implementation()
    {
        erase_implementation(begin_implementation());
    }
    void swap(random_access_list_base &rt)
    {
        std::swap(chunks_end_node, rt.chunks_end_node);
        std::swap(nodes_end_node, rt.nodes_end_node);
        std::swap(chunks, rt.chunks);
        std::swap(node_count, rt.node_count);
    }
};

template <typename T>
class random_access_list : public random_access_list_base
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
    typedef random_access_list_node<T> node_type;
public:
    class const_iterator;
    class iterator final : public std::iterator<std::random_access_iterator_tag, T>
    {
        friend class random_access_list;
        friend class const_iterator;
    private:
        iterator_implementation implementation;
        iterator(iterator_implementation implementation)
            : implementation(implementation)
        {
        }
    public:
        iterator()
        {
        }
        iterator &operator ++()
        {
            implementation.incr();
            return *this;
        }
        iterator &operator --()
        {
            implementation.decr();
            return *this;
        }
        iterator operator ++(int)
        {
            iterator retval = *this;
            implementation.incr();
            return retval;
        }
        iterator operator --(int)
        {
            iterator retval = *this;
            implementation.decr();
            return retval;
        }
        bool operator ==(const iterator &rt) const
        {
            return implementation == rt.implementation;
        }
        bool operator !=(const iterator &rt) const
        {
            return implementation != rt.implementation;
        }
        T &operator *() const
        {
            return static_cast<node_type *>(implementation.node_iterator.node)->value;
        }
        T *operator ->() const
        {
            return &static_cast<node_type *>(implementation.node_iterator.node)->value;
        }
        iterator &operator +=(std::ptrdiff_t n)
        {
            implementation.advance(n);
            return *this;
        }
        iterator &operator -=(std::ptrdiff_t n)
        {
            implementation.advance(-n);
            return *this;
        }
        friend iterator operator +(std::ptrdiff_t n, iterator i)
        {
            i.implementation.advance(n);
            return i;
        }
        iterator operator +(std::ptrdiff_t n) const
        {
            iterator retval = *this;
            retval.implementation.advance(n);
            return retval;
        }
        iterator operator -(std::ptrdiff_t n) const
        {
            iterator retval = *this;
            retval.implementation.advance(-n);
            return retval;
        }
        std::ptrdiff_t operator -(const iterator &r) const
        {
            return implementation - r.implementation;
        }
        T &operator [](std::ptrdiff_t n) const
        {
            return operator +(n).operator *();
        }
        bool operator <(const iterator &r) const
        {
            return implementation.compare(r.implementation) < 0;
        }
        bool operator >(const iterator &r) const
        {
            return implementation.compare(r.implementation) > 0;
        }
        bool operator <=(const iterator &r) const
        {
            return implementation.compare(r.implementation) <= 0;
        }
        bool operator >=(const iterator &r) const
        {
            return implementation.compare(r.implementation) >= 0;
        }
    };
    class const_iterator final : public std::iterator<std::random_access_iterator_tag, const T>
    {
        friend class random_access_list;
    private:
        iterator_implementation implementation;
        const_iterator(iterator_implementation implementation)
            : implementation(implementation)
        {
        }
    public:
        const_iterator()
        {
        }
        const_iterator(iterator i)
            : implementation(i.implementation)
        {
        }
        const_iterator &operator ++()
        {
            implementation.incr();
            return *this;
        }
        const_iterator &operator --()
        {
            implementation.decr();
            return *this;
        }
        const_iterator operator ++(int)
        {
            const_iterator retval = *this;
            implementation.incr();
            return retval;
        }
        const_iterator operator --(int)
        {
            const_iterator retval = *this;
            implementation.decr();
            return retval;
        }
        bool operator ==(const const_iterator &rt) const
        {
            return implementation == rt.implementation;
        }
        bool operator !=(const const_iterator &rt) const
        {
            return implementation != rt.implementation;
        }
        const T &operator *() const
        {
            return static_cast<node_type *>(implementation.node_iterator.node)->value;
        }
        const T *operator ->() const
        {
            return &static_cast<node_type *>(implementation.node_iterator.node)->value;
        }
        const_iterator &operator +=(std::ptrdiff_t n)
        {
            implementation.advance(n);
            return *this;
        }
        const_iterator &operator -=(std::ptrdiff_t n)
        {
            implementation.advance(-n);
            return *this;
        }
        friend const_iterator operator +(std::ptrdiff_t n, const_iterator i)
        {
            i.implementation.advance(n);
            return i;
        }
        const_iterator operator +(std::ptrdiff_t n) const
        {
            const_iterator retval = *this;
            retval.implementation.advance(n);
            return retval;
        }
        const_iterator operator -(std::ptrdiff_t n) const
        {
            const_iterator retval = *this;
            retval.implementation.advance(-n);
            return retval;
        }
        std::ptrdiff_t operator -(const const_iterator &r) const
        {
            return implementation - r.implementation;
        }
        const T &operator [](std::ptrdiff_t n) const
        {
            return operator +(n).operator *();
        }
        bool operator <(const const_iterator &r) const
        {
            return implementation.compare(r.implementation) < 0;
        }
        bool operator >(const const_iterator &r) const
        {
            return implementation.compare(r.implementation) > 0;
        }
        bool operator <=(const const_iterator &r) const
        {
            return implementation.compare(r.implementation) <= 0;
        }
        bool operator >=(const const_iterator &r) const
        {
            return implementation.compare(r.implementation) >= 0;
        }
    };
    iterator begin()
    {
        return iterator(begin_implementation());
    }
    const_iterator begin() const
    {
        return const_iterator(begin_implementation());
    }
    const_iterator cbegin() const
    {
        return const_iterator(begin_implementation());
    }
    iterator end()
    {
        return iterator(end_implementation());
    }
    const_iterator end() const
    {
        return const_iterator(end_implementation());
    }
    const_iterator cend() const
    {
        return const_iterator(end_implementation());
    }
    iterator last()
    {
        return iterator(last_implementation());
    }
    const_iterator last() const
    {
        return const_iterator(last_implementation());
    }
    const_iterator clast() const
    {
        return const_iterator(last_implementation());
    }
    bool empty() const
    {
        return node_count == 0;
    }
    std::size_t size() const
    {
        return node_count;
    }
    void pop_back()
    {
        node_type *node = static_cast<node_type *>(nodes_end_node.left);
        pop_back_implementation();
        delete node;
    }
    void pop_front()
    {
        node_type *node = static_cast<node_type *>(nodes_end_node.right);
        pop_front_implementation();
        delete node;
    }
    void push_back(const T &v)
    {
        node_type *node = new node_type(v);
        push_back_implementation(node);
    }
    void push_back(T &&v)
    {
        node_type *node = new node_type(std::move(v));
        push_back_implementation(node);
    }
    template <typename ...Args>
    void emplace_back(Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_back_implementation(node);
    }
    void push_front(const T &v)
    {
        node_type *node = new node_type(v);
        push_front_implementation(node);
    }
    void push_front(T &&v)
    {
        node_type *node = new node_type(std::move(v));
        push_front_implementation(node);
    }
    template <typename ...Args>
    void emplace_front(Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_front_implementation(node);
    }
    template <typename ...Args>
    iterator emplace(const_iterator pos, Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        return iterator(insert_implementation(pos.implementation, node));
    }
    iterator insert(const_iterator pos, const T &v)
    {
        node_type *node = new node_type(v);
        return iterator(insert_implementation(pos.implementation, node));
    }
    iterator insert(const_iterator pos, T &&v)
    {
        node_type *node = new node_type(std::move(v));
        return iterator(insert_implementation(pos.implementation, node));
    }
    iterator erase(const_iterator pos)
    {
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        iterator retval(pos.implementation);
        ++retval;
        erase_implementation(pos.implementation);
        delete node;
        return retval;
    }
    void resize(std::size_t count)
    {
        while(count < size())
        {
            pop_back();
        }
        while(count > size())
        {
            emplace_back();
        }
    }
    void resize(std::size_t count, const T &value)
    {
        while(count < size())
        {
            pop_back();
        }
        while(count > size())
        {
            push_back(value);
        }
    }
    void clear()
    {
        while(!empty())
            pop_back();
    }
    random_access_list()
    {
    }
    random_access_list(const random_access_list &rt)
    {
        for(const T &v : rt)
        {
            push_back(v);
        }
    }
    random_access_list(random_access_list &&rt)
        : random_access_list_base(std::move(rt))
    {
    }
    ~random_access_list()
    {
        clear();
    }
    void swap(random_access_list &rt)
    {
        random_access_list_base::swap(rt);
    }
    random_access_list &operator =(random_access_list &&rt)
    {
        if(&rt == this)
            return *this;
        random_access_list(std::move(rt)).swap(*this);
        return *this;
    }
    random_access_list &operator =(const random_access_list &rt)
    {
        if(&rt == this)
            return *this;
        random_access_list(rt).swap(*this);
        return *this;
    }
    void splice(const_iterator pos, random_access_list &other, const_iterator it)
    {
        if(&other == this && it == pos)
            return;
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        other.erase_implementation(it.implementation.node_iterator.node);
        insert_implementation(pos, node);
    }
    void splice(const_iterator pos, random_access_list &&other, const_iterator it)
    {
        if(&other == this && it == pos)
            return;
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        other.erase_implementation(it.implementation.node_iterator.node);
        insert_implementation(pos, node);
    }
    void splice(const_iterator pos, random_access_list &other)
    {
        auto iter = other.begin();
        while(iter != other.end())
        {
            auto next_iter = iter;
            ++next_iter;
            splice(pos, other, iter);
            iter = next_iter;
        }
    }
    void splice(const_iterator pos, random_access_list &&other)
    {
        auto iter = other.begin();
        while(iter != other.end())
        {
            auto next_iter = iter;
            ++next_iter;
            splice(pos, other, iter);
            iter = next_iter;
        }
    }
    void splice(const_iterator pos, random_access_list &other, const_iterator first, const_iterator last)
    {
        auto iter = first;
        while(iter != last)
        {
            auto next_iter = iter;
            ++next_iter;
            splice(pos, other, iter);
            iter = next_iter;
        }
    }
    void splice(const_iterator pos, random_access_list &&other, const_iterator first, const_iterator last)
    {
        auto iter = first;
        while(iter != last)
        {
            auto next_iter = iter;
            ++next_iter;
            splice(pos, other, iter);
            iter = next_iter;
        }
    }
};

#endif // RANDOM_ACCESS_LIST_H_INCLUDED
