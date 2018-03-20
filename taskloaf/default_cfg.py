defaults = dict(
    block_root_path = '/dev/shm/taskloaf_',
    cpu_affinity = None,
    zmq_addr = 'tcp://127.0.0.1:5755',
    zmq_join_addr = None
)

def setup_cfg(input_cfg):
    if input_cfg is None:
        input_cfg = dict()
    cfg = dict()
    for k, v in defaults.items():
        if k in input_cfg:
            cfg[k] = input_cfg[k]
        else:
            cfg[k] = defaults[k]
    return cfg
