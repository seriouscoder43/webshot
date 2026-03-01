create extension if not exists ltree;

create table capture (
    id uuid primary key,
    created_at timestamptz not null default now(),
    link text collate "C" not null,
    prefix_key text collate "C" not null,
    prefix_tree ltree not null,
    location text not null
);

-- For prefix/paged scans by link
create index if not exists capture_link_idx on capture (link, created_at desc, id);

-- For purges and denylist checks via prefix_tree
create index if not exists capture_prefix_tree_gist_idx on capture using gist (prefix_tree);
