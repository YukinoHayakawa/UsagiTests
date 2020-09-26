// Compile this file with -Xclang -freflection

/*
#include <gtest/gtest.h>

TEST(,)
{
}
*/

// ReSharper Disable All

#include <Usagi/Entity/Component.hpp>

using namespace usagi;

#include <experimental/meta>
#include <experimental/compiler>

using namespace std::experimental;

#include <fmt/printf.h>

namespace
{
struct A
{
    int a;
    char b;
    float c;
    char d[16];
};
}

constexpr auto print = fragment
{
    fmt::print(
        "offset={}, type={}, name={}\n",
        ((::size_t)&reinterpret_cast<char const volatile&>((((typename(${clazz})*)0)->idexpr(${member})))),
        meta::name_of(meta::type_of(m)),
        meta::name_of(m)
    );
}


template <typename T>
void print_members()
{
    consteval
    {
        constexpr auto clazz = reflexpr(T);
        for(meta::info member : meta::data_member_range(clazz))
        {
            -> print;
        }
    }
}




consteval int test()
{
    consteval
    {
        constexpr auto clazz = reflexpr(A);
        // A::unqualid(meta::name_of(reflexpr(typename(clazz)::a)));
        for(meta::info member : meta::data_member_range(clazz))
        {
            // __reflect_dump(member);
            -> fragment {
                constexpr meta::info m = %{member};
                typename(clazz) *ptr = nullptr;
                static_cast<void*>(std::addressof(ptr->idexpr(m)));
                    // meta::info m = member;
                // __reflect_dump(m);
                __reflect_print(
                    meta::name_of(m),
                    " ",
                    meta::name_of(meta::type_of(m)),
                    " ",
                    // - msvc way, uses reinterpret_cast so can't be constexpr
                    // ((::size_t)&reinterpret_cast<char const volatile&>((((typename(${clazz})*)0)->idexpr(m)))),
                    // - similar to above
                    // (size_t)(&(((typename(clazz)*)0)->*&typename(clazz)::unqualid(meta::name_of(m)))),
                    // - doesn't work. error : expected identifier
                    // offsetof(typename(clazz), idexpr(m)),
                    // - doesn't work. error : no member named '__identifier_splice' in 'A'
                    // offsetof(typename(clazz), unqualid(meta::name_of(m))),
                    ""
                );
            };

            /*
            -> fragment {
                constexpr meta::info m = %{member};
                // sizeof(A::a);
                constexpr A *p = nullptr;
                // p->a;
                // p->unqualid(meta::name_of(m));
                p->idexpr(m);
                //
                // __reflect_print()
                //
                // void * x = p->*&typename(clazz)::unqualid(meta::name_of(m));
                // void * x2 = &typename(clazz)::unqualid(meta::name_of(m));


                // correct rhs type
                __reflect_print(meta::name_of(reflexpr(typename(clazz)*)));
                // auto y = ((::size_t)&reinterpret_cast<char const volatile&>((((typename(clazz)*)0)->unqualid(meta::name_of(m)))));
                // error : no member named '__identifier_splice' in 'A'
                // offsetof(typename(clazz), unqualid(meta::name_of(m)));
                #1#
                __reflect_print(
                    __concatenate(
                        meta::name_of(m),
                        " ",
                        meta::name_of(meta::type_of(m)),
                        " ",
                        sizeof(typename(clazz)::unqualid(__reflect(meta::detail::query_get_name, m)))
                        // static_cast<void*>(&typename(clazz)::unqualid(meta::name_of(m)))
                        // offsetof(typename(clazz), unqualid(meta::name_of(m)))
                    )
                );
            };*/
        }
    }

    return 1;
}

constexpr auto a = test();
// constexpr auto a = test<A>();
