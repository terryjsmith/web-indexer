Open Web Index Crawler
======================

My simple web crawler in C for crawling and parsing the internet.

Crawler
-------

The crawler is responsible for fetching URLs from a list and saving them to the disk to be processed. It is single-threaded and uses libcurl's multi API.  After a fetch, it saves the page information into the database, including the full URL and the MD5 hash of the URL for fast look-ups.

## Flow

The main() function is responsible for managing the HTTP connection pool via libcurl's multi functions. Each connection is wrapped in an HttpRequest class and added to the pool; curl writes the content back into the request's content variable.  When the request is complete, the contents are written to disk and added to another redis queue to be parsed for data and permanently stored.

The crawler is responsible for not crawling the same URL twice, obeying robots.txt and keeping a per-domain check of how long it's been since the last page was fetched from a site.

Parser
------

The parser's job is to grab the already fetched HTML on disk and parse it for anything we might be looking for (right now just links).  It then saves the results back into the MySQL database.

## Flow

[TODO]
