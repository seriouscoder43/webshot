create extension if not exists ltree;

create table webshot (
    id uuid primary key,
    created_at timestamptz not null default now(),
    link text collate "C" not null,
    prefix_key text collate "C" not null,
    prefix_tree ltree not null,
    location text not null
);

-- For prefix/paged scans by link
create index if not exists webshot_link_idx on webshot (link, created_at desc, id);

-- For purges and denylist checks via prefix_tree
create index if not exists webshot_prefix_tree_gist_idx on webshot using gist (prefix_tree);
