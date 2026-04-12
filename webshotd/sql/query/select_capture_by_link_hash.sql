-- $1: normalized scheme-less link
-- $2: content_sha256 (sha256 bytes, 32 bytes)
select
    id,
    created_at
from capture
where link = $1 and content_sha256 = $2
order by created_at desc, id asc
limit 1
