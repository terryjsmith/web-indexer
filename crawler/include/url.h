
#ifndef __URL_H__
#define __URL_H__

#define URL_SCHEME	0
#define URL_DOMAIN	1
#define URL_PATH	2
#define URL_QUERY	3

class URL {
public:
	URL();
	~URL();

	bool Parse(char* url);

public:
	// Get the regular expression we use to parse URLs
	regex_t* GetRegex();

	char* parts[4];
	char* url;

protected:
	static regex_t* m_regex;
};

#endif
