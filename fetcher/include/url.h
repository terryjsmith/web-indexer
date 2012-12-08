
#ifndef __URL_H__
#define __URL_H__

#define URL_SCHEME	0
#define URL_DOMAIN	1
#define URL_PATH	2
#define URL_QUERY	3

class Url {
public:
	Url(char* url);
	~Url();

	// Parse the URL, relative to the base URL
	bool parse(URL* base);

	// Clone this URL into a new one
	Url* clone();

	// Getters
	char* get_url();
	char* get_scheme();
	char* get_host();
	char* get_path();
	char* get_path_hash();
	char* get_query();

	unsigned int long get_domain_id();
	unsigned int long get_url_id();

protected:
	// The URL parts, in order of the defines above
	char* m_parts[4];

	// An internal copy of the full URL
	char* m_url;

	// An MD5 hash of the URL (specifically the path for now)
	char* m_hash;

	// The domain ID of the URL if it's known
	unsigned int long m_domain_id;

	// The URL id of the URL if it's known
	unsigned int long m_url_id;

	// Split this URL into it's parts
	bool _split();

	// Get the regular expression we use to parse URLs
        static regex_t* _get_regex();

	// The global URL parsing regex
	static regex_t* m_regex;
};

#endif
