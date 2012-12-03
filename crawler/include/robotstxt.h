
#ifndef __ROBOTSTXT_H__
#define __ROBOTSTXT_H__

class RobotsTxt {
public:
	RobotsTxt();
	~RobotsTxt();

	// Load the rules for a domain from the database
	void Load(URL* url, MYSQL* conn);

	// Check the rules
	bool Check(URL* url);

protected:
	char** m_rules;
	unsigned int m_count;
};

#endif
