#include <stdlib.h>
#include <regex.h>
#include <url.h>
#include <page.h>

Page::Page(char* url, char* content) {
	url = (char*)malloc(strlen(url) + 1);
	strcpy(this->url, url);

	content = (char*)malloc(strlen(content) + 1);
	strcpy(this->content, content);
}

Page::~Page() {
	if(url) {
		free(url);
		url = 0;
	}

	if(content) {
		free(content);
		content = 0;
	}
}
