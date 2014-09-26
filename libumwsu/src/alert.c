#include <libumwsu/scan.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <time.h>
#include <glib.h>
#include <assert.h>

struct alert {
  xmlDocPtr xml_doc;
};

static xmlNodePtr alert_doc_gdh_node(void)
{
  xmlNodePtr node;
  time_t t;
  struct tm l_tm;
  char buff[32];

  node = xmlNewNode(NULL, "gdh");

  time(&t);
  localtime_r(&t, &l_tm);

  /* format: "2001-12-31T12:00:00" */
  snprintf(buff, sizeof(buff) - 1, "%04d-%02d-%02dT%02d:%02d:%02d", 1900 + l_tm.tm_year, l_tm.tm_mon, l_tm.tm_mday, l_tm.tm_hour, l_tm.tm_min, l_tm.tm_sec);
  buff[sizeof(buff) - 1] = '\0';
  xmlAddChild(node, xmlNewText(buff));

  return node;
}

static void get_ip_addr(char *ip_addr)
{
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
    exit(EXIT_FAILURE);
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;

    if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) {
      int s;
      char mask[NI_MAXHOST];

      s = getnameinfo(ifa->ifa_netmask,
		      (ifa->ifa_addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
		      mask, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (s != 0) {
	printf("getnameinfo() failed: %s\n", gai_strerror(s));
	exit(EXIT_FAILURE);
      }

      if (strcmp(mask, "255.0.0.0") == 0)
	continue;

      s = getnameinfo(ifa->ifa_addr,
		      (ifa->ifa_addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
		      ip_addr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (s != 0) {
	printf("getnameinfo() failed: %s\n", gai_strerror(s));
	exit(EXIT_FAILURE);
      }

      return;
    }
  }

  freeifaddrs(ifaddr);
}

static xmlNodePtr alert_doc_identification_node(void)
{
  xmlNodePtr node;
  char hostname[HOST_NAME_MAX + 1];
  char ip_addr[NI_MAXHOST];

  node = xmlNewNode(NULL, "identification");

  xmlNewChild(node, NULL, "user", getenv("USER"));
  gethostname(hostname, HOST_NAME_MAX + 1);
  xmlNewChild(node, NULL, "hostname", hostname);
  get_ip_addr(ip_addr);
  xmlNewChild(node, NULL, "ip", ip_addr);
  xmlNewChild(node, NULL, "os", "Linux");

  return node;
}

static xmlDocPtr alert_doc_new(void)
{
  xmlDocPtr doc;
  xmlNodePtr root_node;

  LIBXML_TEST_VERSION;

  doc = xmlNewDoc("1.0");
  root_node = xmlNewNode(NULL, "alert");
  xmlNewProp(root_node, "xmlns", "http://www.davfi-project.org/AlertSchema");
  xmlNewProp(root_node, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  xmlNewProp(root_node, "xsi:schemaLocation", "http://www.davfi-project.org/AlertSchema AlertSchema.xsd ");
  xmlDocSetRootElement(doc, root_node);

  return doc;
}

static void alert_doc_add_alert(xmlDocPtr doc, struct umwsu_report *report)
{
  xmlNodePtr root_node, node;

  root_node = xmlDocGetRootElement(doc);
  xmlNewChild(root_node, NULL, "code", "a");
  xmlNewChild(root_node, NULL, "level", "2");

  node = xmlNewChild(root_node, NULL, "uri", report->path);
  xmlNewProp(node, "type", "path");

  xmlAddChild(root_node, alert_doc_gdh_node());

  xmlAddChild(root_node, alert_doc_identification_node());

  xmlNewChild(root_node, NULL, "module", report->mod_name);
  xmlNewChild(root_node, NULL, "module_specific", report->mod_report);
}

static void alert_doc_save(xmlDocPtr doc, const char *filename)
{
  xmlSaveFormatFileEnc(filename, doc, "UTF-8", 1);
}

static void alert_doc_save_to_fd(xmlDocPtr doc, int fd)
{
  xmlSaveCtxtPtr xmlCtxt = xmlSaveToFd(fd, "UTF-8", XML_SAVE_FORMAT);

  if (xmlCtxt != NULL) {
    xmlSaveDoc(xmlCtxt, doc);
    xmlSaveClose(xmlCtxt);
  }
}

static void alert_doc_save_to_buffer(xmlDocPtr doc, xmlBufferPtr *pxml_buf)
{
  xmlSaveCtxtPtr xmlCtxt;

  *pxml_buf = xmlBufferCreate();
  xmlCtxt = xmlSaveToBuffer(*pxml_buf, "UTF-8", XML_SAVE_FORMAT);

  if (xmlCtxt != NULL) {
    xmlSaveDoc(xmlCtxt, doc);
    xmlSaveClose(xmlCtxt);
  }
}

static void alert_doc_free(xmlDocPtr doc)
{
  xmlFreeDoc(doc);
}
 
static struct alert *alert_new()
{
  struct alert *a;

  a = (struct alert *)malloc(sizeof(struct alert));
  assert(a != NULL);

  a->xml_doc = alert_doc_new();

  return a;
}

static void alert_add(struct alert *a, struct umwsu_report *report)
{
  alert_doc_add_alert(a->xml_doc, report);
}

static int connect_socket(const char *path)
{
  int fd;
  struct sockaddr_un server_addr;

  fd = socket( AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket() failed");
    return -1;
  }

  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, path);

  if (connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
    close(fd);
    perror("connecting stream socket");
    return -1;
  }

  return fd;
}

#if 0
#define ALERT_SOCKET_PATH "/var/tmp/davfi_alert.s"

static void alert_send(struct alert *a)
{
  int fd;
				
  if (a->xml_doc == NULL)
    return;

#if 0
  alert_doc_save(a->xml_doc, "-");
#endif

  fd = connect_socket(ALERT_SOCKET_PATH);
  if (fd != -1) {
    alert_doc_save_to_fd(a->xml_doc, fd);
    close(fd);
  }
}
#endif

static size_t discard_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}

static void alert_send(struct alert *a)
{
  xmlBufferPtr xml_buf;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
  CURL *curl;
  CURLcode res;

  if (a->xml_doc == NULL)
    return;

  alert_doc_save_to_buffer(a->xml_doc, &xml_buf);

  curl_global_init(CURL_GLOBAL_ALL);

  curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "xml", CURLFORM_PTRCONTENTS, xmlBufferContent(xml_buf), CURLFORM_END);

  curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:10083/");
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libumwsu/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_data);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);

    curl_formfree(formpost);
  }

  xmlBufferFree(xml_buf);
}

static void alert_free(struct alert *a)
{
  alert_doc_free(a->xml_doc);

  free(a);
}

void alert_callback(struct umwsu_report *report, void *callback_data)
{
  struct alert *a;

  switch(report->status) {
  case UMWSU_CLEAN:
  case UMWSU_UNKNOWN_FILE_TYPE:
  case UMWSU_EINVAL:
  case UMWSU_IERROR:
  case UMWSU_UNDECIDED:
  case UMWSU_WHITE_LISTED:
    return;
  }

  a = alert_new();
  alert_add(a, report);
  alert_send(a);
  alert_free(a);
}

