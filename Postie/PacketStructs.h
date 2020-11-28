#pragma once

#include <inttypes.h>

struct ftn_addr_t {
	uint16_t zone;
	uint16_t net;
	uint16_t node;
	uint16_t point;
};

#pragma pack(push, 1)
struct packet_t {
	uint16_t orignode;
	uint16_t destnode;
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint16_t baud;
	uint16_t version;
	uint16_t origNet;
	uint16_t destNet;
	uint8_t prodCode;
	uint8_t prodVersionMajor;
	char password[8];
	uint16_t origZone;
	uint16_t destZone;
	uint16_t auxNet;
	uint16_t capValid;
	uint8_t prodCodeHi;
	uint8_t prodVersionMinor;
	uint16_t capWord;
	uint16_t origZone2;
	uint16_t destZone2;
	uint16_t origPoint;
	uint16_t destPoint;
	uint32_t prodData;
};

struct packed_message_t {
	uint16_t message_type;
	uint16_t orig_node;
	uint16_t dest_node;
	uint16_t orig_net;
	uint16_t dest_net;
	uint16_t attribute;
	uint16_t cost;
};

#pragma pack(pop)

// Private message
#define PKT_MSGPRIVATE 0x0001
// High priority
#define PKT_MSGCRASH 0x0002
// Read by addressee
#define PKT_MSGREAD 0x0004
// Has been sent
#define PKT_MSGSENT 0x0008
// File attached to msg
#define PKT_MSGFILE 0x0010
// In transit
#define PKT_MSGTRANSIT 0x0020
// Unknown node
#define PKT_MSGORPHAN 0x0040
// Kill after mailing
#define PKT_MSGKILL 0x0080
// Message was entered here
#define PKT_MSGLOCAL 0x0100
// Hold for pickup
#define PKT_MSGHOLD 0x0200
// Unused
#define PKT_MSGUNUSED 0x0400
// File req uest
#define PKT_MSGFREQ 0x0800
// Return receipt request
#define PKT_MSGRRREQ 0x1000
// Is return receipt
#define PKT_MSGISRR 0x2000
// Audit request
#define PKT_MSGAREQ 0x4000
// File update request
#define PKT_MSGFUPDREQ 0x8000
