
#include <regex.h>
#include <url.h>

regex_t* URL::m_regex = 0;

URL::URL() {
	// Initialize
	url = parts[0] = parts[1] = parts[2] = parts[3] = 0;
}

URL::~URL() {
	if(url) {
		free(url);
		url = 0;
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
                regcomp(m_regex, "([A-Z]+)://([A-Z0-9\\.-]+)/*([^\\?]*)\\?*([^#]*)#*.*", REG_EXTENDED | REG_ICASE);
        }

	return(m_regex);
}

bool URL::Parse(char* base, char* url) {
	// Our final URL
	char* final;

	// Save a copy of our URL
        this->url = (char*)malloc(strlen(url) + 1);
        memcpy(this->url, url, strlen(url));
        this->url[strlen(url)] = '\0';

        // Get our regular expression
        regex = URL::GetRegex();

        // Parse the URL
        regmatch_t re_matches[regex->re_nsub + 1];

        // Execute our regex
        if(regexec(regex, input, regex->re_nsub + 1, re_matches, 0) == REG_NOMATCH)
                return(false);

	// Loop through the expected number of matches
        for(unsigned int i = 0; i <= m_regex->re_nsub; i++) {
		parts[i] = (char*)malloc(re_matches[i].rm_eo - re_matches[i].rm_so + 1);
		memcpy(parts[i], url + re_matches[i].rm_so, re_matches[i].rm_eo - re_matches[i].rm_so);
		parts[i][re_matches[i].rm_eo - re_matches[i].rm_so] = '\0';
	}

        return(true);
}
