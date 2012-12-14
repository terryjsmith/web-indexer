Open Web Index
==============

My simple web crawler in C for crawling and parsing the internet.

Fetcher
-------

The fetcher is responsible for fetching URLs from a Redis list and saving them to the disk to be processed. It is single-threaded and uses raw sockets; it is completely asynchronous, from DNS lookup to connection to sending and receiving data.  After a fetch is complete, it saves the page information into the database, including the full URL and the MD5 hash of the URL for fast look-ups.

### Flow

The main() function is responsible for forking off worker processes.  Each working is responsible for managing it's own pool of HTTP requests. Each connection is wrapped in an HttpRequest class and added to the pool; we writes the content back into the request's content variable and also to disk if a filename has been specified. 

The fetcher is responsible for not crawling the same URL twice, obeying robots.txt and keeping a per-domain check of how long it's been since the last page was fetched from a site.

Parser
------

The parser's job is to grab the already fetched HTML on disk and parse it for anything we might be looking for (right now just links).  It then saves the results back into the MySQL database.

### Flow

[TODO]
