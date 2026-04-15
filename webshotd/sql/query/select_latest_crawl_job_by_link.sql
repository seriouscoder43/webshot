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
where link = $1
order by created_at desc, id desc
limit 1;
