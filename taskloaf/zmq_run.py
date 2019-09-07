import taskloaf.defaults as defaults
from .zmq_cluster import zmq_cluster, zmq_launcher


def new_client(f, meet_addr, hostname=None, port=None):
    if hostname is None:
        hostname = defaults.localhost
    if port is None:
        port = defaults.base_port
    zmq_launcher((hostname, port), None, meet_addr, f=f)


def run(f, n_workers=None, hostname=None, ports=None):

    if ports is None:
        client_port = None
        cluster_ports = None
    else:
        client_port = ports[0]
        cluster_ports = ports[1:]

    with zmq_cluster(
        n_workers=n_workers, hostname=hostname, ports=cluster_ports
    ) as cluster:
        new_client(f, cluster.addr(), hostname=hostname, port=client_port)
        # with zmq_client(
        #     cluster, hostname=hostname, port=client_port
        # ) as client:
        #     submission = client.submit(f)
        #     return client.wait(submission)
