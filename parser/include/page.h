#ifndef __PAGE_H__
#define __PAGE_H__

class Page {
public:
	Page(char* url, char* content);
	~Page();

public:
	URL* url;
	char* content;
};

#endif
