#pragma once

#include <vector>
#include <string>

class Node;

class Editor
{
public:
	static std::vector<std::string> enter_message(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer) {
		return enter_message(n, to, subject, area, priv, quotebuffer, nullptr);
	}
	static std::vector<std::string> enter_message(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer, std::vector<std::string>* body);
private:
	static std::vector<std::string> enter_message_ex(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer);
	static std::vector<std::string> enter_message_in(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer, std::vector<std::string>* body);
};

