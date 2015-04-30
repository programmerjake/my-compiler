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
#include <cstdint>
#include <utility>
#include <deque>
#include <list>
#include <algorithm>
#include <cassert>

class random_access_list_base
{
protected:
    struct node_base
    {
        node_base *prev = this;
        node_base *next = this;
        node_base *parent = nullptr;
        node_base *left = nullptr;
        node_base *right = nullptr;
        std::size_t tree_node_count = 1;
        std::uint8_t tree_depth = 0;
        std::uint8_t depth_through_left() const
        {
            if(left)
                return 1 + left->tree_depth;
            return 0;
        }
        std::uint8_t depth_through_right() const
        {
            if(right)
                return 1 + right->tree_depth;
            return 0;
        }
        std::size_t left_node_count() const
        {
            if(left)
                return left->tree_node_count;
            return 0;
        }
        std::size_t right_node_count() const
        {
            if(right)
                return right->tree_node_count;
            return 0;
        }
        void calc_depth_and_node_count()
        {
            tree_depth = std::max(depth_through_left(), depth_through_right());
            tree_node_count = 1 + left_node_count() + right_node_count();
        }
        bool is_end() const
        {
            return tree_node_count == 0;
        }
    };
    struct end_node_t final : public node_base
    {
        node_base *tree_base = nullptr;
        end_node_t()
        {
            tree_node_count = 0;
        }
    };
    void rotate_left(node_base *&tree)
    {
        node_base *node1 = tree;
        assert(node1);
        node_base *node2 = tree->right;
        assert(node2);
        node_base *subtree1 = node1->left;
        node_base *subtree2 = node2->left;
        node_base *subtree3 = node2->right;
        node_base *parent = tree->parent;
        if(subtree1)
            subtree1->parent = node1;
        if(subtree2)
            subtree2->parent = node1;
        if(subtree3)
            subtree3->parent = node2;
        node1->parent = node2;
        node2->parent = parent;
        node1->left = subtree1;
        node1->right = subtree2;
        node2->right = subtree3;
        node2->left = node1;
        tree = node2;
        node1->calc_depth_and_node_count();
        node2->calc_depth_and_node_count();
    }
    void rotate_right(node_base *&tree)
    {
        node_base *node2 = tree;
        assert(node2);
        node_base *node1 = tree->left;
        assert(node1);
        node_base *subtree1 = node1->left;
        node_base *subtree2 = node1->right;
        node_base *subtree3 = node2->right;
        node_base *parent = tree->parent;
        if(subtree1)
            subtree1->parent = node1;
        if(subtree2)
            subtree2->parent = node2;
        if(subtree3)
            subtree3->parent = node2;
        node1->parent = parent;
        node2->parent = node1;
        node1->left = subtree1;
        node2->left = subtree2;
        node2->right = subtree3;
        node1->right = node2;
        tree = node1;
        node1->calc_depth_and_node_count();
        node2->calc_depth_and_node_count();
    }
    void balance(node_base *&tree)
    {
        if(!tree)
            return;
        std::uint8_t ld = tree->depth_through_left();
        std::uint8_t rd = tree->depth_through_right();
        if(ld + 1 < rd)
            rotate_left(tree);
        else if(ld > rd + 1)
            rotate_right(tree);
    }
    end_node_t end_node;
    static node_base *advance_non_end(node_base *node, std::ptrdiff_t n)
    {
        for(;;)
        {
            assert(node);
            if(n == 0)
                return node;
            if(n == 1)
                return node->next;
            if(n == -1)
                return node->prev;
            if(n < 0)
            {
                if(-n > static_cast<std::ptrdiff_t>(node->left_node_count()))
                {
                    node_base *parent = node->parent;
                    assert(parent);
                    if(parent->left == node)
                    {
                        assert(parent->right != node);
                        n -= 1 + static_cast<std::ptrdiff_t>(node->right_node_count());
                        node = parent;
                    }
                    else
                    {
                        assert(parent->right == node);
                        n += 1 + static_cast<std::ptrdiff_t>(node->left_node_count());
                        node = parent;
                    }
                }
                else
                {
                    node = static_cast<node_base *>(node->left);
                    assert(node);
                    n += node->right_node_count() + 1;
                }
            }
            else
            {
                if(n > static_cast<std::ptrdiff_t>(node->right_node_count()))
                {
                    node_base *parent = node->parent;
                    if(parent == nullptr)
                    {
                        assert(n == static_cast<std::ptrdiff_t>(node->right_node_count()) + 1);
                        while(node->right != nullptr)
                            node = node->right;
                        return node->next;
                    }
                    assert(parent);
                    if(parent->left == node)
                    {
                        assert(parent->right != node);
                        n -= 1 + static_cast<std::ptrdiff_t>(node->right_node_count());
                        node = parent;
                    }
                    else
                    {
                        assert(parent->right == node);
                        n += 1 + static_cast<std::ptrdiff_t>(node->left_node_count());
                        node = parent;
                    }
                }
                else
                {
                    node = node->right;
                    assert(node);
                    n -= node->left_node_count() + 1;
                }
            }
        }
    }
    static node_base *advance(node_base *node, std::ptrdiff_t n)
    {
        assert(node);
        if(n == 0)
            return node;
        if(n == 1)
            return node->next;
        if(n == -1)
            return node->prev;
        if(node->is_end())
        {
            assert(n < 0);
            return advance_non_end(node->prev, n + 1);
        }
        return advance_non_end(node, n);
    }
    static std::size_t position(node_base *node)
    {
        assert(node);
        std::ptrdiff_t retval = 0;
        if(node->is_end())
        {
            end_node_t *pend_node = static_cast<end_node_t *>(node);
            if(pend_node->tree_base)
                return pend_node->tree_base->tree_node_count;
            return 0;
        }
        for(;;)
        {
            node_base *parent = node->parent;
            if(parent)
            {
                if(parent->left == node)
                {
                    assert(parent->right != node);
                    retval -= 1 + static_cast<std::ptrdiff_t>(node->right_node_count());
                    node = parent;
                }
                else
                {
                    assert(parent->right == node);
                    retval += 1 + static_cast<std::ptrdiff_t>(node->left_node_count());
                    node = parent;
                }
            }
            else
            {
                retval += node->left_node_count();
                return static_cast<std::size_t>(retval);
            }
        }
    }
    static int compare(node_base *l, node_base *r)
    {
        assert(l);
        assert(r);
        if(l == r)
            return 0;
        if(l->is_end())
            return r->is_end() ? 0 : 1;
        if(r->is_end())
            return -1;
        if(l->left == r)
            return 1;
        if(l->right == r)
            return -1;
        if(r->left == l)
            return -1;
        if(r->right == l)
            return 1;
        if(l->next == r)
            return -1;
        if(l->prev == r)
            return 1;
        std::size_t l_position = position(l);
        std::size_t r_position = position(r);
        if(l_position < r_position)
            return -1;
        assert(l_position > r_position);
        return 1;
    }
    static std::ptrdiff_t difference(node_base *l, node_base *r)
    {
        assert(l);
        assert(r);
        if(l == r)
            return 0;
        if(!l->is_end() && l->next == r)
            return -1;
        if(!r->is_end() && l->prev == r)
            return 1;
        std::size_t l_position = position(l);
        std::size_t r_position = position(r);
        return static_cast<std::ptrdiff_t>(l_position) - static_cast<std::ptrdiff_t>(r_position);
    }
    struct iterator_imp final
    {
        node_base *node;
        iterator_imp(node_base *node = nullptr)
            : node(node)
        {
        }
        void inc()
        {
            assert(node != nullptr);
            node = node->next;
            assert(node != nullptr);
        }
        void dec()
        {
            assert(node != nullptr);
            node = node->prev;
            assert(node != nullptr);
        }
        void advance(std::ptrdiff_t n)
        {
            assert(node);
            node = random_access_list_base::advance(node, n);
            assert(node);
        }
        bool equals(const iterator_imp &rt) const
        {
            return node == rt.node;
        }
        std::size_t position() const
        {
            assert(node);
            return random_access_list_base::position(node);
        }
        int compare(const iterator_imp &rt) const
        {
            if(node == rt.node)
                return 0;
            assert(node);
            assert(rt.node);
            return random_access_list_base::compare(node, rt.node);
        }
        std::ptrdiff_t difference(const iterator_imp &rt) const
        {
            if(node == rt.node)
                return 0;
            assert(node);
            assert(rt.node);
            return random_access_list_base::difference(node, rt.node);
        }
    };
    iterator_imp begin_imp() const
    {
        return iterator_imp(end_node->next);
    }
    iterator_imp end_imp() const
    {
        return iterator_imp(const_cast<node_base *>(&end_node));
    }
    iterator_imp last_imp() const
    {
        return iterator_imp(end_node->prev);
    }
    bool empty() const
    {
        return end_node.tree_base == nullptr;
    }
    std::size_t size() const
    {
        if(empty())
            return 0;
        return end_node.tree_base->tree_node_count;
    }
    bool unit_size() const
    {
        return !empty() && end_node.prev == end_node.next;
    }
    void add_first_node(node_base *new_node)
    {
        assert(empty());
        new_node->prev = new_node->next = &end_node;
        end_node.prev = end_node.next = new_node;
        new_node->parent = nullptr;
        new_node->left = nullptr;
        new_node->right = nullptr;
        new_node->calc_depth_and_node_count();
        end_node.tree_base = new_node;
    }
    void remove_only_node()
    {
        assert(unit_size());
        end_node.prev = end_node.next = &end_node;
        end_node.tree_base = nullptr;
    }
    static void insert_before(node_base *new_node, node_base *&tree_base, std::ptrdiff_t tree_base_offset)
    {
        assert(tree_base);
        if(tree_base_offset <= 0) // insert on left side of tree_base
        {
            if(tree_base->left == nullptr)
            {
                tree_base->left = new_node;
                new_node->left = nullptr;
                new_node->right = nullptr;
                new_node->prev = tree_base->prev;
                new_node->next = tree_base;
                new_node->prev->next = new_node;
                new_node->next->prev = new_node;
                new_node->parent = tree_base;
                new_node->tree_depth = 0;
                new_node->tree_node_count = 1;
            }
            else
            {
                insert_before(new_node, tree_base->left, tree_base_offset + tree_base->left->right_node_count() + 1);
            }
        }
        else
        {
            if(tree_base->right == nullptr)
            {
                tree_base->right = new_node;
                new_node->left = nullptr;
                new_node->right = nullptr;
                new_node->prev = tree_base;
                new_node->next = tree_base->next;
                new_node->prev->next = new_node;
                new_node->next->prev = new_node;
                new_node->parent = tree_base;
                new_node->tree_depth = 0;
                new_node->tree_node_count = 1;
            }
            else
            {
                insert_before(new_node, tree_base->right, tree_base_offset + tree_base->right->left_node_count() + 1);
            }
        }
        balance(tree_base);
    }
    void insert_imp(node_base *new_node, iterator_imp insert_before_iter)
    {
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_before(new_node, end_node.tree_base, insert_before_iter.difference(iterator_imp(end_node.tree_base)));
    }
    void push_front_imp(node_base *new_node)
    {
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_imp(new_node, end_node->next); // begin
    }
    void push_back_imp(node_base *new_node)
    {
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_imp(new_node, &end_node);
    }
    void remove_imp(iterator_imp iter)
    {
        assert(!empty());
        if(unit_size())
        {
            remove_only_node();
            return;
        }
        assert(false);
        #warning finish
    }
};

template <typename T>
class random_access_list final : private random_access_list_base
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
    #error finish
public:
    class const_iterator;
    class iterator final : public std::iterator<std::random_access_iterator_tag, T>
    {
        friend class random_access_list;
        friend class const_iterator;
    private:
        #error finish
        list_iterator iter;
        iterator(list_iterator iter)
            : iter(iter)
        {
        }
    public:
        iterator()
        {
        }
        iterator &operator ++()
        {
            ++iter;
            return *this;
        }
        iterator &operator --()
        {
            --iter;
            return *this;
        }
        iterator operator ++(int)
        {
            return iterator(iter++);
        }
        iterator operator --(int)
        {
            return iterator(iter--);
        }
        bool operator ==(const iterator &rt) const
        {
            return iter == rt.iter;
        }
        bool operator !=(const iterator &rt) const
        {
            return iter != rt.iter;
        }
        T &operator *() const
        {
            return iter->value;
        }
        T *operator ->() const
        {
            return &iter->value;
        }
        iterator &operator +=(std::ptrdiff_t n)
        {
            iter =
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
        #error finish
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
        #error finish
        return iterator(begin_implementation());
    }
    const_iterator begin() const
    {
        #error finish
        return const_iterator(begin_implementation());
    }
    const_iterator cbegin() const
    {
        #error finish
        return const_iterator(begin_implementation());
    }
    iterator end()
    {
        #error finish
        return iterator(end_implementation());
    }
    const_iterator end() const
    {
        #error finish
        return const_iterator(end_implementation());
    }
    const_iterator cend() const
    {
        #error finish
        return const_iterator(end_implementation());
    }
    iterator last()
    {
        #error finish
        return iterator(last_implementation());
    }
    const_iterator last() const
    {
        #error finish
        return const_iterator(last_implementation());
    }
    const_iterator clast() const
    {
        #error finish
        return const_iterator(last_implementation());
    }
    bool empty() const
    {
        #error finish
        return node_count == 0;
    }
    std::size_t size() const
    {
        #error finish
        return node_count;
    }
    void pop_back()
    {
        #error finish
        node_type *node = static_cast<node_type *>(nodes_end_node.left);
        pop_back_implementation();
        delete node;
    }
    void pop_front()
    {
        #error finish
        node_type *node = static_cast<node_type *>(nodes_end_node.right);
        pop_front_implementation();
        delete node;
    }
    void push_back(const T &v)
    {
        #error finish
        node_type *node = new node_type(v);
        push_back_implementation(node);
    }
    void push_back(T &&v)
    {
        #error finish
        node_type *node = new node_type(std::move(v));
        push_back_implementation(node);
    }
    template <typename ...Args>
    void emplace_back(Args &&...args)
    {
        #error finish
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_back_implementation(node);
    }
    void push_front(const T &v)
    {
        #error finish
        node_type *node = new node_type(v);
        push_front_implementation(node);
    }
    void push_front(T &&v)
    {
        #error finish
        node_type *node = new node_type(std::move(v));
        push_front_implementation(node);
    }
    template <typename ...Args>
    void emplace_front(Args &&...args)
    {
        #error finish
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_front_implementation(node);
    }
    template <typename ...Args>
    iterator emplace(const_iterator pos, Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        #error finish
        return iterator(insert_implementation(pos.implementation, node));
    }
    iterator insert(const_iterator pos, const T &v)
    {
        node_type *node = new node_type(v);
        #error finish
        return iterator(insert_implementation(pos.implementation, node));
    }
    iterator insert(const_iterator pos, T &&v)
    {
        node_type *node = new node_type(std::move(v));
        return iterator(insert_implementation(pos.implementation, node));
        #error finish
    }
    iterator erase(const_iterator pos)
    {
        #error finish
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        iterator retval(pos.implementation);
        ++retval;
        erase_implementation(pos.implementation);
        delete node;
        return retval;
    }
    void resize(std::size_t count)
    {
        #error finish
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
        #error finish
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
        #error finish
        while(!empty())
            pop_back();
    }
    random_access_list()
    {
        #error finish
    }
    random_access_list(const random_access_list &rt)
    {
        #error finish
        for(const T &v : rt)
        {
            push_back(v);
        }
    }
    random_access_list(random_access_list &&rt)
        : random_access_list_base(std::move(rt))
    {
        #error finish
    }
    ~random_access_list()
    {
        #error finish
        clear();
    }
    void swap(random_access_list &rt)
    {
        #error finish
        random_access_list_base::swap(rt);
    }
    random_access_list &operator =(random_access_list &&rt)
    {
        #error finish
        if(&rt == this)
            return *this;
        random_access_list(std::move(rt)).swap(*this);
        return *this;
    }
    random_access_list &operator =(const random_access_list &rt)
    {
        #error finish
        if(&rt == this)
            return *this;
        random_access_list(rt).swap(*this);
        return *this;
    }
    void splice(const_iterator pos, random_access_list &other, const_iterator it)
    {
        #error finish
        if(&other == this && it == pos)
            return;
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        other.erase_implementation(it.implementation.node_iterator.node);
        insert_implementation(pos, node);
    }
    void splice(const_iterator pos, random_access_list &&other, const_iterator it)
    {
        #error finish
        if(&other == this && it == pos)
            return;
        node_type *node = static_cast<node_type *>(pos.implementation.node_iterator.node);
        other.erase_implementation(it.implementation.node_iterator.node);
        insert_implementation(pos, node);
    }
    void splice(const_iterator pos, random_access_list &other)
    {
        #error finish
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
        #error finish
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
        #error finish
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
        #error finish
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
