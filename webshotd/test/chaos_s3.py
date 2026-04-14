import uuid

import pytest
from helper.constants import TEST_HOST
from helper.waiters import wait_for_job_status


@pytest.mark.asyncio
async def test_s3_outage_marks_job_failed(service_client, s3_gate, pgsql):
    await s3_gate.sockets_close()
    await s3_gate.stop_accepting()

    link = f"https://{TEST_HOST}/chaos-s3-failure"
    resp = await service_client.post("/v1/capture", json={"link": link})
    assert resp.status == 202
    job_id = resp.json()["uuid"]

    job = await wait_for_job_status(service_client, job_id, expected_status="failed")
    assert job["status"] == "failed"

    db = pgsql["capture_meta_db"]
    with db.cursor() as cur:
        cur.execute("select 1 from capture where id = %s", (uuid.UUID(job_id),))
        assert cur.fetchone() is None

    await s3_gate.to_server_pass()
    await s3_gate.to_client_pass()
    s3_gate.start_accepting()


@pytest.mark.asyncio
async def test_s3_recovers_after_outage(service_client, s3_gate):
    await s3_gate.to_server_pass()
    await s3_gate.to_client_pass()
    s3_gate.start_accepting()

    link = f"https://{TEST_HOST}/chaos-s3-recover"
    resp = await service_client.post("/v1/capture", json={"link": link})
    assert resp.status == 202
    job_id = resp.json()["uuid"]

    await wait_for_job_status(service_client, job_id, expected_status="succeeded")
