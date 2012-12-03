
#include <iostream>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <openssl/md5.h>
#include <url.h>

regex_t* URL::m_regex = 0;

URL::URL(char* url) {
	// Initialize
	parts[0] = parts[1] = parts[2] = parts[3] = 0;
	hash = 0;
	domain_id = url_id = 0;

	// Save a copy of our URL
        this->url = (char*)malloc(strlen(url) + 1);
	strcpy(this->url, url);
}

URL::~URL() {
	if(url) {
		free(url);
		url = 0;
	}

	if(hash) {
		free(hash);
		hash = 0;
	}

	for(int i = 0; i < 4; i++) {
		if(parts[i]) {
			free(parts[i]);
			parts[i] = 0;
		}
	}
}

regex_t* URL::GetRegex() {
	// See if our regex is set up; if not, set it up
        if(m_regex == 0) {
                // Create a new compiled regex
                m_regex = new regex_t;

                // Compile our regular expression
                regcomp(m_regex, "([A-Z]+)://([A-Z0-9\\.-]+)(/*[^\\?]*)\\?*([^#]*)#*.*", REG_EXTENDED | REG_ICASE);
        }

	return(m_regex);
}

URL* URL::Clone() {
	URL* temp = new URL(this->url);
	for(unsigned int i = 0; i < 4; i++) {
		temp->parts[i] = (char*)malloc(strlen(this->parts[i]) + 1);
		strcpy(temp->parts[i], this->parts[i]);
	}

	if(hash) {
		temp->hash = (char*)malloc(strlen(this->hash) + 1);
		strcpy(temp->hash, this->hash);
	}

	return(temp);
}

bool URL::Parse(URL* base) {
	// Check if this URL is already absolute
	if(strncmp(url, "http", 4) == 0) {
		return(_split());
	}

	// Check for a URL relative to the site root
	if(strncmp(url, "/", 1) == 0) {
		// Initialize a temporary URL to put it all together
		unsigned int copy_length = strlen(base->parts[URL_SCHEME]) + 3 + strlen(base->parts[URL_DOMAIN]);
		unsigned int length = copy_length + strlen(url);
		char* complete = (char*)malloc(length + 1);

		memcpy(complete, base->url, copy_length);
		memcpy(complete + copy_length, url, strlen(url));
		complete[length] = '\0';

		// Free the current URL and replace it with the complete URL for splitting
		free(url);
		url = complete;

		return(_split());
	}

	// Check for mailto and javascript links
	if((strncmp(url, "mailto:", 7) == 0) || (strncmp(url, "javascript:", 11) == 0)) {
		return(false);
	}
	
	// Strip out any "current directory" starting part of the URL (./)
	if(strncmp(url, "./", 2) == 0) {
		unsigned int length = strlen(url) - 2;
		char* temp = (char*)malloc(length + 1);
		strcpy(temp, url + 2);

		// Free the URL and copy
		free(url);
		url = temp;
	}

	// Prepend the path part of the base URL onto this URL
	unsigned int copy_length = strlen(base->parts[URL_PATH]) - 1;
	unsigned int url_length = strlen(url);

	char* temp = (char*)malloc(url_length + copy_length);
	strncpy(temp, base->parts[URL_PATH] + 1, copy_length);
	strcpy(temp + copy_length, url);

	free(url);
	url = temp;

	// Break the URL into path parts, dividing along each forward slash (/); start by finding the numbers of parts
	char* pointer = url;

	char* position = 0;
	unsigned int count = 1;
	while((position = strchr(pointer, '/')) != 0) {
		count++;
		pointer = position + 1;
	}

	// Now that we know how many parts, separate
	pointer = url;
	char** path_parts = (char**)malloc(count * sizeof(char*));

	// Loop over each part and copy it in
	for(unsigned int i = 0; i < count; i++) {
		char* found = strchr(pointer, '/');
		int length = found ? found - pointer : strlen(pointer);

		char* part = (char*)malloc(length + 1);
		memcpy(part, pointer, length);
		part[length] = '\0';

		path_parts[i] = part;

		// Increment pointer
		pointer = pointer + (length + 1);
	}

	// Iterate over the parts, getting rid of any directories that need to be recursed (../, ./)
	char* final_path =  (char*)malloc(strlen(url) + 2);
	final_path[strlen(url) + 1] = '\0';

	// Start from the end of the string
	int offset = strlen(url) + 1;

	// This code is a little bit backwards; to be efficient, we now recurse backwards over the path parts, prepending them to the final path we already have
	for(int i = count - 1; i >= 0; i--) {
		// If this part is recursed, just move on, skipping the next part as well
		if(strcmp(path_parts[i], "..") == 0) {
			i--;
			continue;
		}

		if(strcmp(path_parts[i], ".") == 0) {
			continue;
		}

		// Otherwise, add it on to the final path, backwards
		unsigned int part_length = strlen(path_parts[i]);
		offset -= part_length;
		strncpy(final_path + offset, path_parts[i], part_length);

		offset--;
		final_path[offset] = '/';
	}

	// Clean up the path parts
	for(unsigned int i = 0; i < count; i++) {
		free(path_parts[i]);
	}
	free(path_parts);

	// Free the URL as it is now and re-create it
	unsigned int final_path_length = strlen(url) - offset;
	unsigned int base_copy_length = strlen(base->parts[URL_SCHEME]) + 3 + strlen(base->parts[URL_DOMAIN]);

	free(url);

	url = (char*)malloc(base_copy_length + final_path_length + 1);
	memcpy(url, base->url, base_copy_length);
	strcpy(url + base_copy_length, final_path + offset);

	// Free the final path temp
	free(final_path);

	return(_split());
}

bool URL::_split() {
        // Get our regular expression
        regex_t* regex = URL::GetRegex();

        // Parse the URL
        regmatch_t re_matches[regex->re_nsub + 1];

        // Execute our regex
        if(regexec(regex, this->url, regex->re_nsub + 1, re_matches, 0) == REG_NOMATCH)
                return(false);

	// Loop through the expected number of matches
        for(unsigned int i = 1; i <= m_regex->re_nsub; i++) {
		unsigned int index = i - 1;
		parts[index] = (char*)malloc(re_matches[i].rm_eo - re_matches[i].rm_so + 1);
		memcpy(parts[index], url + re_matches[i].rm_so, re_matches[i].rm_eo - re_matches[i].rm_so);
		parts[index][re_matches[i].rm_eo - re_matches[i].rm_so] = '\0';
	}

	// Convert the scheme and domain to lowercase
	for(unsigned int i = 0; i < strlen(parts[URL_SCHEME]); i++)
		parts[URL_SCHEME][i] = tolower(parts[URL_SCHEME][i]);

	for(unsigned int i = 0; i < strlen(parts[URL_DOMAIN]); i++)
		parts[URL_DOMAIN][i] = tolower(parts[URL_DOMAIN][i]);

	// Get the MD5 hash of the path
        unsigned char temp[MD5_DIGEST_LENGTH];
        memset(temp, 0, MD5_DIGEST_LENGTH);
        MD5((const unsigned char*)parts[URL_PATH], strlen(parts[URL_PATH]), temp);

        // Convert it to hex
        hash = (char*)malloc((MD5_DIGEST_LENGTH * 2) + 1);
        for(unsigned int k = 0; k < MD5_DIGEST_LENGTH; k++) {
                sprintf(hash + (k * 2), "%02x", temp[k]);
        }
        hash[MD5_DIGEST_LENGTH * 2] = '\0';

        return(true);
}
