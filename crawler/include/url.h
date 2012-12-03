
#ifndef __URL_H__
#define __URL_H__

#define URL_SCHEME	0
#define URL_DOMAIN	1
#define URL_PATH	2
#define URL_QUERY	3

class URL {
public:
	URL(char* url);
	~URL();

	// Parse the URL, relative to the base URL
	bool Parse(URL* base);

	// Clone this URL into a new one
	URL* Clone();

public:
	// Get the regular expression we use to parse URLs
	regex_t* GetRegex();

	// The URL parts, in order of the defines above
	char* parts[4];

	// An internal copy of the full URL
	char* url;

	// An MD5 hash of the URL (specifically the path for now)
	char* hash;

	// The domain ID of the URL if it's known
	unsigned int long domain_id;

	// The URL id of the URL if it's known
	unsigned int long url_id;

protected:
	// Split this URL into it's parts
	bool _split();

	// The global URL parsing regex
	static regex_t* m_regex;
};

#endif
