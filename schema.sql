-- drop database test_url;-- 
-- CREATE DB IF NOT EXISTS

-- ---------------------------------------------------

-- Table structure for 'users'

-- Stores authenticated users via Google OAuth

-- -----------------------------------------------------
;
CREATE TABLE IF NOT EXISTS users (id INT UNSIGNED NOT NULL AUTO_INCREMENT,google_id VARCHAR(255) NOT NULL COMMENT 'Unique ID provided by Google',email VARCHAR(255) NOT NULL COMMENT 'User email, used for login identification',name VARCHAR(255) NOT NULL,created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,PRIMARY KEY (id),UNIQUE INDEX ux_google_id (google_id),UNIQUE INDEX ux_email (email)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;





-- -----------------------------------------------------

-- Table structure for 'sessions'

-- Manages 1-day session tokens for authenticated users

-- -----------------------------------------------------
;
CREATE TABLE IF NOT EXISTS sessions (id INT UNSIGNED NOT NULL AUTO_INCREMENT,user_id INT UNSIGNED NOT NULL COMMENT 'Foreign key linking to the users table',session_token VARCHAR(255) NOT NULL COMMENT 'The secure token passed to the frontend',expires_at DATETIME NOT NULL COMMENT 'Timestamp when the session becomes invalid (1 day)',created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,PRIMARY KEY (id),UNIQUE INDEX ux_session_token (session_token),CONSTRAINT fk_sessions_user_id FOREIGN KEY (user_id)REFERENCES users (id)ON DELETE CASCADE) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -----------------------------------------------------

-- Table structure for 'shortened_links'

-- Core table storing all shortened URLs and their properties

-- -----------------------------------------------------
;
CREATE TABLE IF NOT EXISTS shortened_links (id INT UNSIGNED NOT NULL AUTO_INCREMENT,original_url TEXT NOT NULL COMMENT 'The full URL to redirect to',short_code VARCHAR(10) NOT NULL COMMENT 'The unique, short identifier (e.g., 6 characters)',user_id INT UNSIGNED NULL COMMENT 'ID of the authenticated creator (NULL if created by guest)',guest_identifier VARCHAR(255) NULL COMMENT 'IP or fingerprint for unauthenticated users',expires_at DATETIME NULL COMMENT 'Optional expiry date/time after which the link fails', is_favourite BOOLEAN DEFAULT FALSE, clicks INT UNSIGNED NOT NULL DEFAULT 0,created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,PRIMARY KEY (id),UNIQUE INDEX ux_short_code (short_code),CONSTRAINT fk_links_user_id FOREIGN KEY (user_id)REFERENCES users (id)ON DELETE SET NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;



-- -----------------------------------------------------

-- Table structure for 'guest_daily_quotas'

-- Tracks the 5-link free trial limit for unauthenticated users

-- -----------------------------------------------------
;
CREATE TABLE IF NOT EXISTS guest_daily_quotas (id INT UNSIGNED NOT NULL AUTO_INCREMENT,guest_identifier VARCHAR(255) NOT NULL COMMENT 'IP address or fingerprint of the guest user',quota_date DATE NOT NULL COMMENT 'The specific date for the quota tracking',links_created INT NOT NULL DEFAULT 1 COMMENT 'Count of links created by this guest on this date',created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,PRIMARY KEY (id),UNIQUE INDEX ux_guest_date (guest_identifier, quota_date)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- -----------------------------------------------------

-- Table structure for 'global_settings'

-- Stores application-wide settings, like the global limit toggle

-- -----------------------------------------------------
;
CREATE TABLE IF NOT EXISTS global_settings (id INT UNSIGNED NOT NULL AUTO_INCREMENT,setting_key VARCHAR(50) NOT NULL COMMENT 'The unique name of the configuration option',setting_value VARCHAR(255) NOT NULL COMMENT 'The value of the setting (e.g., true/false)',description VARCHAR(255) NULL,created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,PRIMARY KEY (id),UNIQUE INDEX ux_setting_key (setting_key)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- -----------------------------------------------------

-- Initial Data Insertion for Global Toggle

-- Sets the default state of the maximum link limit

-- -----------------------------------------------------
;
INSERT INTO global_settings (setting_key, setting_value, description)VALUES ('MAX_LINK_LIMIT_ENABLED','true','If true, unauthenticated users are limited to 5 links per day. Set to false for unlimited guest links.')ON DUPLICATE KEY UPDATE setting_value=setting_value;
;
INSERT INTO global_settings (setting_key, setting_value, description)VALUES ('MAX_GUEST_LINKS_PER_DAY','100000','If true, unauthenticated users are limited to {count} links per day. Set to false for unlimited guest links.')ON DUPLICATE KEY UPDATE setting_value=setting_value;

-- -----------------------------------------------------

-- Initial Data Insertion for Global Toggle

-- Table to have some stats

-- -----------------------------------------------------

-- Create the table only if it doesn't exist
;
CREATE TABLE IF NOT EXISTS endpoint_stats ( id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY, endpoint VARCHAR(255) NOT NULL, count BIGINT NOT NULL, endpoint_type VARCHAR(10) NOT NULL DEFAULT "GET", type ENUM('SYSTEM', 'USER') NOT NULL DEFAULT 'SYSTEM', created_by VARCHAR(45) NOT NULL DEFAULT "SYSTEM", created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, UNIQUE KEY unique_endpoint (endpoint, endpoint_type));

-- Insert rows only if they don't already exist (unique combination endpoint+endpoint_type)
;
INSERT INTO endpoint_stats (endpoint, count, endpoint_type)VALUES ('/shorten', 0, 'POST'),('/(\\w+)', 0, 'GET'),('/api/links', 0, 'GET'),('/api/link/favorite', 0, 'POST'),('/api/link', 0, 'DELETE'),('/api/admin', 0, 'GET'),('/auth/google/callback', 0, 'GET') ON DUPLICATE KEY UPDATE count=count; -- does nothing, avoids duplicate error


-- Insert rows only if they don't already exist (unique combination endpoint+endpoint_type)
;
INSERT INTO endpoint_stats (endpoint, count, endpoint_type) VALUES ('/shorten', 0, 'POST'),('/(\\w+)', 0, 'GET'),('/api/links', 0, 'GET'),('/api/link/favorite', 0, 'POST'),('/api/link', 0, 'DELETE'),('/api/admin', 0, 'GET'),('/auth/google/callback', 0, 'GET') ON DUPLICATE KEY UPDATE count=count; -- does nothing, avoids duplicate error

;
ALTER TABLE endpoint_stats ADD UNIQUE KEY unique_endpoint_ (endpoint, endpoint_type);
