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

#include "util/random_access_list.h"
#include <iostream>
#include <unordered_map>
#include <cassert>
#include <random>
#include <functional>
#include <deque>
#include <sstream>

#ifdef NDEBUG
#define verify(a) do {a;} while(0)
#else
#define verify(a) assert(a)
#endif // NDEBUG

std::ostream *dumpOutputStream = &std::cout;

template <size_t>
class Item final
{
private:
    static std::unordered_map<const Item *, std::size_t> items;
    static std::size_t getNextItem()
    {
        static std::size_t retval = 1;
        return retval++;
    }
    bool movedFrom;
public:
    std::size_t id() const
    {
        auto iter = items.find(this);
        if(iter == items.end())
            return 0;
        return std::get<1>(*iter);
    }
    void check() const
    {
        assert(!movedFrom);
        assert(items.count(this));
    }
    Item()
    {
        movedFrom = false;
        verify(std::get<1>(items.emplace(this, getNextItem())));
    }
    Item(const Item &rt)
    {
        movedFrom = false;
        assert(!rt.movedFrom);
        assert(items.count(&rt));
        verify(std::get<1>(items.emplace(this, getNextItem())));
    }
    Item(Item &&rt)
    {
        movedFrom = false;
        assert(!rt.movedFrom);
        rt.movedFrom = true;
        assert(items.count(&rt));
        verify(std::get<1>(items.emplace(this, getNextItem())));

    }
    ~Item()
    {
        verify(items.erase(this));
    }
    Item &operator =(Item &&rt)
    {
        movedFrom = false;
        assert(!rt.movedFrom);
        rt.movedFrom = true;
        assert(items.count(this));
        assert(items.count(&rt));
        return *this;
    }
    Item &operator =(const Item &rt)
    {
        movedFrom = false;
        assert(!rt.movedFrom);
        assert(items.count(this));
        assert(items.count(&rt));
        return *this;
    }
};

template <size_t N>
std::unordered_map<const Item<N> *, std::size_t> Item<N>::items;

void test()
{
    random_access_list<Item<1>> l1;
    std::list<Item<2>> l2;
    std::default_random_engine rg(214);
    std::function<void()> functions[] =
    {
        [&]()
        {
            *dumpOutputStream << "push_back" << std::endl;
            l1.push_back(Item<1>());
            l2.push_back(Item<2>());
        },
        [&]()
        {
            *dumpOutputStream << "push_front" << std::endl;
            l1.push_front(Item<1>());
            l2.push_front(Item<2>());
        },
        [&]()
        {
            if(l1.empty() || l2.empty())
                return;
            *dumpOutputStream << "pop_back" << std::endl;
            l1.pop_back();
            l2.pop_back();
        },
        [&]()
        {
            if(l1.empty() || l2.empty())
                return;
            *dumpOutputStream << "pop_front" << std::endl;
            l1.pop_front();
            l2.pop_front();
        },
        [&]()
        {
            std::size_t position = std::uniform_int_distribution<std::size_t>(0, l1.size())(rg);
            if(position > l1.size() || position > l2.size())
                return;
            *dumpOutputStream << "insert " << position << std::endl;
            l1.insert(l1.begin() + position, Item<1>());
            auto l2p = l2.begin();
            std::advance(l2p, position);
            l2.insert(l2p, Item<2>());
        },
        [&]()
        {
            if(l1.empty() || l2.empty())
                return;
            std::size_t position = std::uniform_int_distribution<std::size_t>(0, l1.size() - 1)(rg);
            if(position >= l1.size() || position >= l2.size())
                return;
            *dumpOutputStream << "erase " << position << std::endl;
            l1.erase(l1.begin() + position);
            auto l2p = l2.begin();
            std::advance(l2p, position);
            l2.erase(l2p);
        },
    };
    const std::size_t stepCount = 13;
    for(std::size_t i = 0; i < stepCount; i++)
    {
        *dumpOutputStream << i << ":\n";
        if(i == stepCount - 1)
            *dumpOutputStream << std::flush;
        functions[std::uniform_int_distribution<std::size_t>(0, sizeof(functions) / sizeof(functions[0]) - 1)(rg)]();
        l1.verify_all();
        *dumpOutputStream << l1.size() << " [";
        const char *seperator = "";
        for(auto &v : l1)
        {
            *dumpOutputStream << seperator;
            seperator = ", ";
            *dumpOutputStream << v.id();
            v.check();
        }
        *dumpOutputStream << "]" << std::endl;
        *dumpOutputStream << l2.size() << " [";
        seperator = "";
        for(auto &v : l2)
        {
            *dumpOutputStream << seperator;
            seperator = ", ";
            *dumpOutputStream << v.id();
            v.check();
        }
        *dumpOutputStream << "]" << std::endl;
    }
}

int main()
{
    test();
    return 0;
}
