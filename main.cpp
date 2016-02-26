#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "uuid.h"

#include "application.h"

#define EXPECTED_PACKET_SIZE 99

// Service Constants
IPAddress upnp_address( 239, 255, 255, 250 );
int upnp_port = 1900;
int web_port = 49153;
char device_name[64] = "living room light";

// Templates
const std::string upnp_search = "M-SEARCH";
const std::string wemo_search = "ST: urn:Belkin:device:**";
const std::string wemo_reply_template  =
  "HTTP/1.1 200 OK\n"
  "CACHE-CONTROL: max-age=86400\n"
  "DATE: {{TIMESTAMP}}\n"
  "EXT:\n"
  "LOCATION: http://{{IP_ADDRESS}}:49153/setup.xml\n"
  "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\n"
  "01-NLS: {{UUID}}\n"
  "SERVER: Unspecified, UPnP/1.0, Unspecified\n"
  "X-User-Agent: redsonic\n"
  "ST: urn:Belkin:device:**\n"
  "USN: uuid:Socket-1_0-{{SERIAL_NUMBER}}::urn:Belkin:device:**\n"
  "\n\n";

// Support Constants
static char HEX_DIGITS[] = "0123456789abcdef";

IPAddress ip_address;
std::string device_uuid;
std::string device_serial;

UDP udp;
TCPServer server = TCPServer(web_port);

// Kudos: http://stackoverflow.com/a/27658515
std::string replace_all(
  const std::string& str,
  const std::string& find,
  const std::string& replace
) {
  using namespace std;
  std::string result;
  size_t find_len = find.size();
  size_t pos,from=0;
  while (std::string::npos != (pos=str.find(find,from))) {
    result.append(str, from, pos-from);
    result.append(replace);
    from = pos + find_len;
  }
  result.append(str, from, std::string::npos);
  return result;
}

bool isMulticastSearch() {
  int byte_count = udp.parsePacket();

  if ( byte_count > 0 ) {
    // Read to buffer (defensively and stuff)
    Serial.println("Reading data...");
    char buffer[EXPECTED_PACKET_SIZE + 1];
    int read_amount = (EXPECTED_PACKET_SIZE < byte_count)? byte_count : EXPECTED_PACKET_SIZE;
    udp.read(buffer, read_amount);
    buffer[read_amount] = 0;

    // Cast to String
    const std::string data(buffer);
    Serial.println(data.c_str());

    // Find the stuff we care about
    if (data.find(upnp_search) != std::string::npos &&
        data.find(wemo_search) != std::string::npos) {
      return true;
    }
  }

  return false;
}

void sendSearchReply() {
  Serial.println("Sending reply to successful search...");
  udp.beginPacket(upnp_address, upnp_port);

  char ip_string[24];
  sprintf(ip_string, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

  std::string wemo_reply;
  wemo_reply = replace_all(wemo_reply_template, "{{TIMESTAMP}}", "Mon, 22 Jun 2015 17:24:01 GMT");
  wemo_reply = replace_all(wemo_reply, "{{IP_ADDRESS}}", ip_string);
  wemo_reply = replace_all(wemo_reply, "{{UUID}}", device_uuid);
  wemo_reply = replace_all(wemo_reply, "{{SERIAL_NUMBER}}", device_serial);

  udp.write(wemo_reply.c_str());
  udp.endPacket();
}

void toUnsignedString(char dest[], int offset, int len, long i, int shift) {
  int charPos = len;
  int radix = 1 << shift;
  long mask = radix - 1;
  do {
    dest[offset + --charPos] = HEX_DIGITS[(int) (i & mask)];
    i = i >> shift;
  } while (i != 0 && charPos > 0);
}

void hexDigits(char dest[], int offset, int digits, long val) {
  long hi = 1L << (digits * 4);
  toUnsignedString(dest, offset, digits, hi | (val & (hi - 1)), 4);
}

std::string uuidToString(std::pair<uint64_t, uint64_t>uuid_pair) {
  char uuid_string[37];

  hexDigits(uuid_string, 0, 8, uuid_pair.first >> 32);
  uuid_string[8] = 45;
  hexDigits(uuid_string, 9, 4, uuid_pair.first >> 16);
  uuid_string[13] = 45;
  hexDigits(uuid_string, 14, 4, uuid_pair.first);
  uuid_string[18] = 45;
  hexDigits(uuid_string, 19, 4, uuid_pair.second >> 48);
  uuid_string[23] = 45;
  hexDigits(uuid_string, 24, 4, uuid_pair.second >> 44);
  hexDigits(uuid_string, 28, 4, uuid_pair.second >> 28);
  hexDigits(uuid_string, 32, 4, uuid_pair.second >> 12);

  uuid_string[36] = 0;
  return std::string(uuid_string);
}

// std::string join(const std::vector<std::string>& vec, const char* delim) {
//   std::stringstream res;
//   copy(vec.begin(), vec.end(), std::ostream_iterator<std::string>(res, delim));
//   return res.str();
// }

std::string getDeviceSerial() {
  byte mac[6];
  WiFi.macAddress(mac);
  std::stringstream ss;
  ss << "c0"; // prefix
  for(int i = 0; i < 6; ++i) ss << std::hex << (int) mac[i];
  return ss.str();
}

std::string getDeviceUUID() {
  byte mac[6];
  WiFi.macAddress(mac);
  uint64_t host = ((uint64_t) mac[5] << 32) + (mac[4] << 24) + (mac[3] << 16) + (mac[2] << 8) + mac[1];
  uuid::Uuid uuid = uuid::uuid1(host, (uint16_t) mac[0]);
  return uuidToString(uuid.integer());
}

void setup() {
  Serial.begin(9600); // open serial over USB
  Serial.println("Starting up...");

  // Generate device values
  device_uuid = getDeviceUUID();
  device_serial = getDeviceSerial();

  waitUntil(WiFi.ready);
  Serial.print("Local IP: ");
  ip_address = WiFi.localIP();
  Serial.println(ip_address);
  Serial.println("Setting up UDP");

  // Start UDP
  udp.begin(upnp_port);
  udp.joinMulticast(upnp_address);

  // Start TCP
  server.begin();
}

void loop() {
  bool send_reply = isMulticastSearch();
  udp.flush();
  if (send_reply) sendSearchReply();

  TCPClient client = server.available();
  if (client.connected()) {
    while (client.available()) {
      char c = client.read();
      server.write(c);
      Serial.print("message recieved:");
      Serial.println(c);
    }
  }
}
