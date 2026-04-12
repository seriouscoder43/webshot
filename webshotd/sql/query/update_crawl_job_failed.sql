update crawl_job
set
    status = 'failed',
    started_at = coalesce(started_at, now()),
    finished_at = now(),
    error_category = $2,
    error_message = $3
where id = $1 and status in ('pending', 'running')
returning
    id,
    (extract(epoch from (finished_at - started_at)) * 1000)::bigint as duration_ms;
