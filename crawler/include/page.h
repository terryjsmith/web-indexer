
#ifndef __PAGE_H__
#define __PAGE_H__

class Page {
public:
	Page();
	~Page();

public:
	URL* url;
	char* content;
};

#endif
