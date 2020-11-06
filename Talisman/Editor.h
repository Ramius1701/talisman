#pragma once

#include <vector>
#include <string>

class Node;

class Editor
{
public:
	static std::vector<std::string> enter_message(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer);
private:
	static std::vector<std::string> enter_message_ex(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer);
	static std::vector<std::string> enter_message_in(Node* n, std::string to, std::string subject, std::string area, bool priv, std::vector<std::string>* quotebuffer);
};

