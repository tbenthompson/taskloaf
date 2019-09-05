from .zmq_cluster import zmq_cluster
from .zmq_client import zmq_client


def run(f, n_workers=None, hostname="tcp://127.0.0.1", ports=None):

    if ports is None:
        client_port = None
        cluster_ports = None
    else:
        client_port = ports[0]
        cluster_ports = ports[1:]

    with zmq_cluster(
        n_workers=n_workers, hostname=hostname, ports=cluster_ports
    ) as cluster:
        with zmq_client(
            cluster, hostname=hostname, port=client_port
        ) as client:
            submission = client.submit(f)
            return client.wait(submission)
