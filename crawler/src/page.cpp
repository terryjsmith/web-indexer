
#include <stdlib.h>
#include <regex.h>
#include <url.h>
#include <page.h>

Page::Page() {
	url = 0;
	content = 0;
}

Page::~Page() {
	if(url) {
		delete url;
		url = 0;
	}

	if(content) {
		free(content);
		content = 0;
	}
}
