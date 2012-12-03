
#ifndef __SITE_H__
#define __SITE_H__

class Site {
public:
	// Load a domain in by it's string name, create if it doens't exist
	void Load(char* domain, MYSQL* conn);

	// Get/set the last access time for this domain
	unsigned long int GetLastAccess();
	void SetLastAccess(unsigned long int timestamp);

public:
	unsigned long int domain_id;

protected:
	MYSQL* m_conn;
};

#endif
