
#include <page.h>

Page::Page() {
	m_content = 0;
	m_domain = 0;
	m_path = 0;
}

Page::~Page() {
	if(m_domain) {
		free(domain);
		domain =0;
	}

	if(m_path) {
		free(m_path);
		m_path = 0;
	}

	if(m_content) {
		free(m_content);
		m_content = 0;
	}
}
