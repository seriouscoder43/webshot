import pathlib
import uuid

from helper.constants import TEST_HOST
from helper.prefix import prefix_key_from_link, prefix_tree_from_prefix_key
from helper.sql_loader import _adapt_positional_to_psycopg

_SQL_QUERIES_DIR = pathlib.Path(__file__).resolve().parents[1] / "sql" / "query"
INSERT_CAPTURE_SQL = _adapt_positional_to_psycopg(
    (_SQL_QUERIES_DIR / "insert_capture.sql").read_text()
)
DUMMY_SHA256 = b"\x00" * 32


def _replay_url(capture_id: uuid.UUID) -> str:
    return f"https://replay.invalid/{capture_id}"


async def test_list_captures_orders_by_created_at(
    service_client,
    pgsql,
):
    """Insert rows via pgsql and verify ordering for /v1/capture."""

    db = pgsql["capture_meta_db"]

    newer_id = uuid.uuid4()
    older_id = uuid.uuid4()

    cur = db.cursor()
    try:
        prefix_key = prefix_key_from_link(f"{TEST_HOST}/a")
        prefix_tree = prefix_tree_from_prefix_key(prefix_key)
        cur.execute(
            INSERT_CAPTURE_SQL,
            (
                newer_id,
                f"{TEST_HOST}/a",
                prefix_key,
                prefix_tree,
                DUMMY_SHA256,
                _replay_url(newer_id),
            ),
        )
        cur.execute(
            INSERT_CAPTURE_SQL,
            (
                older_id,
                f"{TEST_HOST}/a",
                prefix_key,
                prefix_tree,
                DUMMY_SHA256,
                _replay_url(older_id),
            ),
        )
    finally:
        cur.close()

    response = await service_client.get(
        "/v1/capture",
        params={"link": f"{TEST_HOST}/a"},
    )

    assert response.status == 200
    body = response.json()
    uuids = [item["uuid"] for item in body["items"]]
    assert set(uuids) == {str(newer_id), str(older_id)}


async def test_list_captures_prefix_sees_inserted_links(
    service_client,
    pgsql,
):
    """Insert two links sharing a prefix and list by prefix."""

    db = pgsql["capture_meta_db"]

    cur = db.cursor()
    try:
        capture_id_a = uuid.uuid4()
        prefix_key_a = prefix_key_from_link(f"{TEST_HOST}/prefix/a")
        prefix_tree_a = prefix_tree_from_prefix_key(prefix_key_a)
        cur.execute(
            INSERT_CAPTURE_SQL,
            (
                capture_id_a,
                f"{TEST_HOST}/prefix/a",
                prefix_key_a,
                prefix_tree_a,
                DUMMY_SHA256,
                _replay_url(capture_id_a),
            ),
        )
        capture_id_b = uuid.uuid4()
        prefix_key_b = prefix_key_from_link(f"{TEST_HOST}/prefix/b")
        prefix_tree_b = prefix_tree_from_prefix_key(prefix_key_b)
        cur.execute(
            INSERT_CAPTURE_SQL,
            (
                capture_id_b,
                f"{TEST_HOST}/prefix/b",
                prefix_key_b,
                prefix_tree_b,
                DUMMY_SHA256,
                _replay_url(capture_id_b),
            ),
        )
    finally:
        cur.close()

    response = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix"},
    )

    assert response.status == 200
    body = response.json()
    links = {item["link"] for item in body["items"]}
    assert {f"{TEST_HOST}/prefix/a", f"{TEST_HOST}/prefix/b"}.issubset(links)


async def test_list_captures_paged_previous_and_next(
    service_client,
    pgsql,
):
    """Verify /v1/capture uses page_token to paginate link results."""

    db = pgsql["capture_meta_db"]

    ids = [uuid.uuid4(), uuid.uuid4(), uuid.uuid4(), uuid.uuid4(), uuid.uuid4()]

    cur = db.cursor()
    try:
        prefix_key = prefix_key_from_link(f"{TEST_HOST}/a")
        prefix_tree = prefix_tree_from_prefix_key(prefix_key)
        # Three rows for the same link; created_at uses default now().
        for capture_id in ids:
            cur.execute(
                INSERT_CAPTURE_SQL,
                (
                    capture_id,
                    f"{TEST_HOST}/a",
                    prefix_key,
                    prefix_tree,
                    DUMMY_SHA256,
                    _replay_url(capture_id),
                ),
            )
    finally:
        cur.close()

    # First page: 2 items (page size), next_page_token present.
    resp1 = await service_client.get(
        "/v1/capture",
        params={"link": f"{TEST_HOST}/a"},
    )
    assert resp1.status == 200
    body1 = resp1.json()
    uuids1 = [item["uuid"] for item in body1["items"]]
    assert len(uuids1) == 2
    next_token = body1.get("next_page_token")
    assert next_token
    assert "previous_page_token" not in body1

    # Second page: 2 items, both directions available.
    resp2 = await service_client.get(
        "/v1/capture",
        params={"link": f"{TEST_HOST}/a", "page_token": next_token},
    )
    assert resp2.status == 200
    body2 = resp2.json()
    uuids2 = [item["uuid"] for item in body2["items"]]
    assert len(uuids2) == 2
    next_token2 = body2.get("next_page_token")
    assert next_token2
    previous_token = body2.get("previous_page_token")
    assert previous_token

    resp_previous = await service_client.get(
        "/v1/capture",
        params={"link": f"{TEST_HOST}/a", "page_token": previous_token},
    )
    assert resp_previous.status == 200
    body_previous = resp_previous.json()
    assert [item["uuid"] for item in body_previous["items"]] == uuids1
    assert "previous_page_token" not in body_previous
    assert body_previous.get("next_page_token")

    resp3 = await service_client.get(
        "/v1/capture",
        params={"link": f"{TEST_HOST}/a", "page_token": next_token2},
    )
    assert resp3.status == 200
    body3 = resp3.json()
    uuids3 = [item["uuid"] for item in body3["items"]]
    assert len(uuids3) == 1
    assert set(uuids1 + uuids2 + uuids3) == {str(capture_id) for capture_id in ids}
    assert "next_page_token" not in body3
    assert body3.get("previous_page_token")


async def test_list_captures_prefix_does_not_emit_false_next_page(
    service_client,
    pgsql,
):
    """A full per-link prefix page is not enough to promise another page."""

    db = pgsql["capture_meta_db"]
    link = f"{TEST_HOST}/false-next/a"

    cur = db.cursor()
    try:
        prefix_key = prefix_key_from_link(link)
        prefix_tree = prefix_tree_from_prefix_key(prefix_key)
        for _ in range(2):
            capture_id = uuid.uuid4()
            cur.execute(
                INSERT_CAPTURE_SQL,
                (
                    capture_id,
                    link,
                    prefix_key,
                    prefix_tree,
                    DUMMY_SHA256,
                    _replay_url(capture_id),
                ),
            )
    finally:
        cur.close()

    response = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/false-next"},
    )

    assert response.status == 200
    body = response.json()
    assert len(body["items"]) == 2
    assert "next_page_token" not in body
    assert "previous_page_token" not in body


async def test_list_captures_prefix_previous_page(
    service_client,
    pgsql,
):
    """Verify grouped prefix pagination can move back from the second link page."""

    db = pgsql["capture_meta_db"]

    cur = db.cursor()
    try:
        for idx in range(11):
            link = f"{TEST_HOST}/prefix-prev/{idx:02d}"
            capture_id = uuid.uuid4()
            prefix_key = prefix_key_from_link(link)
            prefix_tree = prefix_tree_from_prefix_key(prefix_key)
            cur.execute(
                INSERT_CAPTURE_SQL,
                (
                    capture_id,
                    link,
                    prefix_key,
                    prefix_tree,
                    DUMMY_SHA256,
                    _replay_url(capture_id),
                ),
            )
    finally:
        cur.close()

    resp1 = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev"},
    )
    assert resp1.status == 200
    body1 = resp1.json()
    links1 = [item["link"] for item in body1["items"]]
    assert links1 == [f"{TEST_HOST}/prefix-prev/{idx:02d}" for idx in range(10)]
    assert "previous_page_token" not in body1
    next_token = body1.get("next_page_token")
    assert next_token

    resp2 = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev", "page_token": next_token},
    )
    assert resp2.status == 200
    body2 = resp2.json()
    assert [item["link"] for item in body2["items"]] == [f"{TEST_HOST}/prefix-prev/10"]
    assert "next_page_token" not in body2
    previous_token = body2.get("previous_page_token")
    assert previous_token

    resp_previous = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev", "page_token": previous_token},
    )
    assert resp_previous.status == 200
    assert [item["link"] for item in resp_previous.json()["items"]] == links1


async def test_list_captures_prefix_previous_then_next_within_one_link(
    service_client,
    pgsql,
):
    """Going back inside one link must preserve the next within-link cursor."""

    db = pgsql["capture_meta_db"]
    link = f"{TEST_HOST}/prefix-prev-one-link/a"

    cur = db.cursor()
    try:
        prefix_key = prefix_key_from_link(link)
        prefix_tree = prefix_tree_from_prefix_key(prefix_key)
        for _ in range(3):
            capture_id = uuid.uuid4()
            cur.execute(
                INSERT_CAPTURE_SQL,
                (
                    capture_id,
                    link,
                    prefix_key,
                    prefix_tree,
                    DUMMY_SHA256,
                    _replay_url(capture_id),
                ),
            )
    finally:
        cur.close()

    resp1 = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev-one-link"},
    )
    assert resp1.status == 200
    body1 = resp1.json()
    uuids1 = [item["uuid"] for item in body1["items"]]
    assert len(uuids1) == 2
    next_token = body1.get("next_page_token")
    assert next_token

    resp2 = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev-one-link", "page_token": next_token},
    )
    assert resp2.status == 200
    body2 = resp2.json()
    uuids2 = [item["uuid"] for item in body2["items"]]
    assert len(uuids2) == 1
    previous_token = body2.get("previous_page_token")
    assert previous_token

    resp_previous = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev-one-link", "page_token": previous_token},
    )
    assert resp_previous.status == 200
    body_previous = resp_previous.json()
    assert [item["uuid"] for item in body_previous["items"]] == uuids1
    next_again_token = body_previous.get("next_page_token")
    assert next_again_token

    resp_next_again = await service_client.get(
        "/v1/capture/prefix",
        params={"prefix": f"{TEST_HOST}/prefix-prev-one-link", "page_token": next_again_token},
    )
    assert resp_next_again.status == 200
    assert [item["uuid"] for item in resp_next_again.json()["items"]] == uuids2
