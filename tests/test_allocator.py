# Design decisions time!
# Should I have an allocator? That question is pretty much the same question as...
# Should I have small objects that use shared memory?
# -- faster!
# -- downside is that it requires an allocator... whereas if I only put specific large objects in shared memory, then memory management falls on the user. this is both good and bad...
# Should I I allow objects to be serialized once and then sent along the wire many times?
# -- this allows much more efficient construction of multi-task operations like a "map" or "reduce"
# Should I store both the value and serialized value of an object?
# -- deserializing functions is nice when the deserialization only happens once...
