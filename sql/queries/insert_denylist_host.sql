insert into host_denylist(host, reason)
values ($1, $2)
on conflict (host) do nothing
