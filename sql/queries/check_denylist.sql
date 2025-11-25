-- $1: text[] of host_rev prefixes (exact host_rev then parent prefixes)
select 1
from host_denylist
where host_rev = any($1)
limit 1
