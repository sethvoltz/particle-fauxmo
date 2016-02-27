#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include "uuid.h"

#include "application.h"

#define ENABLE_DEBUG 1
#define TO_STRING(x) static_cast< std::ostringstream & >(( std::ostringstream() << std::dec << x )).str()
#define WEB_EXPECTED_REQUEST_SIZE 1024

// Config defaults and sizes
#define DEVICE_NAME "unknown device"
#define DEVICE_NAME_SIZE 64


// ----------------------------------------------------------- Service Constants
IPAddress upnp_address( 239, 255, 255, 250 );
int upnp_port = 1900;
int web_port = 49153;


// ------------------------------------------------------------------- Templates
const std::string upnp_search = "M-SEARCH";
const std::string wemo_search = "ST: urn:Belkin:device:**";
const std::string wemo_reply_template =
  "HTTP/1.1 200 OK\r\n"
  "CACHE-CONTROL: max-age=86400\r\n"
  "DATE: {{TIMESTAMP}}\r\n"
  "EXT:\r\n"
  "LOCATION: http://{{IP_ADDRESS}}:{{WEB_PORT}}/setup.xml\r\n"
  "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
  "01-NLS: {{UUID}}\r\n"
  "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
  "X-User-Agent: redsonic\r\n"
  "ST: urn:Belkin:device:**\r\n"
  "USN: uuid:Socket-1_0-{{SERIAL_NUMBER}}::urn:Belkin:device:**\r\n"
  "\r\n";
const std::string wemo_notify_template =
  "NOTIFY * HTTP/1.1\r\n"
  "HOST: 239.255.255.250:1900\r\n"
  "CACHE-CONTROL: max-age = 1800\r\n"
  "LOCATION: http://{{IP_ADDRESS}}:{{WEB_PORT}}/setup.xml\r\n"
  "NT: upnp:rootdevice\r\n"
  "NTS: ssdp:alive\r\n"
  "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
  "USN: uuid:Socket-1_0-{{SERIAL_NUMBER}}::urn:Belkin:device:**\r\n"
  "\r\n";

const std::string setup_request = "GET /setup.xml HTTP/1.1";
const std::string control_request = "SOAPACTION: \"urn:Belkin:service:basicevent:1#SetBinaryState\"";
const std::string turn_on_state = "<BinaryState>1</BinaryState>";
const std::string setup_header_template =
  "HTTP/1.1 200 OK\r\n"
  "CONTENT-LENGTH: {{CONTENT_LENGTH}}\r\n"
  "CONTENT-TYPE: text/xml\r\n"
  "DATE: {{TIMESTAMP}}\r\n"
  "LAST-MODIFIED: Sat, 01 Jan 2000 00:01:15 GMT\r\n"
  "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
  "X-User-Agent: redsonic\r\n"
  "CONNECTION: close\r\n"
  "\r\n"
  "{{XML_RESPONSE}}";
const std::string setup_xml_template =
  "<?xml version=\"1.0\"?>\r\n"
  "<root>\r\n"
  "  <device>\r\n"
  "    <deviceType>urn:DesigngodsNet:device:controllee:1</deviceType>\r\n"
  "    <friendlyName>{{DEVICE_NAME}}</friendlyName>\r\n"
  "    <manufacturer>Belkin International Inc.</manufacturer>\r\n"
  "    <modelName>Emulated Socket</modelName>\r\n"
  "    <modelNumber>3.1415</modelNumber>\r\n"
  "    <UDN>uuid:Socket-1_0-{{SERIAL_NUMBER}}</UDN>\r\n"
  "  </device>\r\n"
  "</root>\r\n";

const std::string control_response_template =
  "HTTP/1.1 200 OK\r\n"
  "CONTENT-LENGTH: 295\r\n"
  "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
  "DATE: {{TIMESTAMP}}\r\n"
  "EXT:\r\n"
  "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
  "X-User-Agent: redsonic\r\n"
  "Content-length: 4"
  "\r\n"
  "OK\r\n";
const std::string four_oh_four =
  "HTTP/1.1 404 Not Found\r\n"
  "Content-type: text/html\r\n"
  "Content-length: 113\r\n"
  "\r\n"
  "<html><head><title>Not Found</title></head><body>\r\n"
  "Sorry, the object you requested was not found.\r\n"
  "</body><html>\r\n";

// Support Constants
static char HEX_DIGITS[] = "0123456789abcdef";


// ------------------------------------------------------------- Runtime Globals
IPAddress ip_address;
std::string device_uuid;
std::string device_serial;
int device_state = 0;

// Socket Servers
UDP udp;
TCPServer server = TCPServer(web_port);

// -------------------------------------------------------------- EEPROM Storage
#define CONFIG_VERSION "st1"
#define CONFIG_START 0

// storage data
struct ConfigStruct {
    char version[4];
    char device_name[DEVICE_NAME_SIZE];
} config = {
    CONFIG_VERSION,
    DEVICE_NAME
};

// Load configuration
void loadConfig() {
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2]) {
    for (unsigned int t = 0; t < sizeof(config); t++) {
      *((char*)&config + t) = EEPROM.read(CONFIG_START + t);
    }
  }
}

// Save configuration
void saveConfig() {
  for (unsigned int t = 0; t < sizeof(config); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&config + t));
}

// ---------------------------------------------------- Particle Cloud Functions
int call_setDeviceName(String name) {
    //update new value to eeprom
    name.toCharArray(config.device_name, DEVICE_NAME_SIZE);
    saveConfig();

    return 1;
}


// ------------------------------------------------------------ Helper Functions
void debug(std::string message) {
  if (ENABLE_DEBUG == 1) {
    Serial.println(message.c_str());
    Particle.publish("DEBUG", message.c_str());
  }
}

std::string getTimestamp() {
  return std::string(Time.format(Time.now(), "%a, %d %b %Y %H:%M:%S %Z"));
}

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


// ---------------------------------------------------- Device Control Functions
void turnDeviceOn() {
  debug("Turning controlled device ON");
  // turn the device on
  device_state = 1;
}

void turnDeviceOff() {
  debug("Turning controlled device OFF");
  // turn the device off
  device_state = 0;
}


// --------------------------------------------------------------- UPnP Handlers
void sendSearchReply() {
  debug("Sending UPnP Reply to multicast group");
  udp.beginPacket(upnp_address, upnp_port);

  char ip_string[24];
  sprintf(ip_string, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

  std::string wemo_reply;
  wemo_reply = replace_all(wemo_reply_template, "{{TIMESTAMP}}", getTimestamp());
  wemo_reply = replace_all(wemo_reply, "{{IP_ADDRESS}}", ip_string);
  wemo_reply = replace_all(wemo_reply, "{{WEB_PORT}}", TO_STRING(web_port));
  wemo_reply = replace_all(wemo_reply, "{{UUID}}", device_uuid);
  wemo_reply = replace_all(wemo_reply, "{{SERIAL_NUMBER}}", device_serial);

  udp.write(wemo_reply.c_str());
  udp.endPacket();
}

void handleMulticastRequest() {
  int byte_count = udp.parsePacket();
  bool send_reply = false;
  std::stringstream buffer;

  if ( byte_count > 0 ) {
    debug("Reading UPnP data from multicast group");

    char this_char;
    int newline_count = 0;
    int counter = 0;
    while (newline_count < 2 && counter < byte_count) {
      this_char = udp.read();
      Serial.print(this_char);
      buffer << this_char;

      if (this_char == '\r') continue;

      if (this_char == '\n') {
        newline_count++;
      } else {
        newline_count = 0;
      }
      counter++;
    }

    // Find the stuff we care about
    const std::string data = buffer.str();
    if (data.find(upnp_search) != std::string::npos &&
        data.find(wemo_search) != std::string::npos) {
      send_reply = true;
    }
  }

  udp.flush();
  if (send_reply) sendSearchReply();
}

void sendMulticastNotify() {
  debug("Sending UPnP Notify to multicast group");
  udp.beginPacket(upnp_address, upnp_port);

  char ip_string[24];
  sprintf(ip_string, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

  std::string wemo_notify;
  wemo_notify = replace_all(wemo_notify_template, "{{IP_ADDRESS}}", ip_string);
  wemo_notify = replace_all(wemo_notify, "{{WEB_PORT}}", TO_STRING(web_port));
  wemo_notify = replace_all(wemo_notify, "{{SERIAL_NUMBER}}", device_serial);

  udp.write(wemo_notify.c_str());
  udp.endPacket();
}

// --------------------------------------------------------------- HTTP Handlers
void handleWebRequest() {
  TCPClient client = server.available();
  if (!client.connected()) return;

  int counter = 0;
  std::stringstream buffer;

  // Now actually read this into a string, check for the path params for
  debug("Reading TCP data on HTTP control port");
  char this_char;
  while (client.available() && counter < WEB_EXPECTED_REQUEST_SIZE) {
    this_char = client.read();
    buffer << this_char;
    counter++;
  }

  std::string request = buffer.str();
  std::string response;

  if (request.find(setup_request) != std::string::npos) {
    debug("Sending XML setup document");
    // the config XML file and the control calls, then return the appropriate
    // template or control what needs to be controlled.
    std::string xml = replace_all(setup_xml_template, "{{DEVICE_NAME}}", std::string(config.device_name));
    xml = replace_all(xml, "{{SERIAL_NUMBER}}", device_serial);

    response = replace_all(setup_header_template, "{{TIMESTAMP}}", getTimestamp());
    response = replace_all(response, "{{CONTENT_LENGTH}}", TO_STRING(xml.length()));
    response = replace_all(response, "{{XML_RESPONSE}}", xml);
  } else if (request.find(control_request) != std::string::npos) {
    if (request.find(turn_on_state) != std::string::npos) {
      turnDeviceOn();
    } else {
      turnDeviceOff();
    }
    response = replace_all(control_response_template, "{{TIMESTAMP}}", getTimestamp());
  } else {
    debug("Sending 404 reponse for unknown request");
    response = four_oh_four;
  }

  server.write((unsigned char*) response.c_str(), response.length());
  client.flush();
  client.stop();
}


// ------------------------------------------------------------- Setup Functions
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
  uint64_t host = 0;
  for (int i = 0; i < 5; ++i) host += ((uint64_t) mac[i + 1] << (i * 8));
  uuid::Uuid uuid = uuid::uuid1(host, (uint16_t) mac[0]);
  return uuidToString(uuid.integer());
}

void setup() {
  Serial.begin(9600); // open serial over USB
  Serial.println("Starting up...");

  // Setup Particle Cloud
  Particle.variable("deviceState", device_state);
  Particle.variable("deviceName", config.device_name, STRING);
  Particle.function("deviceName", call_setDeviceName);

  //load config
  loadConfig();

  // Generate device values
  device_uuid = getDeviceUUID();
  device_serial = getDeviceSerial();

  // Wait for wireless to come online
  waitUntil(WiFi.ready);
  ip_address = WiFi.localIP();

  std::stringstream ss;
  char ip_string[24];
  sprintf(ip_string, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
  ss << "Local IP: " << ip_string << ", Device Name: " << config.device_name;
  debug(ss.str());

  // Start UDP
  udp.begin(upnp_port);
  udp.joinMulticast(upnp_address);

  // Start TCP
  server.begin();

  if (strcmp(config.device_name, DEVICE_NAME) != 0) {
    // Device name is not default. Announce self to the network
    sendMulticastNotify();
  }
}


// ------------------------------------------------------------- Main Event Loop
void loop() {
  handleMulticastRequest();
  handleWebRequest();
}
