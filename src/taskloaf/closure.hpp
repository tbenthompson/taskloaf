#pragma once

#include "fnc.hpp"
#include "data.hpp"
#include "execute.hpp"

#include <cereal/types/vector.hpp>

namespace taskloaf {

template <typename T> 
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args...)> fnc;
    std::vector<Data> input;

    Closure() = default;

    template <typename... As>
    Return operator()(As&&... args) {
        // No forwarding so that everything becomes an lvalue reference
        return fnc(input, args...);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(fnc);
        ar(input);
    }
};

typedef Closure<Data(std::vector<Data>&)> PureTaskT;
typedef Closure<void(std::vector<Data>&)> TriggerT;
typedef Closure<void()> TaskT;

template <size_t I, typename... AllArgs>
struct ClosureArgSeparator {};

template <>
struct ClosureArgSeparator<0> {
    template <typename F, typename... ClosureArgs>
    static auto on(std::vector<Data> args) {
        return Closure<std::result_of_t<F(ClosureArgs&...)>()>{
            [] (std::vector<Data>& d) {
                auto runner = 
                    [] (F& f, ClosureArgs&... closure_args) {
                        return f(closure_args...);
                    };
                return apply_data_args(std::move(runner), d);
            },
            std::move(args)
        };
    }
};

template <typename T, typename... FreeArgs>
struct ClosureArgSeparator<0,T,FreeArgs...> {
    template <typename F, typename... ClosureArgs>
    static auto on(std::vector<Data> args) {
        return Closure<
            std::result_of_t<F(ClosureArgs&...,T&,FreeArgs&...)>(T&,FreeArgs&...)
        >{
            [] (std::vector<Data>& d, T& a1, FreeArgs&... args) {
                auto runner = 
                    [] (F& f, ClosureArgs&... closure_args,
                        T& a1, FreeArgs&... free_args) 
                    {
                        return f(closure_args..., a1, free_args...);
                    };
                return apply_data_args(std::move(runner), d, a1, args...);
            },
            std::move(args)
        };
    }
};

template <size_t I, typename T, typename... AllArgs>
struct ClosureArgSeparator<I,T,AllArgs...> {
    template <typename F, typename... ClosureArgs>
    static auto on(std::vector<Data> args) {
        return ClosureArgSeparator<
            I-1,AllArgs...
        >::template on<F,ClosureArgs...>(std::move(args));
    }
};

template <typename F>
struct is_serializable: std::integral_constant<bool,
    cereal::traits::is_output_serializable<F,cereal::BinaryOutputArchive>::value &&
    cereal::traits::is_input_serializable<F,cereal::BinaryInputArchive>::value>
{};

template <typename... ClosureArgs>
struct MakeClosureHelper {
    template <typename F>
    struct Inner {};

    template <typename Return, typename... AllArgs>
    struct Inner<Return(AllArgs...)> {
        template <typename F, std::enable_if_t<is_serializable<F>::value,int> = 0> 
        static auto make_closure(F&& f, ClosureArgs&&... args) {
            auto tl_fnc = make_function([] (F& f, AllArgs&... all_args) {
                return f(all_args...);
            });
            std::vector<Data> closure_args{
                make_data(std::move(tl_fnc)),
                make_data(std::move(f)),
                make_data(std::forward<ClosureArgs>(args))...
            };
            return ClosureArgSeparator<
                sizeof...(ClosureArgs)+1,F,AllArgs...
            >::template on<decltype(tl_fnc),F,ClosureArgs...>(
                std::move(closure_args)
            );
        }

        template <typename F, std::enable_if_t<!is_serializable<F>::value,int> = 0>
        static auto make_closure(F&& f, ClosureArgs&&... args) {
            auto tl_fnc = make_function(std::forward<F>(f));
            std::vector<Data> closure_args{
                make_data(std::move(tl_fnc)),
                make_data(std::forward<ClosureArgs>(args))...
            };
            return ClosureArgSeparator<
                sizeof...(ClosureArgs),AllArgs...
            >::template on<decltype(tl_fnc),ClosureArgs...>(
                std::move(closure_args)
            );
        }
    };
};

template <typename F, typename... Args>
auto make_closure(F&& f, Args&&... args) {
    return MakeClosureHelper<
        Args...
    >::template Inner<get_signature<F>>::make_closure(
        std::forward<F>(f),
        std::forward<Args>(args)...
    );
}
    
// template <typename F, typename... Args>
// struct ClosureWrapper {
//     F f;
//     std::tuple<Args...> args;
// 
//     template <typename Archive>
//     void serialize(Archive& ar) {
//         auto closure = MakeClosureHelper<
//             Args...
//         >::template Inner<get_signature<F>>::make_closure(
//             std::forward<F>(f),
//             std::forward<Args>(args)...
//         );
// 
//         ar(closure);
//     }
// };

} //end namespace taskloaf

