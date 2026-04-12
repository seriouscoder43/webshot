import asyncio
import uuid

import pytest
from helper.constants import TEST_HOST


async def _wait_for_job_succeeded(service_client, job_id: str, *, attempts: int = 120):
    for _ in range(attempts):
        status_resp = await service_client.get(f"/v1/capture/jobs/{job_id}")
        assert status_resp.status == 200
        job = status_resp.json()
        if job["status"] == "succeeded":
            return job
        if job["status"] == "failed":
            pytest.fail(f"job failed: {job}")
        await asyncio.sleep(0.5)
    pytest.fail("job did not complete in time")


@pytest.mark.asyncio
async def test_dedup_reuses_earlier_capture_uuid(service_client, pgsql):
    link = f"https://{TEST_HOST}/dedup-path"

    resp1 = await service_client.post("/v1/capture", json={"link": link})
    assert resp1.status == 202
    job1_id = resp1.json()["uuid"]
    job1 = await _wait_for_job_succeeded(service_client, job1_id)
    capture_id = job1["result"]["uuid"]
    normalized_link = job1["result"]["link"]

    # Ensure a new job is created even if link cooldown is enabled.
    db = pgsql["shared_state_db"]
    with db.cursor() as cur:
        cur.execute(
            "update crawl_job set created_at = created_at - interval '1 day' where id = %s",
            (uuid.UUID(job1_id),),
        )

    resp2 = await service_client.post("/v1/capture", json={"link": link})
    assert resp2.status == 202
    job2_id = resp2.json()["uuid"]
    assert job2_id != job1_id
    job2 = await _wait_for_job_succeeded(service_client, job2_id)

    assert job2["result"]["uuid"] == capture_id
    assert job2["result"]["link"] == normalized_link

    # Verify the deduped job did not create an additional capture row.
    db = pgsql["capture_meta_db"]
    with db.cursor() as cur:
        cur.execute("select count(*) from capture where link = %s", (normalized_link,))
        (cnt,) = cur.fetchone()
    assert cnt == 1
