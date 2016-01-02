#pragma once
#include "address.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

inline caf::actor connect(caf::blocking_actor* self, Address to_addr)
{
    auto mm = caf::io::get_middleman_actor();
    self->send(mm, caf::connect_atom::value, to_addr.hostname, to_addr.port); 

    caf::actor dest;
    bool connected = false;
    self->do_receive(
        [&](caf::ok_atom, caf::node_id&,
            caf::actor_addr& new_connection, std::set<std::string>&) 
        {
            if (new_connection == caf::invalid_actor_addr) {
                return;
            }

            dest = caf::actor_cast<caf::actor>(new_connection);
            connected = true;
        },
        [&](caf::error_atom, const std::string& errstr) {
            auto wait = 3;
            std::cout << "FAILED CONNECTION " << errstr << std::endl;
            self->delayed_send(
                mm, std::chrono::seconds(wait), caf::connect_atom::value,
                to_addr.hostname, to_addr.port
            );
        }
    ).until([&] {return connected; });

    return dest;
}

} //end namespace taskloaf
