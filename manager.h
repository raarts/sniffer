#ifndef MANAGER_H
#define MANAGER_H

#include "config.h"
#include <string>
#include <vector>
#include <queue>

#ifndef HAVE_LIBSSH
typedef void* ssh_channel;
#endif

void *manager_client(void *dummy);
void *manager_server(void *dummy);
void *manager_ssh(void *dummy);

class ManagerClientThread {
public:
	ManagerClientThread(int client, const char *type, const char *command, int commandLength = 0);
	virtual ~ManagerClientThread() {}
	void run();
	bool isFinished() { return(finished); }
	virtual bool parseCommand() = 0;
	virtual void onCall(int sipResponseNum, const char *callerName, const char *callerNum, const char *calledNum,
			    unsigned int sipSaddr, unsigned int sipDaddr) {}
protected:
	void lock_responses() {
		while(ATOMIC_TEST_AND_SET(&this->_sync_responses, 1));
	}
	void unlock_responses() {
		ATOMIC_CLEAR(&this->_sync_responses);
	}
protected:
	int client;
	string type;
	string command;
	bool finished;
	std::queue<string> responses;
private:
	volatile int _sync_responses;
};

class ManagerClientThread_screen_popup : public ManagerClientThread {
public:
	struct RegexReplace {
		RegexReplace(const char *pattern, const char *replace) {
			this->pattern = pattern;
			this->replace = replace;
		}
		string pattern;
		string replace;
	};
public:
	ManagerClientThread_screen_popup(int client, const char *command, int commandLength = 0);
	bool parseCommand();
	void onCall(int sipResponseNum, const char *callerName, const char *callerNum, const char *calledNum,
		    unsigned int sipSaddr, unsigned int sipDaddr);
private:
	bool parseUserPassword();
	bool isNumericId(const char *id);
private:
	string username;
	string name;
	string dest_number;
	bool allow_change_settings;
	string profile_name;
	bool auto_popup;
	bool show_ip;
	string popup_on;
	bool non_numeric_caller_id;
	vector<RegexReplace> regex_replace_calling_number;
	vector<string> regex_check_calling_number;
	ListIP_wb src_ip;
	string app_launch;
	string app_launch_args_or_url;
	string popup_title;
};

class ManagerClientThreads {
public:
	ManagerClientThreads();
	void add(ManagerClientThread *clientThread);
	void onCall(int sipResponseNum, const char *callerName, const char *callerNum, const char *calledNum,
		    unsigned int sipSaddr, unsigned int sipDaddr);
	void cleanup();
	int getCount();
private:
	void lock_client_threads() {
		while(ATOMIC_TEST_AND_SET(&this->_sync_client_threads, 1));
	}
	void unlock_client_threads() {
		ATOMIC_CLEAR(&this->_sync_client_threads);
	}
private: 
	std::vector<ManagerClientThread*> clientThreads;
	volatile int _sync_client_threads;
};

#endif
