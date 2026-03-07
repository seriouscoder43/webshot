-- $1: deny prefix_tree as text (ltree input)
-- $2: page size
select id
from capture
where prefix_tree <@ text2ltree($1)
order by created_at desc, id asc
limit $2
