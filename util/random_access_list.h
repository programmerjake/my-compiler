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
        if(parent == nullptr)
            end_node.tree_base = node2;
        node1->left = subtree1;
        node1->right = subtree2;
        node2->right = subtree3;
        node2->left = node1;
        tree = node2;
        node1->calc_depth_and_node_count();
        node2->calc_depth_and_node_count();
        for(node_base *node = parent; node; node = node->parent)
        {
            node->calc_depth_and_node_count();
        }
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
        if(parent == nullptr)
            end_node.tree_base = node1;
        node2->parent = node1;
        node1->left = subtree1;
        node2->left = subtree2;
        node2->right = subtree3;
        node1->right = node2;
        tree = node1;
        node2->calc_depth_and_node_count();
        node1->calc_depth_and_node_count();
        for(node_base *node = parent; node; node = node->parent)
        {
            node->calc_depth_and_node_count();
        }
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
        return iterator_imp(end_node.next);
    }
    iterator_imp end_imp() const
    {
        return iterator_imp(const_cast<node_base *>(static_cast<const node_base *>(&end_node)));
    }
    iterator_imp last_imp() const
    {
        return iterator_imp(end_node.prev);
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
    void insert_before(node_base *new_node, node_base *&tree_base, std::ptrdiff_t tree_base_offset)
    {
        assert(new_node);
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
                for(node_base *node = tree_base; node != nullptr; node = node->parent)
                {
                    node->calc_depth_and_node_count();
                }
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
                for(node_base *node = tree_base; node != nullptr; node = node->parent)
                {
                    node->calc_depth_and_node_count();
                }
            }
            else
            {
                insert_before(new_node, tree_base->right, tree_base_offset - tree_base->right->left_node_count() - 1);
            }
        }
        balance(tree_base);
    }
    void insert_imp(node_base *new_node, iterator_imp insert_before_iter)
    {
        assert(new_node);
        assert(insert_before_iter.node);
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_before(new_node, end_node.tree_base, insert_before_iter.difference(iterator_imp(end_node.tree_base)));
    }
    void push_front_imp(node_base *new_node)
    {
        assert(new_node);
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_imp(new_node, end_node.next); // begin
    }
    void push_back_imp(node_base *new_node)
    {
        assert(new_node);
        if(empty())
        {
            add_first_node(new_node);
            return;
        }
        insert_imp(new_node, &end_node);
    }
    void remove_imp(iterator_imp iter)
    {
        node_base *node = iter.node;
        assert(!empty());
        if(unit_size())
        {
            assert(node == end_node.next); // node == begin
            remove_only_node();
            return;
        }
        assert(node);
        assert(!node->is_end());
        if(node->left == nullptr || node->right == nullptr)
        {
            node_base *parent = node->parent;
            node_base **pnode;
            if(parent)
            {
                if(parent->left == node)
                {
                    assert(parent->right != node);
                    pnode = &parent->left;
                }
                else
                {
                    assert(parent->right == node);
                    assert(parent->left != node);
                    pnode = &parent->right;
                }
            }
            else
            {
                pnode = &end_node.tree_base;
            }
            if(node->left == nullptr)
            {
                *pnode = node->right;
            }
            else
            {
                *pnode = node->left;
            }
            node->prev->next = node->next;
            node->next->prev = node->prev;
            if(*pnode)
                (*pnode)->parent = parent;
            while(parent)
            {
                node = parent;
                node->calc_depth_and_node_count();
                parent = node->parent;
                balance(node);
            }
            return;
        }
        node_base *replacement_node = node->next;
        assert(replacement_node->left == nullptr || replacement_node->right == nullptr);
        remove_imp(iterator_imp(replacement_node));
        node_base *parent = node->parent;
        node_base **pnode;
        if(parent)
        {
            if(parent->left == node)
            {
                assert(parent->right != node);
                pnode = &parent->left;
            }
            else
            {
                assert(parent->right == node);
                assert(parent->left != node);
                pnode = &parent->right;
            }
        }
        else
        {
            pnode = &end_node.tree_base;
        }
        *pnode = replacement_node;
        replacement_node->parent = parent;
        replacement_node->left = node->left;
        if(node->left)
            node->left->parent = replacement_node;
        replacement_node->right = node->right;
        if(node->right)
            node->right->parent = replacement_node;
        replacement_node->tree_node_count = node->tree_node_count;
        replacement_node->tree_depth = node->tree_depth;
        replacement_node->prev = node->prev;
        replacement_node->next = node->next;
        node->prev->next = replacement_node;
        node->next->prev = replacement_node;
    }
    void pop_front_imp()
    {
        assert(!empty());
        remove_imp(begin_imp());
    }
    void pop_back_imp()
    {
        assert(!empty());
        remove_imp(last_imp());
    }
    random_access_list_base() = default;
    random_access_list_base(random_access_list_base &&rt)
    {
        end_node.tree_base = rt.end_node.tree_base;
        rt.end_node.tree_base = nullptr;
        end_node.prev = rt.end_node.prev;
        end_node.next = rt.end_node.next;
        rt.end_node.prev = rt.end_node.next = &rt.end_node;
        end_node.prev->next = &end_node;
        end_node.next->prev = &end_node;
    }
    random_access_list_base(const random_access_list_base &) = delete;
    random_access_list_base &operator =(const random_access_list_base &) = delete;
    random_access_list_base &operator =(random_access_list_base &&) = delete;
    void swap(random_access_list_base &other)
    {
        if(empty() && other.empty())
            return;
        if(other.empty())
        {
            other.swap(*this);
            return;
        }
        if(empty())
        {
            end_node.tree_base = other.end_node.tree_base;
            other.end_node.tree_base = nullptr;
            end_node.prev = other.end_node.prev;
            end_node.next = other.end_node.next;
            other.end_node.prev = other.end_node.next = &other.end_node;
            end_node.prev->next = &end_node;
            end_node.next->prev = &end_node;
        }
        else
        {
            end_node_t temp;
            temp.tree_base = other.end_node.tree_base;
            other.end_node.tree_base = end_node.tree_base;
            end_node.tree_base = temp.tree_base;
            temp.prev = other.end_node.prev;
            temp.next = other.end_node.next;
            other.end_node.prev = end_node.prev;
            other.end_node.next = end_node.next;
            other.end_node.prev->next = &other.end_node;
            other.end_node.next->prev = &other.end_node;
            end_node.prev = temp.prev;
            end_node.next = temp.next;
            end_node.prev->next = &end_node;
            end_node.next->prev = &end_node;
        }
    }
    void verify_all_helper(node_base *tree, node_base *before_begin, node_base *end)
    {
        assert(tree != nullptr);
        assert(before_begin != nullptr);
        assert(end != nullptr);
        assert(tree->tree_node_count == tree->left_node_count() + 1 + tree->right_node_count());
        assert(tree->tree_depth == std::max(tree->depth_through_left(), tree->depth_through_right()));
        if(tree->left == nullptr)
        {
            assert(tree == before_begin->next);
            assert(tree->prev == before_begin);
        }
        else
        {
            assert(tree->left->parent == tree);
            verify_all_helper(tree->left, before_begin, tree);
        }
        if(tree->right == nullptr)
        {
            assert(tree == end->prev);
            assert(tree->next == end);
        }
        else
        {
            assert(tree->right->parent == tree);
            verify_all_helper(tree->right, tree, end);
        }
    }
    void verify_all()
    {
        assert(end_node.left == nullptr);
        assert(end_node.right == nullptr);
        assert(end_node.parent == nullptr);
        assert(end_node.tree_depth == 0);
        assert(end_node.tree_node_count == 0);
        if(empty())
        {
            assert(end_node.prev == &end_node);
            assert(end_node.next == &end_node);
            assert(end_node.tree_base == nullptr);
        }
        else
        {
            assert(end_node.tree_base);
            assert(end_node.tree_base->parent == nullptr);
            verify_all_helper(end_node.tree_base, &end_node, &end_node);
        }
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
    struct node_type final : public node_base
    {
        T value;
        template <typename ...Args>
        explicit node_type(Args &&...args)
            : value(std::forward<Args>(args)...)
        {
        }
    };
public:
    class const_iterator;
    class iterator final : public std::iterator<std::random_access_iterator_tag, T>
    {
        friend class random_access_list;
        friend class const_iterator;
    private:
        iterator_imp iter;
        iterator(iterator_imp iter)
            : iter(iter)
        {
        }
    public:
        iterator()
        {
        }
        iterator &operator ++()
        {
            iter.inc();
            return *this;
        }
        iterator &operator --()
        {
            iter.dec();
            return *this;
        }
        iterator operator ++(int)
        {
            iterator retval = *this;
            iter.inc();
            return retval;
        }
        iterator operator --(int)
        {
            iterator retval = *this;
            iter.dec();
            return retval;
        }
        bool operator ==(const iterator &rt) const
        {
            return iter.equals(rt.iter);
        }
        bool operator !=(const iterator &rt) const
        {
            return !iter.equals(rt.iter);
        }
        T &operator *() const
        {
            return static_cast<node_type *>(iter.node)->value;
        }
        T *operator ->() const
        {
            return &static_cast<node_type *>(iter.node)->value;
        }
        iterator &operator +=(std::ptrdiff_t n)
        {
            iter.advance(n);
            return *this;
        }
        iterator &operator -=(std::ptrdiff_t n)
        {
            iter.advance(-n);
            return *this;
        }
        friend iterator operator +(std::ptrdiff_t n, iterator i)
        {
            i.iter.advance(n);
            return i;
        }
        iterator operator +(std::ptrdiff_t n) const
        {
            iterator retval = *this;
            retval.iter.advance(n);
            return retval;
        }
        iterator operator -(std::ptrdiff_t n) const
        {
            iterator retval = *this;
            retval.iter.advance(-n);
            return retval;
        }
        std::ptrdiff_t operator -(const iterator &r) const
        {
            return iter.difference(r.iter);
        }
        T &operator [](std::ptrdiff_t n) const
        {
            return operator +(n).operator *();
        }
        bool operator <(const iterator &r) const
        {
            return iter.compare(r.iter) < 0;
        }
        bool operator >(const iterator &r) const
        {
            return iter.compare(r.iter) > 0;
        }
        bool operator <=(const iterator &r) const
        {
            return iter.compare(r.iter) <= 0;
        }
        bool operator >=(const iterator &r) const
        {
            return iter.compare(r.iter) >= 0;
        }
    };
    class const_iterator final : public std::iterator<std::random_access_iterator_tag, const T>
    {
        friend class random_access_list;
    private:
        iterator_imp iter;
        const_iterator(iterator_imp iter)
            : iter(iter)
        {
        }
    public:
        const_iterator()
        {
        }
        const_iterator(iterator i)
            : iter(i.iter)
        {
        }
        const_iterator &operator ++()
        {
            iter.inc();
            return *this;
        }
        const_iterator &operator --()
        {
            iter.dec();
            return *this;
        }
        const_iterator operator ++(int)
        {
            const_iterator retval = *this;
            iter.inc();
            return retval;
        }
        const_iterator operator --(int)
        {
            const_iterator retval = *this;
            iter.dec();
            return retval;
        }
        bool operator ==(const const_iterator &rt) const
        {
            return iter.equals(rt.iter);
        }
        bool operator !=(const const_iterator &rt) const
        {
            return !iter.equals(rt.iter);
        }
        const T &operator *() const
        {
            return static_cast<node_type *>(iter.node)->value;
        }
        const T *operator ->() const
        {
            return &static_cast<node_type *>(iter.node)->value;
        }
        const_iterator &operator +=(std::ptrdiff_t n)
        {
            iter.advance(n);
            return *this;
        }
        const_iterator &operator -=(std::ptrdiff_t n)
        {
            iter.advance(-n);
            return *this;
        }
        friend const_iterator operator +(std::ptrdiff_t n, const_iterator i)
        {
            i.iter.advance(n);
            return i;
        }
        const_iterator operator +(std::ptrdiff_t n) const
        {
            const_iterator retval = *this;
            retval.iter.advance(n);
            return retval;
        }
        const_iterator operator -(std::ptrdiff_t n) const
        {
            const_iterator retval = *this;
            retval.iter.advance(-n);
            return retval;
        }
        std::ptrdiff_t operator -(const const_iterator &r) const
        {
            return iter.difference(r.iter);
        }
        const T &operator [](std::ptrdiff_t n) const
        {
            return operator +(n).operator *();
        }
        bool operator <(const const_iterator &r) const
        {
            return iter.compare(r.iter) < 0;
        }
        bool operator >(const const_iterator &r) const
        {
            return iter.compare(r.iter) > 0;
        }
        bool operator <=(const const_iterator &r) const
        {
            return iter.compare(r.iter) <= 0;
        }
        bool operator >=(const const_iterator &r) const
        {
            return iter.compare(r.iter) >= 0;
        }
    };
    iterator begin()
    {
        return iterator(begin_imp());
    }
    const_iterator begin() const
    {
        return const_iterator(begin_imp());
    }
    const_iterator cbegin() const
    {
        return const_iterator(begin_imp());
    }
    iterator end()
    {
        return iterator(end_imp());
    }
    const_iterator end() const
    {
        return const_iterator(end_imp());
    }
    const_iterator cend() const
    {
        return const_iterator(end_imp());
    }
    iterator last()
    {
        return iterator(last_imp());
    }
    const_iterator last() const
    {
        return const_iterator(last_imp());
    }
    const_iterator clast() const
    {
        return const_iterator(last_imp());
    }
    bool empty() const
    {
        return random_access_list_base::empty();
    }
    std::size_t size() const
    {
        return random_access_list_base::size();
    }
    void pop_back()
    {
        node_type *node = static_cast<node_type *>(last_imp().node);
        pop_back_imp();
        delete node;
    }
    void pop_front()
    {
        node_type *node = static_cast<node_type *>(begin_imp().node);
        pop_front_imp();
        delete node;
    }
    void push_back(const T &v)
    {
        node_type *node = new node_type(v);
        push_back_imp(node);
    }
    void push_back(T &&v)
    {
        node_type *node = new node_type(std::move(v));
        push_back_imp(node);
    }
    template <typename ...Args>
    void emplace_back(Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_back_imp(node);
    }
    void push_front(const T &v)
    {
        node_type *node = new node_type(v);
        push_front_imp(node);
    }
    void push_front(T &&v)
    {
        node_type *node = new node_type(std::move(v));
        push_front_imp(node);
    }
    template <typename ...Args>
    void emplace_front(Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        push_front_imp(node);
    }
    template <typename ...Args>
    iterator emplace(const_iterator pos, Args &&...args)
    {
        node_type *node = new node_type(std::forward<Args>(args)...);
        insert_imp(node, pos.iter);
        return iterator(iterator_imp(node));
    }
    iterator insert(const_iterator pos, const T &v)
    {
        return emplace(pos, v);
    }
    iterator insert(const_iterator pos, T &&v)
    {
        return emplace(pos, std::move(v));
    }
    iterator erase(const_iterator pos)
    {
        node_type *node = static_cast<node_type *>(pos.iter.node);
        iterator retval(pos.iter);
        ++retval;
        remove_imp(pos.iter);
        delete node;
        return retval;
    }
    void resize(std::size_t count)
    {
        std::size_t current_size = size();
        while(count < current_size)
        {
            pop_back();
            current_size--;
        }
        while(count > current_size)
        {
            emplace_back();
            current_size++;
        }
    }
    void resize(std::size_t count, const T &value)
    {
        std::size_t current_size = size();
        while(count < current_size)
        {
            pop_back();
            current_size--;
        }
        while(count > current_size)
        {
            push_back(value);
            current_size++;
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
        node_type *node = static_cast<node_type *>(pos.iter.node);
        other.remove_imp(it.iter.node);
        insert_imp(node, pos.iter);
    }
    void splice(const_iterator pos, random_access_list &&other, const_iterator it)
    {
        if(&other == this && it == pos)
            return;
        node_type *node = static_cast<node_type *>(pos.iter.node);
        other.remove_imp(it.iter.node);
        insert_imp(node, pos.iter);
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
    void verify_all()
    {
        random_access_list_base::verify_all();
    }
};

#endif // RANDOM_ACCESS_LIST_H_INCLUDED
