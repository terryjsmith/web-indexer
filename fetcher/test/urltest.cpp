
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

#include <string.h>
#include <regex.h>
#include <url.h>

int main(int argc, char** argv) {
	char* base = "http://www.google.com/sub/";
	char* filename = "urltest.txt";

	vector<string> test_urls;
	FILE* fp = fopen(filename, "r");

	char* line = (char*)malloc(200);
	while(fgets(line, 200, fp) != NULL) {
		char* url = (char*)malloc(strlen(line) - 1);
		memcpy(url, line, strlen(line) - 1);
		url[strlen(line) - 1] = '\0';

		test_urls.push_back(url);
	}

	cout << "Parsing base URL " << base << "...";
	URL* url = new URL(base);
	cout << "done.\n";

	url->Parse(0);

	cout << "Scheme: " << url->parts[URL_SCHEME] << ", domain: " << url->parts[URL_DOMAIN] << ", path: " << url->parts[URL_PATH] << "\n\n";
	
	for(unsigned int i = test_urls.size() - 1; i > 0; i--) {
		cout << "Parsing URL " << test_urls[i].c_str() << ":\n";
		URL* tester = new URL((char*)test_urls[i].c_str());
		tester->Parse(url);
		
		cout << "Scheme: " << tester->parts[URL_SCHEME] << ", domain: " << tester->parts[URL_DOMAIN] << ", path: " << tester->parts[URL_PATH] << ", query: " << tester->parts[URL_QUERY] << "\n\n";
		delete tester;
	}

	cout << "Tests complete.\n";

	delete url;

	return(0);
}
