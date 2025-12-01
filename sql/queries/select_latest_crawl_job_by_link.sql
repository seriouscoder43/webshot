select id
from crawl_job
where link = $1
order by created_at desc, id desc
limit 1;
