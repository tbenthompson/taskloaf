#include "future.hpp"
#include "worker.hpp"

namespace taskloaf {

IVarRef plan_then(const IVarRef& input, PureTaskT task) {
    IVarRef out_future(new_id());
    cur_worker->add_trigger(input, {
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            cur_worker->add_task({
                [] (std::vector<Data>& d) {
                    auto& out_future = d[0].get_as<IVarRef>();
                    auto& fnc = d[1].get_as<PureTaskT>();
                    auto& vals = d[2].get_as<std::vector<Data>>();
                    cur_worker->fulfill(out_future, {fnc(vals)});
                },
                {d[0], d[1], make_data(vals)}
            });
        },
        {make_data(out_future), make_data(std::move(task))}
    });
    return std::move(out_future);
}

IVarRef plan_unwrap(const IVarRef& input, PureTaskT unwrapper) {
    IVarRef out_future(new_id());
    (void)input; (void)unwrapper;
    cur_worker->add_trigger(input, {
        [] (std::vector<Data>& d, std::vector<Data>& vals) {
            auto& fnc = d[1].get_as<PureTaskT>();
            auto result = fnc(vals).get_as<IVarRef>();
            cur_worker->add_trigger(result, {
                [] (std::vector<Data>& d, std::vector<Data>& vals) {
                    cur_worker->fulfill(d[0].get_as<IVarRef>(), vals); 
                },
                {d[0]}
            });
        },
        { make_data(out_future), make_data(std::move(unwrapper)) }
    });
    return std::move(out_future);
}

IVarRef plan_ready(Data data) {
    IVarRef out_future(new_id());
    cur_worker->fulfill(out_future, {std::move(data)});
    return std::move(out_future);
}

IVarRef plan_async(PureTaskT task) {
    IVarRef out_future(new_id());
    cur_worker->add_task({
        [] (std::vector<Data>& d) {
            auto& out_future = d[0].get_as<IVarRef>();
            auto& fnc = d[1].get_as<PureTaskT>(); 
            std::vector<Data> empty;
            cur_worker->fulfill(out_future, {fnc(empty)});
        },
        {make_data(out_future), make_data(std::move(task))}
    });
    return std::move(out_future);
}

void whenall_child(std::vector<IVarRef> child_results, size_t next_idx,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto next_result = std::move(child_results[next_idx]);
    if (next_idx == child_results.size() - 1) {
        cur_worker->add_trigger(next_result, {
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& result = d[0].get_as<IVarRef>();
                auto& accumulator = d[1].get_as<std::vector<Data>>();
                accumulator.push_back(vals[0]);
                cur_worker->fulfill(result, std::move(accumulator));
            },
            { make_data(result), make_data(std::move(accumulator)) }
        });
    } else {
        cur_worker->add_trigger(next_result, {
            [] (std::vector<Data>& d, std::vector<Data>& vals) {
                auto& child_results = d[0].get_as<std::vector<IVarRef>>();
                auto& next_idx = d[1].get_as<size_t>();
                auto& accumulator = d[2].get_as<std::vector<Data>>();
                auto& result = d[3].get_as<IVarRef>();
                accumulator.push_back(vals[0]);
                whenall_child(std::move(child_results), next_idx + 1,
                    std::move(accumulator), std::move(result)
                );
            },
            {
                make_data(std::move(child_results)), make_data(next_idx),
                make_data(std::move(accumulator)), make_data(std::move(result)) 
            }
        });
    }
}

IVarRef plan_when_all(std::vector<IVarRef> inputs) {
    IVarRef out_future(new_id());
    whenall_child(std::move(inputs), 0, {}, out_future);
    return std::move(out_future);
}
} //end namespace taskloaf
