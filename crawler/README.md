Open Web Index Crawler
======================

My simple web crawler in C for crawling and parsing the internet.

Flow
----

The main() function and thread is responsible for managing the HTTP connection pool via libcurl's multi functions. Each connection is wrapped in an HttpRequest class and added to the pool; curl writes the content back into the request's content variable.

When the request is complete, the URL and content are read out of the request object and into a Page class.  The Page object is added to the global PagePool -- a singleton global pool of pages that need to be parsed -- from which Parser objects can request more work.

The main thread spins up several Parser objects which are each their own thread; parser objects request pages to parse from the PagePool and handle the parsing of the HTML and subsequent storage of infomation in MySQL and adding URLs to parse in the Redis queue.
