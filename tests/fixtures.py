import pytest
from taskloaf.run import null_comm_worker

@pytest.fixture(scope = "function")
def w():
    with null_comm_worker() as worker:
        yield worker

