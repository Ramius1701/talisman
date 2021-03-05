
#ifndef _MSC_VER
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include "SshClient.h"


SshClient::SshClient()
{
	chan = NULL;
}


SshClient::~SshClient()
{
}


static int ssh_copy_fd_to_chan(socket_t fd, int revents, void* userdata) {
	ssh_channel chan = (ssh_channel)userdata;
	char buf[2048];
	int sz = 0;

	if (!chan) {
#ifdef _MSC_VER
		closesocket(fd);
#else
		close(fd);
#endif
		return -1;
	}
	if (revents & POLLIN) {
		sz = recv(fd, buf, 2048, 0);
		if (sz > 0) {
			ssh_channel_write(chan, buf, sz);
		}
		else {
			ssh_channel_close(chan);
			sz = -1;
		}
	}
	if (revents & POLLHUP) {
		ssh_channel_close(chan);
		sz = -1;
	}
	return sz;
}

static int ssh_copy_chan_to_fd(ssh_session session,
	ssh_channel channel,
	void* data,
	uint32_t len,
	int is_stderr,
	void* userdata) {
	SshClient* sshc = (SshClient *)userdata;
	int sz;
	(void)session;
	(void)channel;
	(void)is_stderr;

	sz = send(sshc->rsock, (const char *)data, len, 0);
	return sz;
}

static void ssh_chan_close(ssh_session session, ssh_channel channel, void* userdata) {
	SshClient* sshc = (SshClient*)userdata;
	(void)session;
	(void)channel;
#ifdef _MSC_VER
	closesocket(sshc->rsock);
#else
	close(sshc->rsock);
#endif
}

void SshClient::run(int rsock) {
	this->rsock = rsock;
	do_run();

	if (rsock != -1) {
#ifdef _MSC_VER
		closesocket(rsock);
#else
		close(rsock);
#endif
	}
	ssh_disconnect(p_ssh_session);
	ssh_free(p_ssh_session);
#ifdef _MSC_VER
	closesocket(csock);
#else
	close(csock);
#endif
	delete this;
}

bool SshClient::do_auth() {
	ssh_message msg;
	struct sockaddr_in serv_addr;

	if (ssh_handle_key_exchange(p_ssh_session)) {
		return false;
	}
	bool gotauth = false;
	while (!gotauth) {
		msg = ssh_message_get(p_ssh_session);
		if (msg == NULL) {
			printf("Message = NULL\n");
			return false;
		}

		switch (ssh_message_type(msg)) {
		case SSH_REQUEST_AUTH:
			switch (ssh_message_subtype(msg)) {
			case SSH_AUTH_METHOD_PASSWORD:
				username = std::string(ssh_message_auth_user(msg));
				password = std::string(ssh_message_auth_password(msg));
				gotauth = true;
				ssh_message_auth_reply_success(msg, 0);
				break;
			case SSH_AUTH_METHOD_NONE:
			default:
				ssh_message_auth_set_methods(msg, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE);
				ssh_message_reply_default(msg);
				break;
			}
			break;
		default:
			ssh_message_auth_set_methods(msg, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE);
			ssh_message_reply_default(msg);
			break;
		}
		ssh_message_free(msg);
	}


	do {
		msg = ssh_message_get(p_ssh_session);
		if (msg) {
			if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL_OPEN && ssh_message_subtype(msg) == SSH_CHANNEL_SESSION) {

				chan = ssh_message_channel_request_open_reply_accept(msg);
				ssh_message_free(msg);
			}
			else {

				ssh_message_reply_default(msg);
				ssh_message_free(msg);
			}
		}
		else {
			printf("MSG == NULL\n");
			return false;
		}
	} while (!chan);
	while (true) {
		msg = ssh_message_get(p_ssh_session);
		if (msg) {
			if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL) {
				if (ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_SHELL) {
					ssh_message_channel_request_reply_success(msg);
					ssh_message_free(msg);
					break;
				}
				else if (ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_PTY) {
					term_width = ssh_message_channel_request_pty_width(msg);
					term_height = ssh_message_channel_request_pty_height(msg);
					ssh_message_channel_request_reply_success(msg);
					ssh_message_free(msg);
					continue;
				}
			}
		}
		else {
			return false;
		}
	}
	return true;
}

void SshClient::do_run() {
	if (rsock == -1) {
		return;
	}

	memset(&ssh_cb, 0, sizeof(struct ssh_channel_callbacks_struct));

	ssh_cb.channel_data_function = ssh_copy_chan_to_fd;
	ssh_cb.channel_eof_function = ssh_chan_close;
	ssh_cb.channel_close_function = ssh_chan_close;
	ssh_cb.userdata = this;

	ssh_callbacks_init(&ssh_cb);
	ssh_set_channel_callbacks(chan, &ssh_cb);

	short events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;

	ssh_event ev = ssh_event_new();
	if (ev == NULL) {
		return;
	}
	if (ssh_event_add_fd(ev, rsock, events, ssh_copy_fd_to_chan, chan) != SSH_OK) {
		return;
	}
	if (ssh_event_add_session(ev, p_ssh_session) != SSH_OK) {
		return;
	}

	do {
		ssh_event_dopoll(ev, 1000);
	} while (!ssh_channel_is_closed(chan));

	ssh_event_remove_fd(ev, rsock);

	ssh_event_remove_session(ev, p_ssh_session);

	ssh_event_free(ev);

#ifdef _MSC_VER
	closesocket(rsock);
#else
	close(rsock);
#endif
}
