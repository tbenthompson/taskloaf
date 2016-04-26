#pragma once

#include "fnc.hpp"
#include "data.hpp"

#include <cereal/types/vector.hpp>

namespace taskloaf {

template <typename T> 
struct Closure {};

template <typename Return, typename... Args>
struct Closure<Return(Args...)> {
    Function<Return(std::vector<Data>&,Args...)> fnc;
    std::vector<Data> input;

    Closure() = default;

    template <typename F>
    Closure(F f, std::vector<Data> args):
        fnc(f),
        input(std::move(args))
    {}

    template <typename... As>
    Return operator()(As&&... args) {
        return fnc(input, std::forward<As>(args)...);
    }

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(fnc);
        ar(input);
    }
};

typedef Closure<Data(std::vector<Data>&)> PureTaskT;
typedef Closure<void(std::vector<Data>& val)> TriggerT;
typedef Closure<void()> TaskT;

template <size_t I, typename... AllArgs>
struct ClosureArgSeparator {};

template <>
struct ClosureArgSeparator<0> {
    template <typename F, typename... ClosureArgs>
    static auto on(std::vector<Data> args) {
        return Closure<std::result_of_t<F(ClosureArgs...)>()>(
            [] (std::vector<Data>& d) {
                auto runner = 
                    [] (F f, ClosureArgs&... closure_args) {
                        return f(closure_args...);
                    };
                return apply_args(std::move(runner), d);
            },
            std::move(args)
        );
    }
};

template <typename T, typename... FreeArgs>
struct ClosureArgSeparator<0,T,FreeArgs...> {
    template <typename F, typename... ClosureArgs>
    static auto on(std::vector<Data> args) {
        return Closure<
            std::result_of_t<F(ClosureArgs...,T,FreeArgs...)>(T,FreeArgs...)
        >(
            [] (std::vector<Data>& d, T& a1, FreeArgs&... args) {
                auto runner = 
                    [] (F f, ClosureArgs&... closure_args,
                        T& a1, FreeArgs&... free_args) 
                    {
                        return f(closure_args..., a1, free_args...);
                    };
                return apply_args(std::move(runner), d, a1, args...);
            },
            std::move(args)
        );
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
struct ClosureBuilder {};

template <typename Return, typename... AllArgs>
struct ClosureBuilder<Return(AllArgs...)> {
    template <typename F, typename... ClosureArgs>
    static auto on(F&& f, ClosureArgs&&... closure_args) {
        std::vector<Data> args{
            make_data(std::forward<F>(f)),
            make_data(std::forward<ClosureArgs>(closure_args))...
        };
        return ClosureArgSeparator<
            sizeof...(ClosureArgs),AllArgs...
        >::template on<F,ClosureArgs...>(std::move(args));
    }
};

template <typename F, typename... Args>
auto make_closure(F&& f, Args&&... args) {
    auto tl_fnc = make_function(std::forward<F>(f));
    return ClosureBuilder<get_signature<F>>::template on<decltype(tl_fnc),Args...>(
        std::move(tl_fnc), std::forward<Args>(args)...
    );
}

} //end namespace taskloaf

