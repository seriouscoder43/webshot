delete from crawl_job
where created_at < $1;
