-- name: select_crawl_job
select
    id,
    link,
    status,
    error_category,
    error_message,
    created_at,
    started_at,
    finished_at,
    result_created_at,
    result_capture_id
from crawl_job
where id = $1;
