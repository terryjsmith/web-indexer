
#include <vector>
using namespace std;

#include <pthread.h>
#include <regex.h>
#include <url.h>
#include <parser.h>

Parser::Parser() {
}

Parser::~Parser() {
	if(m_pages.size()) {
		for(unsigned int i = 0; i < m_pages.size(); i++)
			delete m_pages[i];
	}
}
