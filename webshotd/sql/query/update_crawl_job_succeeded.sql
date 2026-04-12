-- name: update_crawl_job_succeeded
update crawl_job
set
    status = 'succeeded',
    finished_at = now(),
    error_category = null,
    error_message = null,
    result_created_at = $2,
    result_capture_id = $3
where id = $1 and status = 'running'
returning
    id,
    (extract(epoch from (finished_at - started_at)) * 1000)::bigint as duration_ms;
