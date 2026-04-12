-- $4: prefix_tree as text (ltree input)
-- $6: content_sha256 (sha256 bytes, 32 bytes)
insert into capture (id, created_at, link, prefix_key, prefix_tree, location, content_sha256)
values ($1, default, $2, $3, text2ltree($4), $5, $6)
returning id, created_at
