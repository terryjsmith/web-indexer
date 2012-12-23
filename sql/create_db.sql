
CREATE DATABASE IF NOT EXISTS crawler;
USE crawler;

CREATE TABLE IF NOT EXISTS domain (
	domain_id SERIAL NOT NULL,
	domain_name VARCHAR(255) NOT NULL,
	last_access BIGINT(20) UNSIGNED NOT NULL,
	robots_last_access BIGINT(20) UNSIGNED NOT NULL,
	PRIMARY KEY(domain_id),
	INDEX(domain_name)
) ENGINE = INNODB;

CREATE TABLE IF NOT EXISTS robotstxt(
	domain_id BIGINT(20) UNSIGNED NOT NULL,
	rule TEXT NOT NULL,
	INDEX(domain_id)
) ENGINE = INNODB;

CREATE TABLE IF NOT EXISTS url(
	url_id SERIAL NOT NULL,
	domain_id BIGINT(20) UNSIGNED NOT NULL,
	url TEXT NOT NULL,
	url_hash CHAR(32) NOT NULL,
	content_hash CHAR(32) NOT NULL,
	last_code SMALLINT(3) UNSIGNED NOT NULL,
	last_access BIGINT(20) NOT NULL,
	PRIMARY KEY(url_id),
	INDEX(domain_id),
	INDEX(url_hash),
	INDEX(content_hash)
) ENGINE = INNODB;

GRANT ALL ON crawler.* TO 'crawler'@'%' IDENTIFIED BY 'SpasWehabEp4';
FLUSH PRIVILEGES;
