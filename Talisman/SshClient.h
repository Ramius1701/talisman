#pragma once

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>
#include <string>
class SshClient
{
public:
	struct ssh_channel_callbacks_struct ssh_cb;

	ssh_session p_ssh_session;
	int csock;
	int rsock;

	std::string username;
	std::string password;

	int term_width;
	int term_height;

	void run(int rsock);
	SshClient();
	~SshClient();
	bool do_auth();
private:
	void do_run();
	ssh_channel chan;
};

