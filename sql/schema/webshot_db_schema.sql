create table webshot (
    id uuid primary key default gen_random_uuid(),
    created_at timestamptz not null default now(),
    url text collate "C" not null
);

create index items_url_prefix_idx on webshot(url, created_at desc, id);
