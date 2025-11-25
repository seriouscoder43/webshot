insert into crawl_job (id, link, status)
values ($1, $2, 'pending')
returning created_at;
